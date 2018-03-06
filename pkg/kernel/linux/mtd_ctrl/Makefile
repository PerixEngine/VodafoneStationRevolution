JMK_ROOT=../../../../
include $(JMK_ROOT)/envir.mak


JMK_EXPORT_HEADERS_DIR = mtd_ctrl
JMK_EXPORT_HEADERS += mtd_ctrl.h

JMK_MOD_TARGET += rg_mtd_ctrl.o
JMK_MOD_2_STAT += rg_mtd_ctrl.o
JMK_O_OBJS_rg_mtd_ctrl.o += mtd_ctrl.o

JMK_RAMDISK_FIRST_TASKS+=rg_mtd_ctrl_dev

rg_mtd_ctrl_dev:
	$(JMKE_MKNOD) $(JMKE_RAMDISK_DEV_DIR)/rg_mtd_ctrl c 10 241

include $(JMKE_MK)
