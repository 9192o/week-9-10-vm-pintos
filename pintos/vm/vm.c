/* vm.c: Generic interface for virtual memory objects. */

#include <string.h>
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "userprog/process.h"

/* 해시 함수 */
static uint64_t
hash_func(const struct hash_elem *e, void *aux UNUSED);

/* 해시 비교 함수 */
static bool
hash_compare_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

/* 해시 삭제 함수 */
static void
hash_destroy_func(struct hash_elem *e, void *aux UNUSED);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */

	/* TODO: Your code goes here. */
}

/* 페이지의 유형을 가져옵니다.
 * 페이지가 초기화된 후의 유형을 알고싶을 때 유용합니다.
 * 완전히 구현된 함수입니다.*/
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* 보류 중인 페이지 객체를 생성하고 초기화합니다.
 * (정확히는, uninitial 된 페이지이며 인자로 받은 초기화자를 지정해줍니다.)
 * 페이지를 생성하려면 직접 생성하지 말고, 이 함수나 `vm_alloc_page`를 통해 생성하십시오. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &(thread_current()->spt);

	/* 사용자 페이지가 점유되어있는지 확인 */
	if (spt_find_page(spt, upage) != NULL)
		return false;

	/* 페이지 구조체 생성 */
	struct page *page = malloc(sizeof(struct page));
	if (page == NULL)
		return false;

	/* 초기화 */
	switch (VM_TYPE(type))
	{
	case VM_ANON:
		uninit_new(page, upage, init, type, aux, anon_initializer);
		break;
	case VM_FILE:
		uninit_new(page, upage, init, type, aux, file_backed_initializer);
		break;
	/* project 4를 위한 초기화 분기 */
	case VM_PAGE_CACHE:
	default:
		free(page);
		page = NULL;
		return false;
	}

	page->writable = writable;
	if (!spt_insert_page(spt, page))
	{
		free(page);
		page = NULL;
		return false;
	}

	return true;
}

/* spt에서 VA를 찾고, 그 PAGE를 반환합니다. 오류 시 NULL을 반환합니다. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page lookup_page; /* 검색용 페이지 */
	struct hash_elem *elem;
	void *pg_start; /* 검색 주소*/

	/* VA -> PAGE 시작 주소 변환 */
	pg_start = pg_round_down(va);

	/* 검색용 임시 페이지 생성 */
	lookup_page.va = pg_start;

	/* 인자로 전달받은 spt 내에서, VA -> PAGE 매핑을 찾아서 반환한다. */
	elem = hash_find(&(spt->spt_pages), &(lookup_page.spt_page_elem));
	if (elem == NULL)
		return NULL;

	return hash_entry(elem, struct page, spt_page_elem);
}

/* spt에 PAGE를 삽입합니다.
 * 페이지가 존재할 시, 기존 항목을 유지하고 false를 반환합니다. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{
	/* NULL = 성공 / 기존 페이지 반환 = 실패 */
	return hash_insert(&(spt->spt_pages), &(page->spt_page_elem)) == NULL;
}

/* spt에서 PAGE를 삭제합니다. */
void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
}

/* 희생할 struct frame을 반환합니다. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: 누굴 희생시킬지는 당신이 정하십시오. */

	return victim;
}

/* struct frame을 희생시키고, 그 희생자 frame을 반환합니다.
 * 오류 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()을 호출하여 프레임을 확보합니다.
 * 사용 가능한 페이지가 없다면, 해당 페이지를 희생시키고 반환합니다.
 *
 * 이 함수는 항상 유효한 주소를 반환합니다.
 * 즉, 사용자 풀 메모리가 가득 찬 경우, 이 함수는 사용 가능한 메모리 공간을 확보하기 위해 프레임을 제거합니다 */
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;

	/* TODO: 희생과 반환 */

	/* 프레임 관리 구조체 확보 */
	frame = malloc(sizeof(struct frame));
	if (frame == NULL)
		PANIC("todo");

	/* 사용자 물리 프레임 확보 */
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (frame->kva == NULL)
	{
		free(frame);
		PANIC("todo");
	}

	/* 연결된 페이지 없음 */
	frame->page = NULL;

	return frame;
}

/* 스택을 늘립니다. */
static void
vm_stack_growth(void *addr)
{
	vm_alloc_page((VM_ANON | VM_MARKER_0), pg_round_down(addr), true);
}

/* 쓰기 보호된 페이지에서 발생하는 오류를 처리합니다. */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* 폴트 처리 성공 시 true를 반환합니다. */
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present)
{
	struct supplemental_page_table *spt = &(thread_current()->spt);
	struct page *page = NULL;
	uintptr_t rsp;

	/* 주소가 NULL인가? */
	if (addr == NULL)
		return false;

	/* 커널을 참조했는가? */
	if (is_kernel_vaddr(addr))
		return false;

	/* 커널/사용자 측에서 폴트가 났나? */
	if (!user)
		rsp = thread_current()->user_rsp;
	else
		rsp = f->rsp;

	/* 매핑이 있는데 보호를 위반했나? */
	if (!not_present)
		return false;

	/* 페이지 검색 */
	page = spt_find_page(spt, addr);

	/* 페이지가 없는가? */
	if (page == NULL)
	{
		/* 스택 확장 조건을 만족하지 않으면 거부합니다. */
		if (!(USER_STACK - STACK_MAX_SIZE <= (uintptr_t)addr &&
			  USER_STACK > (uintptr_t)addr && (uintptr_t)addr >= rsp - 8))
			return false;

		vm_stack_growth(addr);
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;
	}

	/* 쓰기 금지인데 쓰는건가? */
	if (!(page->writable) && write)
		return false;

	/* 페이지 로딩 */
	return vm_do_claim_page(page);
}

/* PAGE를 free합니다.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* VA에 할당된 PAGE를 확보합니다. */
bool vm_claim_page(void *va)
{
	struct page *page = NULL;
	void *pg_start;

	/* 페이지 검색 */
	pg_start = pg_round_down(va);
	page = spt_find_page(&(thread_current()->spt), pg_start);
	if (page == NULL)
		return false;

	return vm_do_claim_page(page);
}

/* PAGE를 확보하고, MMU를 설정합니다. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* 양방향 연결 */
	frame->page = page;
	page->frame = frame;

	/* 프레임 매핑 */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
	{
		palloc_free_page(frame->kva);
		free(frame);
		page->frame = NULL;
		return false;
	}

	/* 페이지 내용을 프레임에 준비 */
	if (!swap_in(page, frame->kva))
	{
		pml4_clear_page(thread_current()->pml4, page->va);
		palloc_free_page(frame->kva);
		free(frame);
		page->frame = NULL;
		return false;
	}

	return true;
}

/* 새 spt(supplemental_page_table)을 initializing합니다. */
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	/* TODO: init 실패 시 처리 분기 */
	hash_init(&(spt->spt_pages), hash_func, hash_compare_func, NULL);
}

static uint64_t
hash_func(const struct hash_elem *e, void *aux UNUSED)
{
	struct page *page;
	void *pg_start;

	uint64_t hashed;

	page = hash_entry(e, struct page, spt_page_elem);

	/* 페이지 시작 주소 */
	pg_start = pg_round_down(page->va);

	/* 시작 주소 해싱 */
	hashed = hash_bytes(&pg_start, sizeof(pg_start));

	return hashed;
}

static bool
hash_compare_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	struct page *page_a;
	struct page *page_b;

	page_a = hash_entry(a, struct page, spt_page_elem);
	page_b = hash_entry(b, struct page, spt_page_elem);

	return (uintptr_t)(pg_round_down(page_a->va)) < (uintptr_t)(pg_round_down(page_b->va));
}

/* src인자로 받은 spt를 dst인자로 복사합니다.
 * 현재 이 함수를 수행하는 스레드는 "자식 스레드" 입니다. */
bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src)
{
	struct hash_iterator src_iter;

	/* iterator 설정 */
	hash_first(&src_iter, &src->spt_pages);

	/* 해시 테이블 순회 */
	while (hash_next(&src_iter) != NULL)
	{
		struct page *src_page;
		struct page *dst_page;

		/* 페이지 복원 */
		src_page = hash_entry(hash_cur(&src_iter), struct page, spt_page_elem);

		/* 페이지 복사 (미초기화된 부모 페이지 분기) */
		if (src_page->operations->type == VM_UNINIT)
		{
			struct lazy_load_context *parent_ctx;
			struct lazy_load_context *child_ctx;

			parent_ctx = (struct lazy_load_context *)src_page->uninit.aux;
			child_ctx = malloc(sizeof(struct lazy_load_context));
			if (child_ctx == NULL)
				return false;

			/* 자식의 Lazy Loading 컨텍스트 설정 */
			memcpy(child_ctx, parent_ctx, sizeof *child_ctx);

			/* 자식 기반 페이지 생성/spt 삽입 */
			if (!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va, src_page->writable, src_page->uninit.init, child_ctx))
			{
				free(child_ctx);
				return false;
			}

			/* Claim */
			if (!vm_claim_page(src_page->va))
				return false;
		}
		else
		{
			/* 페이지 복사 (메모리에 올라와있는 부모 페이지 분기) */
			if (!vm_alloc_page(page_get_type(src_page), src_page->va, src_page->writable))
				return false;

			/* Claim */
			if (!vm_claim_page(src_page->va))
				return false;

			/* 프레임 내용 복사 */
			dst_page = spt_find_page(dst, src_page->va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* spt가 점유하고 있는 리소스를 해제합니다. */
void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
	/* 해시 해제 */
	if (spt->spt_pages.buckets == NULL)
		return;
	hash_destroy(&(spt->spt_pages), hash_destroy_func);

	spt->spt_pages.buckets = NULL;
}

static void
hash_destroy_func(struct hash_elem *e, void *aux UNUSED)
{
	struct page *page = hash_entry(e, struct page, spt_page_elem);
	vm_dealloc_page(page);
}
