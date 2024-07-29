#ifndef VTMM_H_
#define VTMM_H_

#include <linux/list.h>

#define ML_QUEUE_MAX	8
#define BUCKET_MAX	20
#define BITMAP_MAX	20

struct vtmm_page {
	/*
	- bitmap for read
	- bitmap for write
	- remained dnd time
	- list head
	*/
	unsigned long read_count;
	unsigned long write_count;
	unsigned long va;
	unsigned int remained_dnd_time;
	unsigned int ml_queue_lev;
	bool skip_scan;
	bool is_thp;
	bool promoted;
	bool demoted;
	struct list_head list;//for page bucket
	__u64 addr;
};

void cmpxchg_vtmm_page(struct mem_cgroup *memcg, struct page *old_page, struct page *new_page);
struct vtmm_page *get_vtmm_page(struct mem_cgroup *memcg, struct page *page);
struct vtmm_page *erase_vtmm_page(struct mem_cgroup *memcg, struct page *page, int *lev);

void uncharge_vtmm_page(struct page *page, struct mem_cgroup *memcg);
void uncharge_vtmm_transhuge_page(struct page *page, struct mem_cgroup *memcg);
void prep_transhuge_page_for_vtmm(struct vm_area_struct *vma, struct page *page, unsigned long haddr);
void __prep_transhuge_page_for_vtmm(struct mem_cgroup *memcg, struct page *page, unsigned long haddr);
void copy_transhuge_vtmm_page(struct page *page, struct page *newpage);

int vtmm_kmigrated_init(struct mem_cgroup *memcg);
void vtmm_kmigrated_stop(struct mem_cgroup *memcg);
void vtmm_kmigrated_wakeup(struct mem_cgroup *memcg);

#endif
