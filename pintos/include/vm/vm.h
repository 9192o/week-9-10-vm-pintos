#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "lib/kernel/hash.h"

/* 페이지 유형. 페이지는 한 번에 하나의 상태만 가질 수 있습니다. */
enum vm_type
{
	/* 초기화 되지 않은 페이지 */
	VM_UNINIT = 0,
	/* page not related to the file (익명 페이지) */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* 저장 정보를 위한 보조 비트 플래그 마커입니다.
	 * int형 마커를 더 추가할 수 있습니다. */
	VM_MARKER_0 = (1 << 3), /* WOONG: 스택 표시용으로 정함. */
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE.
	 * 이 값을 초과하지 마십시오. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page
{
	const struct page_operations *operations;
	void *va;			 /* 사용자 공간 기준 주소 */
	struct frame *frame; /* 프레임에 대한 역참조 */

	/* Your implementation */
	struct hash_elem spt_page_elem; /* 해시 테이블에 들어갈 요소 */
	bool writable;

	/* 각 데이터 유형은 union에 바인딩됩니다.
	 * 각 함수는 현재 union을 자동으로 감지합니다. */
	/* 쉽게 말해, 시스템의 페이지는 한 번에 하나의 값만 가질 수 있습니다. */
	union
	{
		struct uninit_page uninit; /* 초기화되지 않은 페이지 */
		struct anon_page anon;	   /* 익명 페이지 */
		struct file_page file;	   /* 파일 페이지 */
#ifdef EFILESYS
		struct page_cache page_cache; /* 또는 페이지 캐시 (Project 4)*/
#endif
	};
};

/* The representation of "frame" */
struct frame
{
	void *kva;
	struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations
{
	bool (*swap_in)(struct page *, void *);
	bool (*swap_out)(struct page *);
	void (*destroy)(struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                \
	if ((page)->operations->destroy) \
	(page)->operations->destroy(page)

/* 현재 프로세스의 메모리 공간을 나타냅니다.
 * 이 구조체에 대해 특정 설계를 따르도록 강요하지는 않습니다.
 *
 * 어떻게 만들까는, 당신에게 달려있습니다. */
struct supplemental_page_table
{
	struct hash spt_pages;
};

#include "threads/thread.h"
void supplemental_page_table_init(struct supplemental_page_table *spt);

bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt, void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

#endif /* VM_VM_H */
