/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <drm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

#include "common.h"
#include "format.h"
#include "image.h"

static unsigned int util_yuv_height(unsigned int fourcc,
				    unsigned int width, unsigned int height)
{
	unsigned int virtual_height;

	switch (fourcc) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		virtual_height = height * 3 / 2;
		break;

	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		virtual_height = height * 2;
		break;

	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		virtual_height = height * 3;
		break;

	default:
		virtual_height = height;
		break;
	}

	return virtual_height;
}

static int util_load_raw_yuv(const char *file, unsigned int fourcc,
			     void *virtual[3], unsigned int width,
			     unsigned int height,
			     unsigned int stride[3], unsigned int bpp,
			     unsigned int start_offset)
{
	void *addr;
	FILE *fp = NULL;
	int i, n, div;

	fp = fopen(file, "rb");
	if (!fp) {
		fprintf(stderr, "Error file %s\n", file);
		perror("- error");
		return errno;
	}
	fseek(fp, start_offset, SEEK_SET);

	for (i = 0; i < 3; i++) {
		addr = virtual[i];
		div = 1;

		if (i != 0)
			if ((fourcc == DRM_FORMAT_YUV420) ||
			    (fourcc == DRM_FORMAT_YVU420))
				div = 2;

		for (n = 0; n < (int)height / div; n++) {
			size_t ret = fread(addr, 1, stride[i], fp);

			if (ret != (size_t)stride[i]) {
				fprintf(stderr, "Reading error: %s\n",
					strerror(errno));
				return errno;
			}

			addr += stride[i];
		}
	}

	fclose(fp);

	return 0;
}

static int util_load_raw_rgb(const char *file, unsigned int fourcc,
			     void *virtual, unsigned int width,
			     unsigned int height,
			     unsigned int stride, unsigned int bpp,
			     unsigned int start_offset)
{
	void *buffer;
	FILE *fp = NULL;
	int i;

	fp = fopen(file, "rb");
	if (!fp) {
		fprintf(stderr, "Error file %s\n", file);
		perror("- error");
		return errno;
	}
	fseek(fp, start_offset, SEEK_SET);

	buffer = malloc(stride);
	if (!buffer) {
		fprintf(stderr, "memory allocation failed\n");
		fclose(fp);
		return -ENOMEM;
	}

	for (i = 0; i < (int)height; i++) {
		size_t ret = fread(buffer, 1, stride, fp);

		if (ret != (size_t)stride) {
			fprintf(stderr, "Reading error: %s\n", strerror(errno));
			return errno;
		}

		memcpy(virtual, buffer, stride);
		virtual += stride;
	}

	free(buffer);
	fclose(fp);

	return 0;
}

struct bo *util_bo_create_image(int fd, unsigned int fourcc,
				unsigned int width, unsigned int height,
				unsigned int handles[4],
				unsigned int pitches[4],
				unsigned int offsets[4],
				const struct util_image_info *image)
{
	struct bo *bo;
	void *planes[3] = { 0, };
	void *virtual;
	struct stat st;
	int bpp = 8, isyuv = 0;
	unsigned int virtual_height = height;
	int ret;

	if (!image || !image->file) {
		fprintf(stderr, "No input image info !!!\n");
		return NULL;
	}

	if (!stat(image->file, &st) && (errno == EEXIST)) {
		fprintf(stderr, "Image file not found :%s\n", image->file);
		return NULL;
	}

	bpp = util_format_bpp(fourcc, width, height);
	if (!bpp)
		return NULL;

	isyuv = util_format_is_yuv(fourcc);
	if (isyuv)
		virtual_height = util_yuv_height(fourcc, width, height);

	bo = bo_create_dumb(fd, width, virtual_height, bpp);
	if (!bo)
		return NULL;

	ret = bo_map(bo, &virtual);
	if (ret) {
		fprintf(stderr, "failed to map buffer: %s\n",
			strerror(-errno));
		bo_destroy_dumb(bo);
		return NULL;
	}

	ret = bo_dumb_to_plane(fourcc, width, height,
			       bo, virtual, handles, pitches, offsets, planes);
	if (ret) {
		bo_unmap(bo);
		bo_destroy_dumb(bo);
		return NULL;
	}

	if (isyuv)
		util_load_raw_yuv(image->file, fourcc,
				  planes, width, height, pitches,
				  bpp, image->offset);
	else
		util_load_raw_rgb(image->file, fourcc,
				  planes[0], width, height, pitches[0],
				  bpp, image->offset);

	bo_unmap(bo);

	return bo;
}
