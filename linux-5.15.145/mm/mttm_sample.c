/*
 * Handle pebs sample for multi tenants tiered memory.
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>

int register_pid = 0;


#ifdef CONFIG_PROC_SYSCTL
int sysctl_register_pid(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp,
			loff_t *ppos)
{
	int err = 0;
	
	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (err < 0)
		return err;
	if (write) {
		pr_info("[%s] registered pid : %d\n",__func__, register_pid);

	}
	return err;
}


#endif


