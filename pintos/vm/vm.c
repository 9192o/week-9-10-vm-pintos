/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* spt에서 VA를 찾고, 그 PAGE를 반환합니다. 오류 시 NULL을 반환합니다. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page lookup_page = {0} ; // 임시 페이지
	lookup_page.va = pg_round_down(va); // 정렬 맞춤 (페이지 시작 부분)

	struct hash_elem *elem = hash_find(&spt->pages, &lookup_page.hash_elem);

	if (elem == NULL)
		return NULL;
	
	// elem을 가지고 있는 실제 page의 주소를 복원해 반환
	return hash_entry(elem, struct page, hash_elem);
}

/* 검증과 함께 spt에 PAGE를 삽입합니다. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{	
	page->va = pg_round_down(page->va); // 정렬 맞춤 (페이지 시작 부분)

	// 삽입 시도하고 성공 여부 반환 (중복 여부도 여기서 확인해줌)
	return hash_insert(&spt->pages, &page->hash_elem) == NULL;
}

/* spt에서 PAGE를 삭제합니다. */
void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	void *new_kva = palloc_get_page(PAL_USER); // 유저 풀에서 page를 얻어옴

	if (new_kva == NULL) // 페이지 할당 실패하면 중단
		PANIC("todo");
	
	struct frame *new_frame = malloc(sizeof *new_frame); // 프레임 할당
	if (new_frame == NULL) { // 프레임 할당 실패하면 중단
        palloc_free_page(new_kva);
        PANIC("failed to allocate frame metadata");
    }

	new_frame->kva = new_kva; // 프레임에 kva 저장
    new_frame->page = NULL; // 연결된 페이지 없다고 표시(나중에 다른 곳에서 설정함)

	return new_frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	return swap_in(page, frame->kva);
}

/* 해시 함수 */
static uint64_t
hash_page(const struct hash_elem *elem, void *aux UNUSED)
{
	const struct page *page = hash_entry(elem, struct page, hash_elem);
	void *page_start = pg_round_down(page->va);
	// va 라운딩해서 넣어야 함
	return hash_bytes(&page_start, sizeof page_start);
}

/* 해시 비교 함수 */
static bool
hash_cmp(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
    const struct page *page_a = hash_entry(a, struct page, hash_elem);
    const struct page *page_b = hash_entry(b, struct page, hash_elem);

    return (uintptr_t) pg_round_down(page_a->va) < (uintptr_t) pg_round_down(page_b->va);
}
/* Initialize new supplemental page table */
// spt에 어떤 자료구조를 쓸 지는 마음대로 (list, hashmap, ...)
// 호출: 새 프로세스 시작(userprog.process.c 의 initd), 프로세스 fork(userprog/process.c의 __do_fork)
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	hash_init(&spt->pages, hash_page, hash_cmp, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
