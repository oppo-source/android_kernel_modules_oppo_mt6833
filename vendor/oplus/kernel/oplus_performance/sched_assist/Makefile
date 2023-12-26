# project: oplus/kernel/cpu
obj-y += sched_assist_common.o
obj-y += sched_assist_workqueue.o
obj-y += sched_assist_rt.o
#ifdef CONFIG_OPLUS_FEATURE_CPU_JANKINFO
obj-y += sa_jankinfo.o
#endif
obj-$(CONFIG_LOCKING_PROTECT) += sched_assist_locking.o

#qcom
ifeq ($(CONFIG_OPLUS_SYSTEM_KERNEL_QCOM), y)
ifeq ($(VERSION).$(PATCHLEVEL), 5.4)
obj-y += sched_assist_slide.o
endif
#mtk
else
obj-$(CONFIG_SCHED_WALT) += sched_assist_slide_v1.o
endif

obj-$(CONFIG_OPLUS_CPU_AUDIO_PERF) += sched_assist_audio.o

# project: oplus/kernel/synchronize
obj-$(CONFIG_OPLUS_LOCKING_STRATEGY) += oplus_locking_strategy.o
oplus_locking_strategy-y += sync/locking_main.o
oplus_locking_strategy-y += sync/futex.o
oplus_locking_strategy-y += sync/sysfs.o
oplus_locking_strategy-y += sync/mutex.o
oplus_locking_strategy-y += sync/rwsem.o
oplus_locking_strategy-$(CONFIG_OPLUS_LOCKING_MONITOR) += sync/kern_lock_stat.o

obj-$(CONFIG_OPLUS_FEATURE_EAS_OPT) += eas_opt/
