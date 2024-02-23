/*
 * Control plane for multi tenants tiered memory.
 * It includes PEBS sampling, hot/cold identification, requesting migration.
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/syscalls.h>

#include "../kernel/events/internal.h"

#include <linux/mttm.h>

int enable_ksampled = 0;
int current_tenants = 0;
struct task_struct *ksampled_thread = NULL;
struct perf_event ***pfe;
DEFINE_SPINLOCK(register_lock);

static bool valid_va(unsigned long addr)
{
	if(!(addr >> (PGDIR_SHIFT + 9)) && addr != 0)
		return true;
	else
		return false;
}

static __u64 get_pebs_event(enum eventtype e)
{
	switch(e) {
		case DRAMREAD:
			return DRAM_LLC_LOAD_MISS;
		case CXLREAD:
			return REMOTE_DRAM_LLC_LOAD_MISS;
		default:
			return NR_EVENTTYPE;
	}
}

static int __perf_event_open(__u64 config, __u64 config1, __u64 cpu,
        __u64 type)
{
	struct perf_event_attr attr;
	struct file *file;
	int event_fd, __pid;

	memset(&attr, 0, sizeof(struct perf_event_attr));

	attr.type = PERF_TYPE_RAW;
	attr.size = sizeof(struct perf_event_attr);
	attr.config = config;
	attr.config1 = config1;

	attr.sample_period = PEBS_SAMPLE_PERIOD;

	attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR;

	attr.disabled = 0;
	attr.exclude_kernel = 1;
	attr.exclude_hv = 1;
	attr.exclude_callchain_kernel = 1;
	attr.exclude_callchain_user = 1;
	attr.precise_ip = 1;
	/*attr.enable_on_exec = 1;
	attr.comm = 1;
	attr.comm_exec = 1;
	*/

	event_fd = mttm_perf_event_open(&attr, -1, cpu, -1, 0);
	if (event_fd <= 0) {
		pr_info("[%s] perf_event_open fail. event_fd: %d\n",__func__, event_fd);
		return -1;
	}

	file = fget(event_fd);
	if (!file) {
		pr_info("[%s] invalid file\n",__func__);
		return -1;
	}

	pfe[cpu][type] = fget(event_fd)->private_data;
	return 0;
}

SYSCALL_DEFINE1(mttm_register_pid,
		pid_t, pid)
{
	int cpu, event;
		
	spin_lock(&register_lock);

	if(current_tenants == LIMIT_TENANTS) {
		pr_info("[%s] Can't register tenant due to limit\n",__func__);
		spin_unlock(&register_lock);
		return 0;
	}

	current_tenants++;
	spin_unlock(&register_lock);
	pr_info("[%s] registered pid : %d. current_tenants : %d, memcg id : %d\n",
		__func__, pid, current_tenants, mem_cgroup_id(mem_cgroup_from_task(current)));
	
	return 0;
}

SYSCALL_DEFINE1(mttm_unregister_pid,
		pid_t, pid)
{
	int i;

	spin_lock(&register_lock);

	current_tenants--;

	spin_unlock(&register_lock);
	pr_info("[%s] unregistered pid : %d, current_tenants : %d\n",
		__func__, pid, current_tenants);
	return 0;
}

/*
 * Called at page fault handler.
 * Allocate metadata for page info.
 */
/*int register_pginfo(
{
//TODO

}


static void update_pginfo(unsigned long addr)
{




}
*/

static void ksampled_do_work(void)
{
	int cpu, event, cond = true;
	int nr_sampled = 0, nr_skip = 0;

	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		nr_sampled = 0;
		for(event = 0; event < NR_EVENTTYPE; event++) {
			do {
				struct perf_buffer *rb;
				struct perf_event_mmap_page *up;
				struct perf_event_header *ph;
				struct mttm_event *me;
				unsigned long pg_index, offset;
				int page_shift;
				__u64 head;

				if(!pfe[cpu][event]) {
					break;
				}	

				__sync_synchronize();	

				rb = pfe[cpu][event]->rb;
				if(!rb) {
					pr_info("[%s] event->rb is NULL\n",__func__);
					break;
				}

				up = READ_ONCE(rb->user_page);
				head = READ_ONCE(up->data_head);
				if(head == up->data_tail) {
					nr_skip++;
					break;
				}	

				// It does not modify the up->data_head.
				
				head -= up->data_tail;
				if(head > (up->data_size * MAX_SAMPLE_RATIO / 100)) {
					pr_info("[%s] cond : true, head : %lu, max threshold : %lu, cpu : %d, event : %d\n",
						__func__, head, up->data_size * MAX_SAMPLE_RATIO / 100, cpu, event);
					cond = true;
				}
				else if (head < (up->data_size * MIN_SAMPLE_RATIO / 100)) {
					//pr_info("[%s] cond : false, head : %lu, min threshold : %lu, cpu : %d, event : %d\n",
					//	__func__, head, up->data_size * MIN_SAMPLE_RATIO / 100, cpu, event);
					cond = false;
				}

				smp_rmb();

				page_shift = PAGE_SHIFT + page_order(rb);
				offset = READ_ONCE(up->data_tail);
				pg_index = (offset >> page_shift) & (rb->nr_pages - 1);
				offset &= (1 << page_shift) - 1;
				ph = (void *)(rb->data_pages[pg_index] + offset);
				switch(ph->type) {
					case PERF_RECORD_SAMPLE:
						me = (struct mttm_event *)ph;
						if(!valid_va(me->addr)) {
							pr_info("[%s] invalid addr. cpu : %d, event : %d\n",
								__func__, cpu, event);
							break;
						}
						
						//update_pginfo
						}
						break;
					case PERF_RECORD_THROTTLE:
					case PERF_RECORD_UNTHROTTLE:
						break;
					case PERF_RECORD_LOST_SAMPLES:
						pr_info("[%s] lost sample\n",__func__);
						break;
					default:
						pr_info("[%s] unknown sample\n",__func__);
						break;
				}	

				smp_mb();
				WRITE_ONCE(up->data_tail, up->data_tail + ph->size);
			} while(cond);
		}
	}

}

static int ksampled(void *dummy)
{
	unsigned long sleep_timeout = usecs_to_jiffies(20000);

	while(!kthread_should_stop()) {
		ksampled_do_work();
		schedule_timeout_interruptible(sleep_timeout);
	}

	return 0;
}

static int ksampled_run(void)
{
	int ret = 0, i, cpu, event;
	if (!ksampled_thread) {
		pfe = kzalloc(sizeof(struct perf_event **) * CORES_PER_SOCKET, GFP_KERNEL);
		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			pfe[cpu] = kzalloc(sizeof(struct perf_event *) * NR_EVENTTYPE, GFP_KERNEL);
		}

		for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
			for(event = 0; event < NR_EVENTTYPE; event++) {
				if(get_pebs_event(event) == NR_EVENTTYPE) {
					pfe[cpu][event] = NULL;
					continue;
				}
				
				if(__perf_event_open(get_pebs_event(event), 0, cpu, event))
					return -1;
				if(mttm_perf_event_init(pfe[cpu][event], MTTM_PEBS_BUFFER_SIZE))
					return -1;
			}
		}

		ksampled_thread = kthread_run_on_cpu(ksampled, NULL, KSAMPLED_CPU, "ksampled");
		if(IS_ERR(ksampled_thread)) {
			pr_err("Failed to start ksampled\n");
			ret = PTR_ERR(ksampled_thread);
			ksampled_thread = NULL;
		}
	}
	return ret;
}

static void ksampled_stop(void)
{
	int cpu, event;
	if(ksampled_thread) {
		kthread_stop(ksampled_thread);	
		ksampled_thread = NULL;
	}
	
	for (cpu = 0; cpu < CORES_PER_SOCKET; cpu++) {
		for (event = 0; event < NR_EVENTTYPE; event++) {
			if (pfe[cpu][event])
				perf_event_disable(pfe[cpu][event]);
		}
	}

	for(cpu = 0; cpu < CORES_PER_SOCKET; cpu++)
		kfree(pfe[cpu]);
	kfree(pfe);

	pr_info("[%s] ksampled stop\n", __func__);

}


#ifdef CONFIG_PROC_SYSCTL
int sysctl_enable_ksampled(struct ctl_table *table, int write,
			void *buffer, size_t *lenp, loff_t *ppos)
{
	int err = 0;
	
	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (err < 0)
		return err;
	if (write) {
		if(!ksampled_thread &&
			enable_ksampled == 1 &&
			current_tenants == 0) {
			ksampled_run();
		}
		else if(ksampled_thread &&
			enable_ksampled == 0) {
			ksampled_stop();
		}
	}
	return err;
}
#endif





