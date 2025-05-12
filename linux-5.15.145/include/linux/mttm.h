#ifndef	_MTTM_H
#define	_MTTM_H

#include <uapi/linux/perf_event.h>

#define KSAMPLED_CPU		0
#define KPTSCAND_CPU		0
#define LIMIT_TENANTS		20
#define CORES_PER_SOCKET	24
#define SAMPLE_RATE_STABLE_CNT	10
#define PEBS_INIT_PERIOD	4999
#define PEBS_STABLE_PERIOD	10007

#define MTTM_PEBS_BUFFER_SIZE	128 /* # of pages.(32 -> 128KB) */
#define MAX_SAMPLE_RATIO	50
#define MIN_SAMPLE_RATIO	10

#define DRAM_LLC_LOAD_MISS		0x1d3
#define REMOTE_DRAM_LLC_LOAD_MISS	0x2d3
#define ALL_STORES			0x82d0

#define MTTM_INIT_THRESHOLD	1
#define MTTM_INIT_COOLING_PERIOD	20000
#define MTTM_INIT_ADJUST_PERIOD		4000

#define NR_REGION		5
//#define KMIGRATED_PERIOD_IN_MS	50


// CHA counters are MSR-based.  
//   The starting MSR address is 0x0E00 + 0x10*CHA
//      Offset 0 is Unit Control -- mostly un-needed
//      Offsets 1-4 are the Counter PerfEvtSel registers
//      Offset 5 is Filter0     -- selects state for LLC lookup event (and TID, if enabled by bit 19 of PerfEvtSel)
//      Offset 6 is Filter1 -- lots of filter bits, including opcode -- default if unused should be 0x03b, or 0x------33 if using opcode matching
//      Offset 7 is Unit Status
//      Offsets 8,9,A,B are the Counter count registers
#define CHA_MSR_PMON_BASE 0x0E00L
#define CHA_MSR_PMON_CTL_BASE 0x0E01L
#define CHA_MSR_PMON_FILTER0_BASE 0x0E05L
#define CHA_MSR_PMON_FILTER1_BASE 0x0E06L
#define CHA_MSR_PMON_STATUS_BASE 0x0E07L
#define CHA_MSR_PMON_CTR_BASE 0x0E08L

#define NUM_CHA_BOXES 28
#define NUM_CHA_COUNTERS 4

#define EWMA_EXP 1
#define PRECISION 10 // bits


#define MIN_LOCAL_LAT 80
#define MIN_REMOTE_LAT 135

#define NUM_TIERS 2
#define NUM_COUNTERS 2
#define TOR_CORE_MON 1
#define RxC_CORE_MON 2

//IMC
#define NUM_SOCKETS             2
#define NUM_IMC_CHANNELS        6 //per socket
#define NUM_IMC_COUNTERS        5 //4 + 1(DCLK)


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
