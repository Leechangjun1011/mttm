/*
 * kptscand
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/mm_inline.h>
#include <linux/swap.h>
#include <linux/pid.h>
#include <linux/sched/task.h>
#include <asm/pgtable.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#include <trace/events/mttm.h>
#include "../kernel/events/internal.h"
#include "internal.h"
#include <linux/mttm.h>
#include <linux/xarray.h>
#include <linux/vtmm.h>

int enable_kptscand = 0;
extern int current_tenants;
DEFINE_SPINLOCK(vtmm_register_lock);
struct task_struct *kptscand_thread = NULL;

#define NUM_AVAIL_DMA_CHAN	16
#define DMA_CHAN_PER_PAGE	2
extern unsigned int use_dma_migration;
extern struct dma_chan *copy_chan[NUM_AVAIL_DMA_CHAN];
extern struct dma_device *copy_dev[NUM_AVAIL_DMA_CHAN];

extern struct mem_cgroup **memcg_list;
extern unsigned int use_dram_determination;
extern unsigned long mttm_local_dram;

struct vtmm_page *get_vtmm_page(struct xarray *xa, struct page *page)
{
	return (struct vtmm_page *)xa_load(xa, page_to_pfn(page));
}

void uncharge_vtmm_page(struct page *page, struct mem_cgroup *memcg)
{
	struct vtmm_page *vp = NULL;
	int i;
	if(!memcg)
		return;
	if(!memcg->vtmm_enabled)
		return;

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		vp = xa_erase(memcg->ml_queue[i], page_to_pfn(page));
		if(vp) {
			spin_lock(memcg->bucket_lock[i]);
			list_del(&vp->list);
			spin_unlock(memcg->bucket_lock[i]);
			kmem_cache_free(vtmm_page_cache, vp);
			break;
		}
	}

}

SYSCALL_DEFINE1(vtmm_register_pid,
                const char __user *, u_name)
{
        int i; 
        struct mem_cgroup *memcg = mem_cgroup_from_task(current);
        char name[PATH_MAX];
        spin_lock(&vtmm_register_lock);

        if(current_tenants == LIMIT_TENANTS) {
                pr_info("[%s] Can't register tenant due to limit\n",__func__);
                spin_unlock(&vtmm_register_lock);
                return 0;
        } 

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		memcg->ml_queue[i] = kmalloc(sizeof(struct xarray), GFP_KERNEL);
		xa_init(memcg->ml_queue[i]);
	}	
 	for(i = 0; i < BUCKET_MAX; i++) {
		memcg->page_bucket[i] = kmalloc(sizeof(struct list_head), GFP_KERNEL);
		INIT_LIST_HEAD(memcg->page_bucket[i]);
		memcg->bucket_lock[i] = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
		spin_lock_init(memcg->bucket_lock[i]);
	}

	memcg->vtmm_enabled = true;

        if(use_dma_migration) {
                memcg->dma_chan_start = current_tenants * DMA_CHAN_PER_PAGE;
                if(memcg->dma_chan_start + DMA_CHAN_PER_PAGE > NUM_AVAIL_DMA_CHAN)
                        memcg->dma_chan_start = 0;
        }

        WRITE_ONCE(memcg_list[current_tenants], memcg);
        current_tenants++;

        copy_from_user(name, u_name, strnlen_user(u_name, PATH_MAX));
        strlcpy(memcg->tenant_name, name, PATH_MAX);

        for(i = 0; i < LIMIT_TENANTS; i++) {
                if(memcg_list[i]) {
                        if(memcg_list[i]->vtmm_enabled && use_dram_determination) {
                                WRITE_ONCE(memcg_list[i]->nodeinfo[0]->max_nr_base_pages, mttm_local_dram / current_tenants);
                                WRITE_ONCE(memcg_list[i]->max_nr_dram_pages, mttm_local_dram / current_tenants);
                                pr_info("[%s] [ %s ] dram size set to %lu MB\n",
                                        __func__, memcg_list[i]->tenant_name, memcg_list[i]->max_nr_dram_pages >> 8);
                        }
                }
        }

        pr_info("[%s] name : [ %s ], current_tenants : %d, dma_chan_start : %u, local_dram : %lu MB\n",
                __func__, memcg->tenant_name, current_tenants, memcg->dma_chan_start, (mttm_local_dram / current_tenants) >> 8);

        spin_unlock(&vtmm_register_lock);

        return 0;
}


SYSCALL_DEFINE1(vtmm_unregister_pid,
                pid_t, pid)
{
        int i;
        struct mem_cgroup *memcg = mem_cgroup_from_task(current);
        spin_lock(&vtmm_register_lock);

        current_tenants--;
//        kmigrated_stop(memcg); 

	memcg->vtmm_enabled = false;

	for(i = 0; i < LIMIT_TENANTS; i++) {
                if(READ_ONCE(memcg_list[i]) == memcg) {
                        WRITE_ONCE(memcg_list[i], NULL);
                        break;
                }
        }

	for(i = 0; i < ML_QUEUE_MAX; i++) {
		xa_destroy(memcg->ml_queue[i]);
		kfree(memcg->ml_queue[i]);
	}

	for(i = 0; i < BUCKET_MAX; i++) {
		kfree(memcg->page_bucket[i]);
		kfree(memcg->bucket_lock[i]);
	}

        // Re-distribute local DRAM
        for(i = 0; i < LIMIT_TENANTS; i++) {
                if(READ_ONCE(memcg_list[i])) {
                        WRITE_ONCE(memcg_list[i]->dram_fixed, false);
                }
        }

        spin_unlock(&vtmm_register_lock);

        pr_info("[%s] unregistered pid : %d, name : [ %s ], current_tenants : %d\n",
                __func__, pid, memcg->tenant_name, current_tenants);
        return 0;
}



static int kptscand(void *dummy)
{
	unsigned long sleep_timeout = msecs_to_jiffies(5000);
	unsigned long total_time, total_cputime = 0, one_cputime;
	struct mem_cgroup *memcg;
	int i;

	total_time = jiffies;
	while(!kthread_should_stop()) {
		one_cputime = jiffies;
		for(i = 0; i < LIMIT_TENANTS; i++) {
			memcg = READ_ONCE(memcg_list[i]);
			if(memcg) {
				unsigned long nr_xa_pages = 0;
				unsigned long index;
				struct vtmm_page *vp;
				unsigned long nr_list_pages = 0;
				struct list_head *lh;

				xa_for_each(memcg->ml_queue[0], index, vp) {
					nr_xa_pages++;
				}
			

				list_for_each(lh, memcg->page_bucket[0]) {
					nr_list_pages++;
				}

				pr_info("[%s] [ %s ] xa_size : %lu (%lu MB), list_size : %lu (%lu MB)\n",
					__func__, memcg->tenant_name,
					nr_xa_pages, nr_xa_pages >> 8, nr_list_pages, nr_list_pages >> 8);
			}
		}

		total_cputime += (jiffies - one_cputime);
		schedule_timeout_interruptible(sleep_timeout);
	}

	total_time = jiffies - total_time;
	pr_info("[%s] total_time : %lu, total_cputime : %lu\n",
		__func__, total_time, total_cputime);
	return 0;
}


static int kptscand_run(void)
{
	int ret = 0, i;
	dma_cap_mask_t copy_mask;

	if(!kptscand_thread) {
		if(!memcg_list)
			memcg_list = kzalloc(sizeof(struct mem_cgroup *) * LIMIT_TENANTS, GFP_KERNEL);

		pr_info("[%s] try kthread run\n",__func__);
		kptscand_thread = kthread_run_on_cpu(kptscand, NULL, KPTSCAND_CPU, "kptscand");
		pr_info("[%s] kthread run done\n",__func__);
		if(IS_ERR(kptscand_thread)) {
			pr_err("Failed to start kptscand\n");
			ret = PTR_ERR(kptscand_thread);
			kptscand_thread = NULL;
		}
		else {
			if(use_dma_migration) {
				dma_cap_zero(copy_mask);
                                dma_cap_set(DMA_MEMCPY, copy_mask);
                                dmaengine_get();

                                for(i = 0; i < NUM_AVAIL_DMA_CHAN; i++) {
                                        if(!copy_chan[i])
                                                copy_chan[i] = dma_request_channel(copy_mask, NULL, NULL);
                                        if(!copy_chan[i]) {
                                                pr_err("%s: cannot grap channel: %d\n", __func__, i);
                                                continue;
                                        }

                                        copy_dev[i] = copy_chan[i]->device;
                                        if(!copy_dev[i]) {
                                                pr_err("%s: no device: %d\n", __func__, i);
                                                continue;
                                        }
                                }

                                pr_info("[%s] dma channel opened\n",__func__);
                        }
                        pr_info("[%s] kptscand start\n",__func__);
		}
	}

	return ret;
}


static void kptscand_stop(void)
{
	int i;
	if(kptscand_thread) {
		kthread_stop(kptscand_thread);
		kptscand_thread = NULL;
	}

	if(memcg_list) {
		kfree(memcg_list);
		memcg_list = NULL;
	}

	if(use_dma_migration) {
		for(i = 0; i < NUM_AVAIL_DMA_CHAN; i++) {
			if(copy_chan[i]) {
				dma_release_channel(copy_chan[i]);
				copy_chan[i] = NULL;
				copy_dev[i] = NULL;
			}
		}
		dmaengine_put();
	}

	pr_info("[%s] kptscand stop\n",__func__);
}




#ifdef CONFIG_PROC_SYSCTL
int sysctl_enable_kptscand(struct ctl_table *table, int write,
                        void *buffer, size_t *lenp, loff_t *ppos)
{
        int err = 0;

        if (write && !capable(CAP_SYS_ADMIN))
                return -EPERM;

        err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

        if (err < 0)
                return err;
        if (write) {
                if(!kptscand_thread &&
                        enable_kptscand == 1 &&
                        current_tenants == 0) {
                        kptscand_run();
                }
                else if(kptscand_thread &&
                        enable_kptscand == 0) {
                        kptscand_stop();
                }
        }
        return err;
}

#endif
