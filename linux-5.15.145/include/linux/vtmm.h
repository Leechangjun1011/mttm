#ifndef VTMM_H_
#define VTMM_H_

#include <linux/list.h>

#define ML_QUEUE_MAX	16
#define BUCKET_MAX	20


struct vtmm_page {
	/*
	- bitmap for read
	- bitmap for write
	- remained dnd time
	- list head
	*/
	__u64 read_count;
	__u64 write_count;
	unsigned int remained_dnd_time;
	bool is_thp;
	struct list_head list;
	__u64 addr;
};


void uncharge_vtmm_page(struct page *page, struct mem_cgroup *memcg);
void uncharge_vtmm_transhuge_page(struct page *page, struct mem_cgroup *memcg);
void prep_transhuge_page_for_vtmm(struct vm_area_struct *vma, struct page *page);
void __prep_transhuge_page_for_vtmm(struct mem_cgroup *memcg, struct page *page);


#endif
