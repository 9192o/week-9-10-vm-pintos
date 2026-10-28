#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

/* for Lazy Loading */
struct lazy_load_context
{
	struct file *file;
	size_t page_read_bytes;
	size_t page_zero_bytes;
	off_t page_ofs;
};

struct file_page
{
	struct file *file;		/* 파일 정보 */
	size_t file_read_bytes; /* 읽은 바이트 */
	off_t file_ofs;			/* 파일 오프셋 */
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
			  struct file *file, off_t offset);
void do_munmap(void *va);
#endif
