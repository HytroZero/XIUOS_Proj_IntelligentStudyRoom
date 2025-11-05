include $(KERNEL_ROOT)/.config

ifeq ($(CONFIG_ADD_NUTTX_FEATURES),y)
    include $(APPDIR)/Make.defs

    include $(APPDIR)/Application.mk

endif

ifeq ($(CONFIG_ADD_XIZI_FEATURES),y)

    CFLAGS += -I../../Framework/connection
    CFLAGS += -I../../Framework/connection/wifi
    
    SRC_FILES := $(wildcard *.c)
    SRC_DIR := mqtt
    include $(KERNEL_ROOT)/compiler.mk

endif
