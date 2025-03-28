#include $(srctree)/drivers/misc/mediatek/Makefile.custom

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/aladdin_ov08d10mipiraw_Sensor.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_SUBDRV_OBJ)
endif

#obj-y += aladdin_ov08d10mipiraw_Sensor.o

