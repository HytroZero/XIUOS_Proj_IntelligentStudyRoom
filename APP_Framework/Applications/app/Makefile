include $(KERNEL_ROOT)/.config

ifeq ($(CONFIG_ADD_NUTTX_FEATURES),y)
    include $(APPDIR)/Make.defs

    include $(APPDIR)/Application.mk

endif

ifeq ($(CONFIG_ADD_XIZI_FEATURES),y)

    CFLAGS += -I/home/xiuos/test/XIUOS_Proj_IntelligentStudyRoom/APP_Framework/Framework/connection
    CFLAGS += -I/home/xiuos/test/XIUOS_Proj_IntelligentStudyRoom/xiuos/APP_Framework/Framework/connection/wifi
    
    SRC_FILES := $(wildcard *.c)

    include $(KERNEL_ROOT)/compiler.mk

endif
