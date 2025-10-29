include $(KERNEL_ROOT)/.config

ifeq ($(CONFIG_ADD_NUTTX_FEATURES),y)
    include $(APPDIR)/Make.defs

    include $(APPDIR)/Application.mk

endif

ifeq ($(CONFIG_ADD_XIZI_FEATURES),y)
    SRC_FILES := sonsor_mutex.h sonsor_mutex.c

    include $(KERNEL_ROOT)/compiler.mk

endif
