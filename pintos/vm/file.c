/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "userprog/syscall.h"
#include <string.h>
#include <round.h>

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

static bool file_load_segment(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page = &page->file;

	if (page->frame != NULL)
	{
		if (pml4_is_dirty(thread_current()->pml4, page->va))
		{
			off_t write_bytes = 0;

			lock_acquire(&filesys_lock);
			write_bytes = file_write_at(page->file.file, page->frame->kva, page->file.file_read_bytes, page->file.file_ofs);
			lock_release(&filesys_lock);

			/* 저장 실패시 오류 */
			ASSERT(write_bytes == page->file.file_read_bytes);
		}

		/* 사용자 매핑 제거 */
		pml4_clear_page(thread_current()->pml4, page->va);

		/* 물리 프레임 해제 */
		palloc_free_page(page->frame->kva);
		free(page->frame);
		page->frame = NULL;
	}
}

/* addr에 파일을 매핑합니다.
 * 성공 시 매핑된 주소를 반환하며, 실패하면 NULL을 반환합니다. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	uintptr_t mapped = (uintptr_t)addr;
	bool success = false;
	/* 주소가 없는가? 매핑할 길이가 0인가? 오프셋이 음수인가? */
	if (addr == NULL || !length || offset < 0)
		return NULL;

	/* 오프셋보다 파일 길이가 작은가? */
	lock_acquire(&filesys_lock);
	success = (file_length(file) > offset);
	lock_release(&filesys_lock);

	if (!success)
		return NULL;

	/* 파일 열기 */
	lock_acquire(&filesys_lock);
	file = file_reopen(file);
	lock_release(&filesys_lock);

	if (file == NULL)
		return NULL;

	/* 파일 페이지 매핑 */
	lock_acquire(&filesys_lock);
	size_t read_bytes = length < (file_length(file) - offset) ? length : (file_length(file) - offset);
	size_t zero_bytes = ROUND_UP(length, PGSIZE) - read_bytes;
	lock_release(&filesys_lock);

	/* 기록 구조체 생성 */
	struct mapped_file *mapped_file = malloc(sizeof *mapped_file);
	if (mapped_file == NULL)
		return NULL;

	/* 기록 후 연결 */
	mapped_file->file = file;
	mapped_file->start = (void *)mapped;
	mapped_file->pages_count = ROUND_UP(length, PGSIZE) / PGSIZE;

	while (read_bytes > 0 || zero_bytes > 0)
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 파일 로딩을 위한 컨텍스트 설정 */
		struct lazy_load_context *f_ctx = malloc(sizeof(struct lazy_load_context));
		if (f_ctx == NULL)
			return NULL;

		f_ctx->file = file;
		f_ctx->page_read_bytes = page_read_bytes;
		f_ctx->page_zero_bytes = page_zero_bytes;
		f_ctx->page_ofs = offset;

		/* 컨텍스트 전달과 페이지 초기화 */
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, file_load_segment, f_ctx))
		{
			free(f_ctx);
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;

		/* 파일 오프셋 갱신 */
		offset += page_read_bytes;
	}
	list_push_back(&(thread_current()->spt.mapped_files), &(mapped_file->mapped_files_elem));

	return (void *)mapped;
}

static bool
file_load_segment(struct page *page, void *aux)
{
	struct lazy_load_context *f_ctx = (struct lazy_load_context *)aux;
	bool success = false;
	/* 파일 읽기 */
	lock_acquire(&filesys_lock);
	success = (file_read_at(f_ctx->file, page->frame->kva, f_ctx->page_read_bytes, f_ctx->page_ofs) == f_ctx->page_read_bytes);
	lock_release(&filesys_lock);

	if (!success)
	{
		free(f_ctx);
		return false;
	}

	/* 정보 저장 */
	page->file.file = f_ctx->file;
	page->file.file_ofs = f_ctx->page_ofs;
	page->file.file_read_bytes = f_ctx->page_read_bytes;

	/* 나머지 부분은 0으로 채우기 */
	memset(page->frame->kva + f_ctx->page_read_bytes, 0, f_ctx->page_zero_bytes);

	/* 컨텍스트 구조체 해제 */
	free(f_ctx);
	return true;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	struct supplemental_page_table *spt = &(thread_current()->spt);
	struct list *mapped_files = &(spt->mapped_files);
	struct page *page;

	if (is_kernel_vaddr(addr))
		PANIC("Invalid Access");

	page = spt_find_page(spt, addr);
	if (page == NULL)
		return;

	/* 페이지 타입 별 언매핑 함수 호출 */
	switch (page_get_type(page))
	{
	case VM_ANON:
		/* TODO */
		break;
	case VM_FILE:
	{
		struct list_elem *e;
		struct mapped_file *mapped_file = NULL;

		/* 매핑된 파일 리스트 탐색 ㄴ*/
		for (e = list_begin(mapped_files); e != list_end(mapped_files); e = list_next(e))
		{
			mapped_file = list_entry(e, struct mapped_file, mapped_files_elem);
			if (mapped_file->start == addr)
				break;
		}

		/* 못 찾은 경우 */
		if (e == list_end(mapped_files))
			return;

		/* 연결된 파일 페이지 해제. */
		for (size_t i = 0; i < mapped_file->pages_count; i++)
		{
			page = spt_find_page(spt, (void *)((uintptr_t)(mapped_file->start) + PGSIZE * i));
			spt_remove_page(spt, page);
		}
		list_remove(e);

		lock_acquire(&filesys_lock);
		file_close(mapped_file->file);
		lock_release(&filesys_lock);

		free(mapped_file);
		break;
	}
	default:
		break;
	}
}
