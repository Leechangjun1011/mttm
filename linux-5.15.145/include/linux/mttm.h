#include <uapi/linux/perf_event.h>

#define KSAMPLED_CPU		0
#define PEBS_SAMPLE_PERIOD	10007
#define LIMIT_TENANTS		10
#define CORES_PER_SOCKET	24
#define MTTM_PEBS_BUFFER_SIZE	32 /* # of pages.(32 -> 128KB) */
#define MAX_SAMPLE_RATIO	50
#define MIN_SAMPLE_RATIO	10

#define DRAM_LLC_LOAD_MISS	0x1d3
#define REMOTE_DRAM_LLC_LOAD_MISS	0x2d3

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


extern void mttm_mm_init(struct mm_struct *mm);
extern unsigned long get_nr_lru_pages_node(struct mem_cgroup *memcg, pg_data_t *pgdat);
