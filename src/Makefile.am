WARN_CFLAGS = \
	-Wall \
	-Wextra \
	-Wsign-compare \
	-Werror-implicit-function-declaration \
	-Wpointer-arith \
	-Wwrite-strings \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wnested-externs \
	-Wpacked \
	-Wswitch-enum \
	-Wmissing-format-attribute \
	-Wstrict-aliasing=2 \
	-Winit-self \
	-Wdeclaration-after-statement \
	-Wold-style-definition \
	-Wno-unused-parameter \
	-Wno-attributes \
	-Wno-long-long \
	-Winline \
	-Wshadow \
	-Wno-missing-field-initializers

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-Wno-pointer-arith \
	-fvisibility=hidden \
	-pthread \
	-Wno-unused-result \
	-g -O2 \
	-I${includedir}/drm

AM_CFLAGS += -static

UTIL_SOURCES = iomap.c
DRMKMS_SOURCES = kms.c buffers.c format.c image.c
DEVICE_SOURCES = mlc.c

#capture_display_LDADD = -ldrm 
capture_display_LDADD = libdrm-arm.a -lm

capture_display_SOURCES = capture_display.c $(DEVICE_SOURCES) $(UTIL_SOURCES) $(DRMKMS_SOURCES)
bin_PROGRAMS = capture-display
