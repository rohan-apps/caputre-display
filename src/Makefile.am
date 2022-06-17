AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-W \
	-Wno-unused-result \
	-I${includedir}/drm

AM_CFLAGS += -static

UTIL_SOURCES = iomap.c
DRMKMS_SOURCES = kms.c buffers.c format.c image.c
DEVICE_SOURCES = mlc.c

#capture_display_LDADD = -ldrm 
capture_display_LDADD = libdrm-arm.a -lm

capture_display_SOURCES = capture_display.c $(DEVICE_SOURCES) $(UTIL_SOURCES) $(DRMKMS_SOURCES)
bin_PROGRAMS = capture-display
