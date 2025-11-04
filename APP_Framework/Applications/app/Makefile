include $(KERNEL_ROOT)/.config

ifeq ($(CONFIG_ADD_NUTTX_FEATURES),y)
    include $(APPDIR)/Make.defs

    include $(APPDIR)/Application.mk

endif

ifeq ($(CONFIG_ADD_XIZI_FEATURES),y)

<<<<<<< HEAD
    CFLAGS += -I../../Framework/connection
    CFLAGS += -I../../Framework/connection/wifi
=======
    CFLAGS += -I/home/xiuos/test/XIUOS_Proj_IntelligentStudyRoom/APP_Framework/Framework/connection
    CFLAGS += -I/home/xiuos/test/XIUOS_Proj_IntelligentStudyRoom/xiuos/APP_Framework/Framework/connection/wifi
>>>>>>> f054d182f6d03b3cd2ba6707413525e5e97124f3
    
    SRC_FILES := $(wildcard *.c)

    include $(KERNEL_ROOT)/compiler.mk

endif
