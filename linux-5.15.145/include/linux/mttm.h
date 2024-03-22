#include <uapi/linux/perf_event.h>

#define KSAMPLED_CPU		0
//#define PEBS_SAMPLE_PERIOD	10007
#define LIMIT_TENANTS		10
#define CORES_PER_SOCKET	24
#define MTTM_PEBS_BUFFER_SIZE	32 /* # of pages.(32 -> 128KB) */
#define MAX_SAMPLE_RATIO	50
#define MIN_SAMPLE_RATIO	10
#define DRAM_LLC_LOAD_MISS	0x1d3
#define REMOTE_DRAM_LLC_LOAD_MISS	0x2d3

#define MTTM_INIT_THRESHOLD	1
#define MTTM_THRES_COOLING_ALLOC	(256 * 1024 * 10) // 10GB
#define MTTM_COOLING_PERIOD	200000
#define MTTM_ADJUST_PERIOD	10000

#define KMIGRATED_PERIOD_IN_MS	50

struct mttm_event {
	struct perf_event_header header;
	__u64 ip;
	__u32 pid, tid;
	__u64 addr;
};

enum eventtype {
	DRAMREAD = 0,
	CXLREAD = 1,
	NR_EVENTTYPE,
};

#define MTTM_MIN_FREE_PAGES	256 * 10 //10MB
extern void check_transhuge_cooling(void *arg, struct page *page);
extern struct page *get_meta_page(struct page *page);
extern void __prep_transhuge_page_for_mttm(struct mm_struct *mm, struct page *page);
extern void prep_transhuge_page_for_mttm(struct vm_area_struct *vma, struct page *page);
extern void copy_transhuge_pginfo(struct page *page, struct page *newpage);

extern bool node_is_toptier(int nid);
extern int set_page_coolstatus(struct page *page, pte_t *pte, struct mm_struct *mm);
extern void uncharge_mttm_pte(pte_t *pte, struct mem_cgroup *memcg);
extern void uncharge_mttm_page(struct page *page, struct mem_cgroup *memcg);
extern void check_base_cooling(pginfo_t *pginfo, struct page *page);

extern void move_page_to_active_lru(struct page *page);
extern void move_page_to_inactive_lru(struct page *page);
extern unsigned long get_memcg_demotion_wmark(unsigned long max_nr_pages);
extern unsigned long get_memcg_promotion_wmark(unsigned long max_nr_pages);

extern unsigned int get_idx(unsigned long num);
extern void mttm_mm_init(struct mm_struct *mm);
extern unsigned long get_nr_lru_pages_node(struct mem_cgroup *memcg, pg_data_t *pgdat);
extern int kmigrated_init(struct mem_cgroup *memcg);
extern void kmigrated_stop(struct mem_cgroup *memcg);
extern void kmigrated_wakeup(struct mem_cgroup *memcg);
