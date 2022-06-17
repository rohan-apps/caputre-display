#ifndef __UTIL_IMAGE_H__
#define __UTIL_IMAGE_H__

#include "buffers.h"

enum util_image_type {
	UTIL_IMAGE_BMP,
	UTIL_IMAGE_RAW,
};

struct util_image_info {
        const char *file;
        enum util_image_type type;
        unsigned int offset;
};

struct bo *util_bo_create_image(int fd, unsigned int fourcc,
                                unsigned int width, unsigned int height,
                                unsigned int handles[4], unsigned int pitches[4],
                                unsigned int offsets[4],
                                const struct util_image_info *image);

#endif
