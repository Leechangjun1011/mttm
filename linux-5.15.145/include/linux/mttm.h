#ifndef	_MTTM_H
#define	_MTTM_H

#include <uapi/linux/perf_event.h>

#define KSAMPLED_CPU		0
#define KPTSCAND_CPU		0
#define LIMIT_TENANTS		20
#define CORES_PER_SOCKET	24
#define REGION_DETERMINATION_COOLING	3
#define WORKLOAD_CLASSIFICATION_COOLING	3
#define SAMPLE_RATE_STABLE_CNT	10

#define MTTM_PEBS_BUFFER_SIZE	32 /* # of pages.(32 -> 128KB) */
#define MAX_SAMPLE_RATIO	50
#define MIN_SAMPLE_RATIO	10

#define DRAM_LLC_LOAD_MISS		0x1d3
#define REMOTE_DRAM_LLC_LOAD_MISS	0x2d3
#define ALL_STORES			0x82d0

#define MTTM_INIT_THRESHOLD	1
#define MTTM_THRES_COOLING_ALLOC	(256 * 1024 * 10) // 10GB
#define MTTM_INIT_COOLING_PERIOD	20000
#define MTTM_INIT_ADJUST_PERIOD		4000

#define NR_REGION		5
//#define KMIGRATED_PERIOD_IN_MS	50



struct mttm_event {
	struct perf_event_header header;
	__u64 ip;
	__u32 pid, tid;
	__u64 addr;
};

enum eventtype {
	DRAMREAD = 0,
	CXLREAD = 1,
	MEMSTORE = 2,
	NR_EVENTTYPE,
};

#define MTTM_MIN_FREE_PAGES	256 * 10 //10MB
unsigned long get_anon_rss(struct mem_cgroup *memcg);

extern void check_transhuge_cooling(void *arg, struct page *page);
extern void check_transhuge_cooling_reset(void *arg, struct page *page);
extern struct page *get_meta_page(struct page *page);
extern void __prep_transhuge_page_for_mttm(struct mm_struct *mm, struct page *page);
extern void prep_transhuge_page_for_mttm(struct vm_area_struct *vma, struct page *page);
extern void copy_transhuge_pginfo(struct page *page, struct page *newpage);

extern bool node_is_toptier(int nid);
extern int set_page_coolstatus(struct page *page, pte_t *pte, struct mm_struct *mm);
extern void uncharge_mttm_pte(pte_t *pte, struct mem_cgroup *memcg, struct page *page);
extern void uncharge_mttm_page(struct page *page, struct mem_cgroup *memcg);
extern void check_base_cooling(pginfo_t *pginfo, struct page *page);
extern void check_base_cooling_reset(pginfo_t *pginfo, struct page *page);

void set_lru_adjusting(struct mem_cgroup *memcg, bool inc_thres);
extern void move_page_to_active_lru(struct page *page);
extern void move_page_to_inactive_lru(struct page *page);
extern unsigned long get_memcg_demotion_wmark(unsigned long max_nr_pages);
extern unsigned long get_memcg_promotion_wmark(unsigned long max_nr_pages);
extern unsigned long get_memcg_promotion_expanded_wmark(unsigned long max_nr_pages);
extern bool need_direct_demotion(pg_data_t *pgdat, struct mem_cgroup *memcg);
extern unsigned long nr_promotion_target(pg_data_t *pgdat, struct mem_cgroup *memcg);
bool promotion_available(int nid, struct mem_cgroup *memcg, unsigned long *nr, bool expanded);

extern unsigned int get_idx(uint32_t num);
extern uint32_t *get_ac_pointer(struct mem_cgroup *memcg, unsigned int gi, unsigned int hi, unsigned int bi);
extern void mttm_mm_init(struct mm_struct *mm);
extern unsigned long get_nr_lru_pages_node(struct mem_cgroup *memcg, pg_data_t *pgdat);
extern int kmigrated_init(struct mem_cgroup *memcg);
extern void kmigrated_stop(struct mem_cgroup *memcg);
extern void kmigrated_wakeup(struct mem_cgroup *memcg);

#endif
