// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) KBUILD_MODNAME " %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include <linux/cpufreq.h>
#include "../../../drivers/gpu/mediatek/gpufreq/v2/include/gpufreq_v2.h"
#include "../../../drivers/misc/mediatek/qos/mtk_qos_sram.h"
#include "../../../drivers/misc/mediatek/qos/mtk_qos_share.h"
#include "../../../drivers/misc/mediatek/qos/mtk_qos_ipi.h"
#include <dvfsrc-exp.h>
#include <slbc_ipi.h>
#include <slbc_sdk.h>
#include "idc_collect.h"

#define IDC_DEV "slc_dev"
#define BASE_VERSION	0x1
#define INDICATOR_COLLECT_VER(SIZE) ((SIZE << 16) | BASE_VERSION)
#define MMAP_DATA_VER	   1
#define MMAP_DATA_MAX_CNT   20
#define THREAD_POLLING_TIME 50 /* ms */

#define IDC_IOCTL_DEF(ioctl, _func) \
		[IDC_IOCTL_NR(ioctl)] = { \
				.cmd = ioctl, \
				.func = _func, \
		}

#define IDC_IOCTL_BASE		  's'
#define IDC_IO(nr)			  _IO(IDC_IOCTL_BASE, nr)
#define IDC_IOR(nr, type)	   _IOR(IDC_IOCTL_BASE, nr, type)
#define IDC_IOW(nr, type)	   _IOW(IDC_IOCTL_BASE, nr, type)
#define IDC_IOWR(nr, type)	  _IOWR(IDC_IOCTL_BASE, nr, type)
#define IDC_IOCTL_NR(n)		 _IOC_NR(n)
#define IDC_CORE_IOCTL_CNT	  ARRAY_SIZE(idc_ioctls)

#define IDC_IOCTL_GET_VERSION	   IDC_IOR(0x1, unsigned int)
#define IDC_IOCTL_GET_INDICATOR		 IDC_IOR(0x2, unsigned long long)

#define CPU_MAX_CLUSTER_NUM	 3   /* [CHIP] */
#define GPU_TARGET_DEFAULT	  0   /* [CHIP] */

extern void get_cg_force_size(unsigned int *cpu,  unsigned int *gpu);
extern void get_cache_usage_size(int *cpu, int *gpu, int *other);
extern int get_oplus_slc_dis_cdwb_status(void);
extern void get_cg_force_ratio(unsigned int *cpu, unsigned int *gpu);
extern int get_cache_priority_status(void);

enum {
	IDC_EMI_TP_CPU,
	IDC_EMI_TP_GPU,
	IDC_SLC_CPU_HITRATE,
	IDC_SLC_GPU_HITRATE,
	IDC_SLC_CPU_HITBW,
	IDC_SLC_GPU_HITBW,
	/* [CHIP] */
	IDC_CPU_CLUSTER0_FREQ,
	IDC_CPU_CLUSTER1_FREQ,
	IDC_CPU_CLUSTER2_FREQ,
	IDC_CPU_CLUSTER0_LOAD,
	IDC_CPU_CLUSTER1_LOAD,
	IDC_CPU_CLUSTER2_LOAD,
	/* [CHIP] */
	IDC_GPU_FREQ,
	IDC_MAX,
};

enum {
	EMI_TP_TOTAL,
	EMI_TP_CPU,
	EMI_TP_GPU,
	EMI_TP_MM,
	MAX_EMI_TP,
};

struct indicator_data{
	/* slc status */
	int cpu_usage;
	int gpu_usage;
	/* EMI TP */
	unsigned long long emi_tp_cpu;
	unsigned long long emi_tp_gpu;
	/* slc */
	unsigned long long slc_cpu_hit_rate;
	unsigned long long slc_gpu_hit_rate;
	unsigned long long slc_cpu_hit_bw;
	unsigned long long slc_gpu_hit_bw;
	/* cpu freq */
	unsigned long long cpu_cluster0_freq;
	unsigned long long cpu_cluster1_freq;
	unsigned long long cpu_cluster2_freq;
	/* cpu load */
	unsigned long long cpu_cluster0_load;
	unsigned long long cpu_cluster1_load;
	unsigned long long cpu_cluster2_load;

	/* gpu freq */
	unsigned long long gpu_freq;

	/* ddr info */
	unsigned long long ddr_dvfsrc_dram_khz;
	unsigned long long ddr_dvfsrc_vcore_uv;
};

struct indicator_ringbuf {
	bool buf_full;
	unsigned int idx;
	struct indicator_data data[MMAP_DATA_MAX_CNT];
};

struct MMAP_DATA {
	unsigned int cnt;
	struct indicator_data data[MMAP_DATA_MAX_CNT];
	/* idc ctrl */
	unsigned int idc_debug;
	unsigned int idc_enable;
	unsigned int blocklistmap;
	/* slc status */
	int cpu_force;
	int gpu_force;
	int dis_cdwb;	 /*0 : enable ; 1 : disablei */
	int cpu_force_ratio;
	int gpu_force_ratio;
	int priority;
};

struct Indicator_collect_priv{
	int type;
	void *mmap_addr;
};

struct idc_dev_info {
	dev_t devno;
	struct cdev cdev;
	struct class *class;
};

typedef int idc_ioctl_t(void *kdata, void *priv_info);

struct idc_ioctl_desc {
	unsigned int cmd;
	idc_ioctl_t *func;
};

struct cpu_cluster_info{
	unsigned int first_cpu;
	unsigned int num_cpu;
};

static struct idc_dev_info *dev_info;
static spinlock_t data_lock;

static unsigned int idc_debug;

static struct indicator_ringbuf local_buf;
static struct cpu_cluster_info cpu_cluster[CPU_MAX_CLUSTER_NUM];

static int ioctl_idc_version(void *kdata, void *priv_info);
static int ioctl_idc_data(void *kdata, void *priv_info);
static const struct idc_ioctl_desc idc_ioctls[] = {
	IDC_IOCTL_DEF(IDC_IOCTL_GET_VERSION, ioctl_idc_version),
	IDC_IOCTL_DEF(IDC_IOCTL_GET_INDICATOR, ioctl_idc_data),
};

static struct task_struct *indicator_collect_thread_oneshot;
static char indicator_collect_enable;
static unsigned int idc_blocklistmap;
static struct timer_list idc_timer;
static unsigned int uah_parm[3];

static void idc_slc_info(struct indicator_data *data)
{
	data->slc_cpu_hit_bw = slbc_get_cache_hit_bw(ID_CPU);
	data->slc_gpu_hit_bw = slbc_get_cache_hit_bw(ID_GPU);
}

static void idc_emi_info(struct indicator_data *data)
{
	static int bw_idx = -1;
	int select_idx;
	u32 emi_tp[MAX_EMI_TP] = {0};
	if (bw_idx == -1)
		bw_idx = qos_rec_get_hist_idx();
	for (select_idx = 0 ; select_idx < MAX_EMI_TP; select_idx++)
		emi_tp[select_idx] = (qos_rec_get_hist_data_bw(bw_idx, select_idx) & 0x7FFFFFFF) ^ 12345600;

	data->emi_tp_cpu = emi_tp[EMI_TP_CPU];
	data->emi_tp_gpu = emi_tp[EMI_TP_GPU];
}

static void cluster_init(void)
{
	unsigned int i;
	int j = -1;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(i) {
		policy = cpufreq_cpu_get_raw(i);
		if (!policy)
			continue;
		if (policy->cpu == i) {
			++j;
			if (j >= CPU_MAX_CLUSTER_NUM)
				break;
			cpu_cluster[j].first_cpu = i;
			cpu_cluster[j].num_cpu = 1;
		} else
			cpu_cluster[j].num_cpu++;
	}

	for (j = 0 ; j < CPU_MAX_CLUSTER_NUM; j++) {
		pr_info("[%s][%d]first cpu %d ; num_cpu %d\n", __func__, j, cpu_cluster[j].first_cpu, cpu_cluster[j].num_cpu);
	}
}

static void idc_cpu_info(struct indicator_data *data)
{
	struct cpufreq_policy *policy;
	unsigned int cluster_idx, freq_cur[CPU_MAX_CLUSTER_NUM];

	for (cluster_idx = 0 ; cluster_idx < CPU_MAX_CLUSTER_NUM ; cluster_idx++) {
		policy = cpufreq_cpu_get_raw(cpu_cluster[cluster_idx].first_cpu);
		freq_cur[cluster_idx] = (policy == NULL) ? 0 : policy->cur;
	}
	data->cpu_cluster0_freq = freq_cur[0];
	data->cpu_cluster1_freq = freq_cur[1];
	data->cpu_cluster2_freq = freq_cur[2];
}

static void idc_gpu_info(struct indicator_data *data)
{
	data->gpu_freq = (long long)gpufreq_get_cur_freq(TARGET_DEFAULT);
}

static void idc_get_slc_status(struct indicator_data *data)
{
	int use_c, use_g, use_other;

	get_cache_usage_size(&use_c, &use_g, &use_other);
	data->cpu_usage = use_c;
	data->gpu_usage = use_g;
}

static void idc_ddr_info(struct indicator_data *data)
{
	data->ddr_dvfsrc_dram_khz = (unsigned long long)mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_KHZ);
	data->ddr_dvfsrc_vcore_uv = (unsigned long long)mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_UV);
}

static void indicator_collect_info(void)
{
	unsigned long flags;
	struct indicator_data tmp;
	int idx;

	memset(&tmp, 0, sizeof(struct indicator_data));
	idc_slc_info(&tmp);
	idc_gpu_info(&tmp);
	idc_cpu_info(&tmp);
	idc_emi_info(&tmp);
	idc_ddr_info(&tmp);
	idc_get_slc_status(&tmp);

	spin_lock_irqsave(&data_lock, flags);
	idx = local_buf.idx;
	local_buf.data[idx] = tmp;
	idx++;
	if (idx >= MMAP_DATA_MAX_CNT) {
		idx = 0;
		local_buf.buf_full = true;
	}
	local_buf.idx = idx;
	spin_unlock_irqrestore(&data_lock, flags);
}

static int indicator_collect_main(void *arg)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_RUNNING);
		indicator_collect_info();
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();
	}
	return 0;
}

static int idc_dev_open(struct inode *inode, struct file *filp)
{
	struct Indicator_collect_priv *info;

	info = kzalloc(sizeof(struct Indicator_collect_priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(info))
		return -ENOMEM;

	info->type = -1;
	info->mmap_addr = NULL;
	spin_lock_init(&data_lock);
	filp->private_data = info;

	return 0;
}

static int idc_dev_release(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	struct Indicator_collect_priv *info = filp->private_data;
	if (info->mmap_addr != NULL) {
		spin_lock_irqsave(&data_lock, flags);
		vfree(info->mmap_addr);
		info->mmap_addr = NULL;
		spin_unlock_irqrestore(&data_lock, flags);
	}
	kfree(info);
	info = NULL;
	return 0;
}

static int idc_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;

	struct Indicator_collect_priv *info = filp->private_data;
	if (IS_ERR_OR_NULL(info)) {
			pr_err("%s: info is NULL\n", __func__);
			return -EINVAL;
	}

	info->mmap_addr = vmalloc_user(sizeof(struct MMAP_DATA));
	if (IS_ERR_OR_NULL(info->mmap_addr)) {
			pr_err("mmap_addr vmalloc failed!\n");
			return -ENOMEM;
	}

	if (remap_vmalloc_range(vma, info->mmap_addr,
			  vma->vm_pgoff)) {
		pr_err("remap failed\n");
		ret = -EAGAIN;
		goto err_remap;
	}
	return 0;

err_remap:
	vfree(info->mmap_addr);
	info->mmap_addr = NULL;
	return ret;
}

#define KDATA_SIZE	  64
long idc_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct Indicator_collect_priv *info = filp->private_data;
	const struct idc_ioctl_desc *ioctl = NULL;
	idc_ioctl_t *func;
	unsigned int nr = IDC_IOCTL_NR(cmd);
	int ret = -EINVAL;
	char kdata[KDATA_SIZE] = { };
	unsigned int in_size, out_size;

	if (nr >=  IDC_CORE_IOCTL_CNT) {
		pr_err("out of array\n");
		return -EINVAL;
	}

	ioctl = &idc_ioctls[nr];
	out_size = in_size = _IOC_SIZE(cmd);
	if ((cmd & IOC_IN) == 0)
		in_size = 0;
	if ((cmd & IOC_OUT) == 0)
		out_size = 0;

	if (out_size > KDATA_SIZE || in_size > KDATA_SIZE) {
		pr_err("out of memory\n");
		ret = -ENOMEM;
		goto err_out_of_mem;
	}

	func = ioctl->func;
	if (unlikely(!func)) {
		pr_err("no func\n");
		ret = -EINVAL;
		goto err_no_func;
	}

	if (copy_from_user(kdata, (void __user *)arg, in_size)) {
		pr_err("copy_from_user failed\n");
		ret = -EFAULT;
		goto err_fail_cp;
	}

	ret = func(kdata, info);
	if (copy_to_user((void __user *)arg, kdata, out_size)) {
		pr_err("copy_to_user failed\n");
		ret = -EFAULT;
	}

err_fail_cp:
err_no_func:
err_out_of_mem:
	return ret;
}

static const struct file_operations idc_dev_fops = {
	.owner = THIS_MODULE,
	.open = idc_dev_open,
	.release = idc_dev_release,
	.mmap = idc_dev_mmap,
	.unlocked_ioctl = idc_dev_ioctl,
};

static int create_dev(void)
{
	struct device *dev;
	int ret = 0;

	dev_info = kzalloc(sizeof(struct idc_dev_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(dev_info)) {
		pr_err("Fail to alloc dev info\n");
		ret = -ENOMEM;
		goto err_info_alloc;
	}

	ret = alloc_chrdev_region(&dev_info->devno, 0, 1, IDC_DEV);
	if (ret) {
		pr_err("Fail to alloc devno, ret=%d\n", ret);
		goto err_cdev_alloc;
	}

	dev_info->class = class_create(IDC_DEV);
	if (IS_ERR_OR_NULL(dev_info->class)) {
		pr_err("Fail to create class, ret=%d\n", ret);
		goto err_class_create;
	}

	dev = device_create(dev_info->class, NULL, dev_info->devno,
			dev_info, IDC_DEV);

	cdev_init(&dev_info->cdev, &idc_dev_fops);
	dev_info->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_info->cdev, dev_info->devno, 1);
	if (ret) {
		pr_err("Fail to add cdev, ret=%d\n", ret);
		goto err_cdev_add;
	}

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("Fail to create device, ret=%d\n", ret);
		goto err_device_create;
	}

	return 0;
err_device_create:
	class_destroy(dev_info->class);
err_cdev_add:
	unregister_chrdev_region(dev_info->devno, 1);
err_class_create:
	cdev_del(&dev_info->cdev);
err_cdev_alloc:
	kfree(dev_info);
err_info_alloc:
	return ret;
}

static int ioctl_idc_version(void *kdata, void *priv_info)
{
	unsigned int *tmp = (unsigned int *)kdata;
	*tmp = INDICATOR_COLLECT_VER(sizeof(struct MMAP_DATA));
	return 0;
}

static void show_indicator(void)
{
	int i;
	pr_info("[oplus_slc]=====local_buf.idx %d=====\n", local_buf.idx);
	for (i = 0 ; i < MMAP_DATA_MAX_CNT ; i++) {
		pr_info("[oplus_slc][%d] [%2d,%2d]\n", i, local_buf.data[i].cpu_usage, local_buf.data[i].gpu_usage);
		pr_info("[oplus_slc][%d] emi_tp_cpu %lld: emi_tp_gpu %lld\n", i, local_buf.data[i].emi_tp_cpu, local_buf.data[i].emi_tp_gpu);
		pr_info("[oplus_slc][%d] cpu_cluster0 %lld: cpu_cluster1 %lld; cpu_cluster2 %lld\n"
			, i, local_buf.data[i].cpu_cluster0_freq, local_buf.data[i].cpu_cluster1_freq, local_buf.data[i].cpu_cluster2_freq);
		pr_info("[oplus_slc][%d] gpu_freq %lld\n", i, local_buf.data[i].gpu_freq);
		pr_info("[oplus_slc][%d] ddr_freq %lld(kHz): vcore %lld(uv)\n", i, local_buf.data[i].ddr_dvfsrc_dram_khz, local_buf.data[i].ddr_dvfsrc_vcore_uv);
		pr_info("[oplus_slc][%d] slc_cpu_hit_bw %lld: slc_gpu_hit_bw %lld\n", i, local_buf.data[i].slc_cpu_hit_bw, local_buf.data[i].slc_gpu_hit_bw);
		pr_info("[oplus_slc][%d] slc_cpu_hit_rate %lld: slc_gpu_hit_rate %lld\n", i, local_buf.data[i].slc_cpu_hit_rate, local_buf.data[i].slc_gpu_hit_rate);
	}
}

static int ioctl_idc_data(void *kdata, void *priv_info)
{
	unsigned long flags;
	struct Indicator_collect_priv *info = priv_info;
	struct MMAP_DATA *buf = NULL;
	unsigned int f_c, f_g, dis_cdwb, fr_c, fr_g;
	int priority;

	if (IS_ERR_OR_NULL(info) || IS_ERR_OR_NULL(info->mmap_addr)) {
		pr_err("slc_ioctl_get_pmu: info is NUL\n");
		return -EINVAL;
	}
	buf = info->mmap_addr;
	get_cg_force_size(&f_c, &f_g);
	dis_cdwb = get_oplus_slc_dis_cdwb_status();
	get_cg_force_ratio(&fr_c, &fr_g);
	priority = get_cache_priority_status();

	spin_lock_irqsave(&data_lock, flags);
	if (idc_debug)
		show_indicator();
	buf->cpu_force = f_c;
	buf->gpu_force = f_g;
	buf->dis_cdwb = dis_cdwb;
	buf->cpu_force_ratio = fr_c;
	buf->gpu_force_ratio = fr_g;
	buf->priority = priority;

	buf->idc_enable = (unsigned int)indicator_collect_enable;
	buf->idc_debug = idc_debug;
	buf->blocklistmap = idc_blocklistmap;

	if (local_buf.buf_full) {
		buf->cnt = MMAP_DATA_MAX_CNT;
		if (local_buf.idx == 0)
			memcpy(buf->data, local_buf.data, sizeof(struct indicator_data) * MMAP_DATA_MAX_CNT);
		else {
			memcpy(&buf->data[0], &local_buf.data[local_buf.idx], sizeof(struct indicator_data) * (MMAP_DATA_MAX_CNT - local_buf.idx));
			memcpy(&buf->data[(MMAP_DATA_MAX_CNT - local_buf.idx)], &local_buf.data[0], sizeof(struct indicator_data) * local_buf.idx);
		}
	} else {
		buf->cnt = local_buf.idx;
		memcpy(buf->data, local_buf.data, sizeof(struct indicator_data) * local_buf.idx);
	}
	spin_unlock_irqrestore(&data_lock, flags);
	return 0;
}

static void idc_clear_history(void)
{
	unsigned long flags;
	spin_lock_irqsave(&data_lock, flags);
	memset(&local_buf, 0, sizeof(struct indicator_ringbuf));
	spin_unlock_irqrestore(&data_lock, flags);
}

static void idc_timer_callback(struct timer_list *timer)
{
	if (indicator_collect_thread_oneshot) {
		wake_up_process(indicator_collect_thread_oneshot);
		mod_timer(&idc_timer, jiffies + msecs_to_jiffies(THREAD_POLLING_TIME));
	}
}

static ssize_t idc_enable_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int val;
	char page[32] = {0};

	if (simple_write_to_buffer(page, sizeof(page), ppos, buf, count) <= 0)
		return -EINVAL;

	if (sscanf(page, "%u", &val) < 0) {
		pr_err("error setting argument. argument should be 1 or 0\n");
		return -EINVAL;
	}

	if (indicator_collect_enable == !!val)
		return count;

	if (val&1) {
		idc_clear_history();
		/*enable task*/
		if (IS_ERR(indicator_collect_thread_oneshot)) {
			pr_err("indicator_collect_thread_oneshot fail\n");
			return -1;
		}
		wake_up_process(indicator_collect_thread_oneshot);
		mod_timer(&idc_timer, jiffies + msecs_to_jiffies(THREAD_POLLING_TIME));
		indicator_collect_enable = 1;
	} else {
		/*suspend task*/
		indicator_collect_enable = 0;
		del_timer_sync(&idc_timer);
	}

	return count;
}

static const struct proc_ops idc_enable_proc_ops = {
	.proc_write			 = idc_enable_proc_write,
	.proc_lseek			 = seq_lseek,
};

static int idc_info_proc_show(struct seq_file *m, void *v)
{
        seq_printf(m, "indicator_collect_enable %d\n", indicator_collect_enable);
        seq_printf(m, "idc_debug %d\n", idc_debug);
        seq_printf(m, "idc_blocklistmap 0x%x\n", idc_blocklistmap);
        return 0;
}

static int idc_info_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, idc_info_proc_show, pde_data(inode));
}

static const struct proc_ops idc_info_proc_ops = {
        .proc_open      = idc_info_proc_open,
        .proc_read      = seq_read,
        .proc_lseek     = seq_lseek,
};

static int idc_ctrl_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u %u %u\n", uah_parm[0], uah_parm[1], uah_parm[2]);
	return 0;
}

static int idc_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, idc_ctrl_proc_show, pde_data(inode));
}

static ssize_t idc_ctrl_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int val_1;
	unsigned int val_2;
	unsigned int val_3;
	char page[32] = {0};

	if (simple_write_to_buffer(page, sizeof(page), ppos, buf, count) <= 0)
		return -EINVAL;

	ret = sscanf(page, "%u %u %u", &val_1, &val_2, &val_3);
	if (ret < 0) {
		pr_err("[%s]error setting argument.\n", __func__);
		return -EINVAL;
	}

	uah_parm[0] = val_1;
	uah_parm[1] = val_2;
	uah_parm[2] = val_3;

	if (val_1 == 1) {
		pr_info("[%s] block list, ret %d\n", __func__, ret);
		if (val_3 == 1)
			idc_blocklistmap |= (1 << val_2);
		else
			idc_blocklistmap &= (~(1 << val_2));
	} else if (val_1 == 99) {
		idc_debug = (val_2 == 0) ? 0 : 1;
	}
	return count;
}

static const struct proc_ops idc_ctrl_proc_ops = {
	.proc_write	= idc_ctrl_proc_write,
	.proc_open	= idc_ctrl_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
};

static void create_proc(struct proc_dir_entry *parent_dir)
{
	proc_create_data("idc_enable", 0664, parent_dir, &idc_enable_proc_ops, NULL);
	proc_create_data("idc_ctrl", 0664, parent_dir, &idc_ctrl_proc_ops, NULL);
	proc_create_data("idc_info", 0444, parent_dir, &idc_info_proc_ops, NULL);
}

int oplus_slc_indicator_init(struct proc_dir_entry *parent_dir)
{
	int ret = -1;
	indicator_collect_thread_oneshot = kthread_create(indicator_collect_main, 0, "idc_collect");
	if (IS_ERR(indicator_collect_thread_oneshot)) {
		pr_err("Failed to create indicator_collect_thread_oneshot");
		return ret;
	}

	cluster_init();
	create_dev();
	create_proc(parent_dir);
	timer_setup(&idc_timer, idc_timer_callback, TIMER_DEFERRABLE);

	return 0;
}

void oplus_slc_indicator_exit(void)
{
	if (indicator_collect_thread_oneshot)
		kthread_stop(indicator_collect_thread_oneshot);
}

void oplus_slc_indicator_suspend(bool isSuspend)
{
	if (isSuspend) {
		indicator_collect_enable = 0;
		del_timer(&idc_timer);
		pr_info("oplus_slc_idc suspend\n");
	} else
		pr_info("oplus_slc_idc resume\n");
}

