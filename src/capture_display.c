#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "kms.h"
#include "format.h"
#include "image.h"

#include "io.h"
#include "iomap.h"
#include "mlc.h"

#define DRM_MODULE_NAME "nexell"

struct raw_header {
        char sign[4];
        int device, layer;
        unsigned int dummy;
        /* offset 0x10 */
        struct mlc_reg mlc;     /* 986 byte */
};
#define RAW_HEADER_SIZE  (1024)
#define RAW_HEADER_SIGN  { 'M', 'L', 'C', '\n' }

struct plane_opt {
        unsigned int plane_id;  /* the id of plane to use */
	unsigned int crtc_id;  /* the id of CRTC to bind to */
	unsigned int fb_id;
	struct bo *bo;
	unsigned int fourcc;

        /* source rect */
	unsigned int src_x, src_y, src_w, src_h;
        /* target rect */
        int crtc_x, crtc_y;
        unsigned int crtc_w, crtc_h;

        struct util_image_info image;
};

enum op_flag {
        op_flag_print,
        op_flag_capture,
        op_flag_update,
        op_flag_unknown,
};

struct op_arg {
        int device;
        enum mlc_layer layer;
        const void *addr, *mem;
	unsigned int hw_format;
        int bpp;
        char *file;
        FILE *fp;
        enum op_flag flag;
        struct plane_opt plane;
};

struct format_name {
        unsigned int format;
        const char *name;
};

static const struct format_name mlc_format_name[] = {
        /* rgb */
        { mlc_rgbfmt_r5g6b5   , "R5G6B5" },
        { mlc_rgbfmt_b5g6r5   , "B5G6R5" },
        { mlc_rgbfmt_x1r5g5b5 , "X1R5G5B5" },
	{ mlc_rgbfmt_x1b5g5r5 , "X1B5G5R5" },
	{ mlc_rgbfmt_x4r4g4b4 , "X4R4G4B4" },
	{ mlc_rgbfmt_x4b4g4r4 , "X4B4G4R4" },
	{ mlc_rgbfmt_x8r3g3b2 , "X8R3G3B2" },
	{ mlc_rgbfmt_x8b3g3r2 , "X8B3G3R2" },
	{ mlc_rgbfmt_a1r5g5b5 , "A1R5G5B5" },
	{ mlc_rgbfmt_a1b5g5r5 , "A1B5G5R5" },
	{ mlc_rgbfmt_a4r4g4b4 , "A4R4G4B4" },
	{ mlc_rgbfmt_a4b4g4r4 , "A4B4G4R4" },
	{ mlc_rgbfmt_a8r3g3b2 , "A8R3G3B2" },
	{ mlc_rgbfmt_a8b3g3r2 , "A8B3G3R2" },
	{ mlc_rgbfmt_r8g8b8   , "R8G8B8"   },
	{ mlc_rgbfmt_b8g8r8   , "B8G8R8"   },
	{ mlc_rgbfmt_x8r8g8b8 , "X8R8G8B8" },
	{ mlc_rgbfmt_x8b8g8r8 , "X8B8G8R8" },
	{ mlc_rgbfmt_a8r8g8b8 , "A8R8G8B8" },
	{ mlc_rgbfmt_a8b8g8r8 , "A8B8G8R8" },
        /* video */
        { mlc_yuvfmt_420      , "YUV420"   },
	{ mlc_yuvfmt_422      , "YUV422"   },
	{ mlc_yuvfmt_444      , "YUV444"   },
	{ mlc_yuvfmt_yuyv     , "YUYV"     },
};

static void print_mlc_rgb(int device, int layer, struct mlcrgblayer *r)
{
        struct mlc_reg reg;

        if (r == NULL) {
                hw_mlc_dump(device, &reg);
                r = &reg.rgb[layer];
        }

        fprintf(stdout, "MLC.%d RGB.%d\n", device, layer);
        fprintf(stdout, " leftright                : 0x%08x\n", r->mlcleftright);
        fprintf(stdout, " topbottom                : 0x%08x\n", r->mlctopbottom);
        fprintf(stdout, " invalidleftright0        : 0x%08x\n", r->mlcinvalidleftright0);
        fprintf(stdout, " invalidtopbottom0        : 0x%08x\n", r->mlcinvalidtopbottom0);
        fprintf(stdout, " invalidleftright1        : 0x%08x\n", r->mlcinvalidleftright1);
        fprintf(stdout, " invalidtopbottom1        : 0x%08x\n", r->mlcinvalidtopbottom1);
        fprintf(stdout, " control                  : 0x%08x\n", r->mlccontrol);
        fprintf(stdout, " hstride                  : 0x%08x\n", r->mlchstride);
        fprintf(stdout, " vstride                  : 0x%08x\n", r->mlcvstride);
        fprintf(stdout, " tpcolor                  : 0x%08x\n", r->mlctpcolor);
        fprintf(stdout, " invcolor                 : 0x%08x\n", r->mlcinvcolor);
        fprintf(stdout, " address                  : 0x%08x\n", r->mlcaddress);
}

static void print_mlc_yuv(int device, struct mlcyuvlayer *r)
{
        struct mlc_reg reg;

        if (r == NULL) {
                hw_mlc_dump(device, &reg);
                r = &reg.yuv;
        }

        fprintf(stdout, "MLC.%d VIDEO\n", device);
        fprintf(stdout, " leftright                : 0x%08x\n", r->mlcleftright);
        fprintf(stdout, " topbottom                : 0x%08x\n", r->mlctopbottom);
        fprintf(stdout, " control                  : 0x%08x\n", r->mlccontrol);
        fprintf(stdout, " vstride                  : 0x%08x\n", r->mlcvstride);
        fprintf(stdout, " tpcolor                  : 0x%08x\n", r->mlctpcolor);
        fprintf(stdout, " invcolor                 : 0x%08x\n", r->mlcinvcolor);
        fprintf(stdout, " address                  : 0x%08x\n", r->mlcaddress);
        fprintf(stdout, " addresscb                : 0x%08x\n", r->mlcaddresscb);
        fprintf(stdout, " addresscr                : 0x%08x\n", r->mlcaddresscr);
        fprintf(stdout, " vstridecb                : 0x%08x\n", r->mlcvstridecb);
        fprintf(stdout, " vstridecr                : 0x%08x\n", r->mlcvstridecr);
        fprintf(stdout, " hscale                   : 0x%08x\n", r->mlchscale);
        fprintf(stdout, " vscale                   : 0x%08x\n", r->mlcvscale);

        fprintf(stdout, " uenh                     : 0x%08x\n", r->mlcluenh);
        fprintf(stdout, " chenh[0]                 : 0x%08x\n", r->mlcchenh[0]);
        fprintf(stdout, " chenh[1]                 : 0x%08x\n", r->mlcchenh[1]);
        fprintf(stdout, " chenh[2]                 : 0x%08x\n", r->mlcchenh[2]);
        fprintf(stdout, " chenh[3]                 : 0x%08x\n", r->mlcchenh[2]);
}

static void print_mlc(int device, struct mlc_reg *r)
{
        struct mlc_reg reg;
        int i;

        if (r == NULL) {
                hw_mlc_dump(device, &reg);
                r = &reg; 
        }

        fprintf(stdout, "MLC.%d\n", device);
        fprintf(stdout, "controlt                  : 0x%08x\n", r->mlccontrolt);
        fprintf(stdout, "screensize                : 0x%08x\n", r->mlcscreensize);
        fprintf(stdout, "bgcolor                   : 0x%08x\n", r->mlcbgcolor);

        for (i = 0; i < 2; i++)
                print_mlc_rgb(device, i, &r->rgb[i]);

        print_mlc_yuv(device, &r->yuv);

        fprintf(stdout, "MLC.%d Gamma\n", device);
        fprintf(stdout, " paletetable2             : 0x%08x\n", r->mlcpaletetable2);
        fprintf(stdout, " gammacont                : 0x%08x\n", r->mlcgammacont);
        fprintf(stdout, " rgammatablewrite         : 0x%08x\n", r->mlcrgammatablewrite);
        fprintf(stdout, " ggammatablewrite         : 0x%08x\n", r->mlcggammatablewrite);
        fprintf(stdout, " bgammatablewrite         : 0x%08x\n", r->mlcbgammatablewrite);
        fprintf(stdout, " yuvlayergammatable_red   : 0x%08x\n", r->yuvlayergammatable_red);
        fprintf(stdout, " yuvlayergammatable_green : 0x%08x\n", r->yuvlayergammatable_green);
        fprintf(stdout, " yuvlayergammatable_blue  : 0x%08x\n", r->yuvlayergammatable_blue);
}

static void dump_mlc_rgb(int device, int layer)
{
        struct mlc_reg reg;
        unsigned int *addr, *base;
        unsigned int offset = 0;
        int i;

        hw_mlc_dump(device, &reg);

        addr = (unsigned int *)&reg.rgb[layer];
        base = (unsigned int *)hw_mlc_get_base(device) +
                ((unsigned int)&reg.rgb[layer] - (unsigned int)&reg);

        offset = (unsigned int)base % 16;
        base = base - offset;
        addr = addr - offset;

        fprintf(stdout, "MLC.%d RGB.%d\n", device, layer);
        for (i = 0; i < (int)sizeof(struct mlcrgblayer)/4; i++) {
                if (!(i % 4))
	                fprintf(stdout, "\n0x%08x: ", base);
                fprintf(stdout, "0x%08x ", *(unsigned int*)addr);
                base++, addr++;
        }
        fprintf(stdout, "\n");
}

static void dump_mlc_yuv(int device)
{
        struct mlc_reg reg;
        unsigned int *addr, *base;
        unsigned int offset = 0;
        int i;

        hw_mlc_dump(device, &reg);

        addr = (unsigned int *)&reg.yuv;
        base = (unsigned int *)hw_mlc_get_base(device) +
              ((unsigned int)addr - (unsigned int)&reg);

        offset = (unsigned int)base % 16;
        base = base - offset;
        addr = addr - offset;

        fprintf(stdout, "MLC.%d VIDEO\n", device);
        for (i = 0; i < (int)sizeof(struct mlcyuvlayer)/4; i++) {
                if (!(i % 4))
	                fprintf(stdout, "\n0x%08x: ", base);
                fprintf(stdout, "0x%08x ", *(unsigned int*)addr);
                base++, addr++;
        }
        fprintf(stdout, "\n");
}

static void dump_mlc(int device)
{
        struct mlc_reg reg;
        unsigned int *addr = (unsigned int *)&reg;
        unsigned int *base = (unsigned int *)hw_mlc_get_base(device);
        int i;

        hw_mlc_dump(device, &reg);
       
        fprintf(stdout, "MLC.%d\n", device);
        for (i = 0; i < (int)sizeof(struct mlc_reg)/4; i++) {
                if (!(i % 4))
	                fprintf(stdout, "\n0x%08x: ", base);
                fprintf(stdout, "0x%08x ", *addr);
                base++, addr++;
        }
        fprintf(stdout, "\n");
}

static const char *hw_format_name(unsigned int format, int bpp)
{
        int i;

        if (bpp == 32) {
                if (format == mlc_rgbfmt_r8g8b8)
                        format = mlc_rgbfmt_x8r8g8b8;

                if (format == mlc_rgbfmt_r8g8b8)
                        format = mlc_rgbfmt_x8r8g8b8;
        }

        for (i = 0; i < (int)ARRAY_SIZE(mlc_format_name); i++) {
                if (format == mlc_format_name[i].format)
                        return mlc_format_name[i].name;
        }

        return NULL; 
}

static int format_convert_rgb(unsigned int format,
                        unsigned int bpp, unsigned int *fourcc)
{
	switch (format) {
        case mlc_rgbfmt_x1r5g5b5:
	        *fourcc = DRM_FORMAT_XRGB1555;
		break;
        case mlc_rgbfmt_x1b5g5r5:
	        *fourcc = DRM_FORMAT_XBGR1555;
		break;
        case mlc_rgbfmt_r5g6b5:
	        *fourcc = DRM_FORMAT_RGB565;
		break;
        case mlc_rgbfmt_b5g6r5:
	        *fourcc = DRM_FORMAT_BGR565;
		break;
        case mlc_rgbfmt_r8g8b8:
        case mlc_rgbfmt_x8r8g8b8:
	        *fourcc = (bpp == 24) ? DRM_FORMAT_RGB888 : DRM_FORMAT_XRGB8888;
		break;
        case mlc_rgbfmt_b8g8r8:
        case mlc_rgbfmt_x8b8g8r8:
	        *fourcc = (bpp == 24) ? DRM_FORMAT_BGR888 : DRM_FORMAT_XBGR8888;
		break;
        case mlc_rgbfmt_a8r8g8b8:
	        *fourcc = DRM_FORMAT_ARGB8888;
		break;
        case mlc_rgbfmt_a8b8g8r8:
	        *fourcc = DRM_FORMAT_ABGR8888;
		break;
	default:
		fprintf(stderr, "Failed, not support 0x%x format %dbpp\n",
                                format, bpp);
		return -EINVAL;
	}

	return 0;
}

static unsigned int format_convert_yuv(unsigned int format, unsigned int *fourcc)
{
	switch (format) {
        case mlc_yuvfmt_420:
	        *fourcc = DRM_FORMAT_YUV420;
		break;
        case mlc_yuvfmt_422:
	        *fourcc = DRM_FORMAT_YUV422;
		break;
        case mlc_yuvfmt_444:
	        *fourcc = DRM_FORMAT_YUV444;
		break;
        case mlc_yuvfmt_yuyv:
	        *fourcc = DRM_FORMAT_YUYV;
		break;
	default:
		fprintf(stderr, "Failed, not support 0x%x format\n", format);
		return -EINVAL;
	}
	return 0;
}

static int get_plane_ids(struct device *dev, struct op_arg *op)
{
        struct plane_opt *p = &op->plane;
        int counts = 0;
        int i;

        /* crtc id */
	for (i = 0; i < dev->resources->res->count_crtcs; i++) {
		struct crtc *_crtc = &dev->resources->crtcs[i];
		drmModeCrtc *crtc = _crtc->crtc;

                if (i == op->device) {
                        if (!crtc)
			        return -EINVAL;

                        p->crtc_id = crtc->crtc_id;
                        break;
                }
	}

	if (!dev->resources->plane_res)
		return -EINVAL;

        /* plane id */
	for (i = 0; i < (int)dev->resources->plane_res->count_planes; i++) {
		struct plane *_plane = &dev->resources->planes[i];
		drmModePlane *plane = _plane->plane;
 
               
                if (!(plane->possible_crtcs & (1 << op->device)))
                        continue;

                if (counts == (int)op->layer) {
                        if (!plane)
			        return -EINVAL;
 
                        p->plane_id = plane->plane_id;
                        break;
                }
                counts++;
	}

	return 0;
}

static int format_to_fourcc(struct raw_header *header, struct op_arg *op)
{
        struct plane_opt *p = &op->plane;
        unsigned int format;
        int bpp;

        if (op->layer == mlc_layer_video) {
                struct mlcyuvlayer *r = &header->mlc.yuv;

                format = r->mlccontrol & _maskbit(16, 3);
                op->hw_format = format;
                op->bpp = 8;
                format_convert_yuv(format, &p->fourcc);
        } else {
                struct mlcrgblayer *r = &header->mlc.rgb[op->layer];

                format = r->mlccontrol & _maskbit(16, 16);
                bpp = r->mlchstride * 8;
                op->hw_format = format;
                op->bpp = bpp;
                format_convert_rgb(format, bpp, &p->fourcc);
        }

        return 0;
}

static int set_plane_rect(struct raw_header *header, struct op_arg *op)
{
        struct plane_opt *p = &op->plane;
        int constant = 2048; /* MLC H/V scale factor */
        int x, y;

        if (op->layer == mlc_layer_video) {
                struct mlcyuvlayer *r = &header->mlc.yuv;
                unsigned int format = r->mlccontrol & _maskbit(16, 3);
                int div = format == mlc_yuvfmt_yuyv ? 2 : 1;
                int w, h;

                x = _getbits(r->mlcleftright, 16, 12);
                y = _getbits(r->mlctopbottom, 16, 12);
                /* Note. get width with scale factor */
                w = _getbits(r->mlcleftright, 0, 11) - _getbits(r->mlcleftright, 16, 11) + 1;
                h = _getbits(r->mlctopbottom, 0, 11) - _getbits(r->mlctopbottom, 16, 11) + 1;

                /* Note. get width with stride for the aligned image */
#if 0                
                p->src_w = _getbits(r->mlchscale, 0, 23) * w / constant; 
#endif
                p->src_x = 0, p->src_y = 0;
                p->src_w = r->mlcvstride / div;
                p->src_h = _getbits(r->mlcvscale, 0, 23) * h / constant; 

                p->crtc_x = x,
                p->crtc_y = y;
                p->crtc_w = w;
                p->crtc_h = h;

        } else {
                struct mlcrgblayer *r = &header->mlc.rgb[op->layer];

                x = _getbits(r->mlcleftright, 16, 12);
                y = _getbits(r->mlctopbottom, 16, 12);
                /* Note. get width with stride for the aligned image */
#if 0
                p->src_w = _getbits(r->mlcleftright, 0, 11) -
                       _getbits(r->mlcleftright, 16, 11) + 1;
#endif
                p->src_x = 0, p->src_y = 0;
                p->src_w = r->mlcvstride / r->mlchstride;
                p->src_h = _getbits(r->mlctopbottom, 0, 11) -
                       _getbits(r->mlctopbottom, 16, 11) + 1;

                p->crtc_x = x,
                p->crtc_y = y;
                p->crtc_w = p->src_w;
                p->crtc_h = p->src_h;
        }


        return 0;
}

static int set_plane_image(struct raw_header *header, struct op_arg *op)
{
        struct util_image_info *image = &op->plane.image;

        image->file = op->file;
        image->offset = RAW_HEADER_SIZE;
        image->type = UTIL_IMAGE_RAW;

        return 0;
}

static int set_plane_property(struct raw_header *header, struct op_arg *op)
{
        
        return 0;
}

static int drm_set_plane(struct device *dev, struct plane_opt *p)
{
	drmModePlane *ovr = NULL;
	unsigned int handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
	unsigned int plane_id;
	struct bo *plane_bo;
	unsigned int plane_flags = 0;
	int crtc_x, crtc_y, crtc_w, crtc_h;
	struct crtc *crtc = NULL;
	unsigned int pipe;
	unsigned int i;

	/* Find an unused plane which can be connected to our CRTC. Find the
	 * CRTC index first, then iterate over available planes.
	 */
	for (i = 0; i < (unsigned int)dev->resources->res->count_crtcs; i++) {
		if (p->crtc_id == dev->resources->res->crtcs[i]) {
			crtc = &dev->resources->crtcs[i];
			pipe = i;
			break;
		}
	}

	if (!crtc) {
		fprintf(stderr, "CRTC %u not found\n", p->crtc_id);
		return -1;
	}

	plane_id = p->plane_id;

	for (i = 0; i < dev->resources->plane_res->count_planes; i++) {
		ovr = dev->resources->planes[i].plane;
		if (!ovr)
			continue;

		if (plane_id && plane_id != ovr->plane_id)
			continue;

		if (!drm_format_support(ovr, p->fourcc))
			continue;

		if ((ovr->possible_crtcs & (1 << pipe)) &&
		    (ovr->crtc_id == 0 || ovr->crtc_id == p->crtc_id)) {
			plane_id = ovr->plane_id;
			break;
		}
	}

	if (i == dev->resources->plane_res->count_planes) {
		fprintf(stderr, "no unused plane available for CRTC %u\n",
			crtc->crtc->crtc_id);
		return -1;
	}

	fprintf(stderr, "testing %d,%d,%dx%d@%s overlay plane %u\n",
		p->src_x, p->src_y, p->src_w, p->src_h, util_format_name(p->fourcc), plane_id);

	plane_bo = util_bo_create_image(dev->fd, p->fourcc, p->src_w, p->src_h, handles,
			     pitches, offsets, &p->image);
	if (plane_bo == NULL)
		return -1;

	p->bo = plane_bo;

	/* just use single plane format for now.. */
	if (drmModeAddFB2(dev->fd, p->src_w, p->src_h, p->fourcc,
			handles, pitches, offsets, &p->fb_id, plane_flags)) {
		fprintf(stderr, "failed to add fb: %s\n", strerror(errno));
		return -1;
	}

        crtc_x = p->crtc_x;
        crtc_y = p->crtc_y;
	crtc_w = p->crtc_w;
	crtc_h = p->crtc_h;

        printf("crtc x:%d,y:%d, w:%d,h:%d, plane x:%d, y:%d, w:%d, h:%d\n",
                crtc_x, crtc_y, crtc_w, crtc_h,
                p->src_x, p->src_y, p->src_w, p->src_h);

	/* note src coords (last 4 args) are in Q16 format */
	if (drmModeSetPlane(dev->fd, plane_id, crtc->crtc->crtc_id, p->fb_id,
			    plane_flags, crtc_x, crtc_y, crtc_w, crtc_h,
			    0, 0, p->src_w << 16, p->src_h << 16)) {
		fprintf(stderr, "failed to enable plane: %s\n",
			strerror(errno));
		return -1;
	}

	ovr->crtc_id = crtc->crtc->crtc_id;

	return 0;
}

static void drm_clear_plane(struct device *dev, struct plane_opt *p)
{
	if (p->fb_id)
		drmModeRmFB(dev->fd, p->fb_id);

	if (p->bo)
		bo_destroy_dumb(p->bo);
}

static int raw_capture_rgb(struct op_arg *op, struct mlcrgblayer *reg)
{
	FILE *fp = op->fp;
        char *mode = op->flag == op_flag_capture ? "ab" : "rb";
        void *addr, *mem, *mapped = NULL;
        int x, y, width, height, linestride, size;
        unsigned int format;
        int bpp, i;

        if (fp == NULL || reg == NULL)
                return -EINVAL;

        fp = fopen(op->file, mode);

        addr = (void *)reg->mlcaddress;
        format = reg->mlccontrol & _maskbit(16, 16);
        width = _getbits(reg->mlcleftright, 0, 11) -
                 _getbits(reg->mlcleftright, 16, 11) + 1;
        height = _getbits(reg->mlctopbottom, 0, 11) -
                 _getbits(reg->mlctopbottom, 16, 11) + 1;
        x = _getbits(reg->mlcleftright, 16, 12);
        y = _getbits(reg->mlctopbottom, 16, 12);
       
        linestride = reg->mlcvstride;
        size = linestride * height;
        bpp = reg->mlchstride * 8;

        fprintf(stdout, "get mlc.%d, rgb.%d %s(0x%x), %d,%d, %d x %d, line: %d, size: %dbyte\n",
                        op->device, op->layer, hw_format_name(format, bpp),
                        format, x, y, width, height, linestride, size);

       	mem = iomem_map(addr, size, mapped);
        if (mem == NULL){
	        fprintf(stderr ,"Fail, device %d, map %p\n",
                                op->device, addr);
                goto __exit_rgb;
	}

        fprintf(stdout, "- map %p -> %p %dbyte\n", addr, mem, size);

        for (i = 0; i < height; i++) {
                fwrite(mem, 1, linestride, fp);
                mem += linestride;
        }

__exit_rgb:
        iomem_free(mapped, size);
        fclose(fp);

        return 0;
}

static int raw_caputre_yuv(struct op_arg *op, struct mlcyuvlayer *reg)
{
	FILE *fp = op->fp;
        void *addr, *mem[3] = { NULL, }, *mapped[3] = { NULL, };
        int x, y, width, height, stride, size[3] = { 0, };
        unsigned int format;
        int constant = 2048; /* MLC H/V scale factor */
        int div = 1, i, ret;

        if (fp == NULL || reg == NULL)
                return -EINVAL;

        fp = fopen(op->file, "ab");

        format = reg->mlccontrol & _maskbit(16, 3);
        /* Note. get width with stride for the aligned image */
#if 0   
        width = _getbits(reg->mlcleftright, 0, 11) -
                 _getbits(reg->mlcleftright, 16, 11) + 1;
#endif        
        div = format == mlc_yuvfmt_yuyv ? 2 : 1;
        width = reg->mlcvstride / div;
        /* Note. get width with scale factor */
        height = _getbits(reg->mlctopbottom, 0, 11) -
                 _getbits(reg->mlctopbottom, 16, 11) + 1;
        height = _getbits(reg->mlcvscale, 0, 23) * height / constant; 
        x = _getbits(reg->mlcleftright, 16, 12);
        y = _getbits(reg->mlctopbottom, 16, 12);

        fprintf(stdout, "get mlc.%d, yuv.%d %s(0x%x), %d,%d, %d x %d, line: %d\n",
                        op->device, op->layer, hw_format_name(format, 8),
                        format, x, y, width, height, reg->mlcvstride);
        fprintf(stdout, "get mlc.%d, yuv.%d scale(h:%s,%x, v:%s(%x), %d x %d\n",
                        op->device, op->layer, 
                        _getbits(reg->mlchscale, 28, 2) == 3 ? "on" : "off",
                        _getbits(reg->mlchscale, 28, 2),
                        _getbits(reg->mlcvscale, 28, 2) == 3 ? "on" : "off",
                        _getbits(reg->mlcvscale, 28, 2),
                        _getbits(reg->mlchscale, 0, 23),
                        _getbits(reg->mlcvscale, 0, 23));

        for (i = 0; i < 3; i++) {
                int n, div = 1;

                if (i == 0) {
                        addr = (void *)reg->mlcaddress;
                        stride = reg->mlcvstride;
                } else if (i == 1) {
                        addr = (void *)reg->mlcaddresscb;
                        stride = reg->mlcvstridecb;
                        if (format == mlc_yuvfmt_420)
                                div = 2;
                } else {
                        addr = (void *)reg->mlcaddresscr;
                        stride = reg->mlcvstridecr;
                        if (format == mlc_yuvfmt_420)
                                div = 2;
                }
                size[i] = stride * (height / div);

                fprintf(stdout, "[%d] line %d x height %d (%dbyte)\n",
                        i, stride, height/div, size[i]);

               	mem[i] = iomem_map(addr, size[i], mapped[i]);
                if (mem[i] == NULL){
	                fprintf(stderr ,"Fail, device %d, map %p\n",
                                        op->device, addr);
                        ret = -EINVAL;
                        goto __exit;
	        }
                fprintf(stdout, "- map %p -> %p %dbyte\n", addr, mem[i], size[i]);

                for (n = 0; n < (height/div); n++) {
                        fwrite(mem[i], 1, stride, fp);
                        mem[i] += stride;
                }

                if (format == mlc_yuvfmt_yuyv)
                        goto __exit;
        }

__exit:
        for (i = 0; i < 3; i++) 
                iomem_free(mapped[i], size[i]);

        fclose(fp);

        return ret;
}

static int raw_image_header(struct raw_header *header, struct op_arg *op)
{
        const char sign[4] = RAW_HEADER_SIGN;
        char *mode = op->flag == op_flag_capture ? "wb" : "rb";
	FILE *fp = NULL;

        fp = fopen(op->file, mode);
        if (!fp) {
                fprintf(stderr, "Error file %s\n", op->file);
                perror("- error");
                return errno;
        }

        op->fp = fp;

        if (op->flag == op_flag_capture) {
                header->sign[0] = sign[0];
                header->sign[1] = sign[1];
                header->sign[2] = sign[2];
                header->sign[3] = sign[3];

                header->device = op->device;
                header->layer = op->layer;

                hw_mlc_dump(op->device, &header->mlc);

                fwrite((void *)header, 1, RAW_HEADER_SIZE, fp);
        } else {
                fread((void *)header, 1, RAW_HEADER_SIZE, fp);
                if (strncmp(header->sign, sign, 4)) {
                        fprintf(stderr, "Not found signature !!!\n");
                        return -EINVAL;
                }
                op->device = header->device;
                op->layer = header->layer;
        }

        fclose(fp);

        return 0;
}

static int capture_device(struct op_arg *op)
{
        struct raw_header *header;
        int ret;

        header = (struct raw_header *)malloc(RAW_HEADER_SIZE);
        if (!header) {
                fprintf(stderr, "memory allocation failed\n");
	        return -ENOMEM;
	}
        memset(header, 0, RAW_HEADER_SIZE);

        ret = raw_image_header(header, op);
        if (ret)
                goto __exit_capture;
        
        if (op->layer == mlc_layer_video)
                raw_caputre_yuv(op, &header->mlc.yuv);
        else
                raw_capture_rgb(op, &header->mlc.rgb[op->layer]);

__exit_capture:
        free(header);

        return ret;
}

static int update_device(struct op_arg *op)
{
        struct raw_header *header = NULL;
	struct device dev;
	struct plane_opt *p = &op->plane;
        struct mlc_reg mlc;
        int ret;

        memset(&dev, 0, sizeof(dev));

	dev.fd = drm_open(NULL, DRM_MODULE_NAME);
	if (dev.fd < 0)
		return -EINVAL;

        header = (struct raw_header *)malloc(RAW_HEADER_SIZE);
        if (!header) {
                fprintf(stderr, "memory allocation failed\n");
	        return -ENOMEM;
	}
        memset(header, 0, RAW_HEADER_SIZE);

        ret = raw_image_header(header, op);
        if (ret) 
                goto __exit_update;

	dev.resources = drm_get_resources(&dev);
	if (!dev.resources) {
                ret = -EINVAL;
                goto __exit_update;
	}

        ret = get_plane_ids(&dev, op);
        if (ret)
                goto __exit_update;

        fprintf(stdout, "set mlc.%d layer.%d -> crt.%d plane.%d\n",
                        op->device, op->layer,
                        op->plane.crtc_id, op->plane.plane_id);

        ret = format_to_fourcc(header, op);
        if (ret)
                goto __exit_update;

        ret = set_plane_rect(header, op);
        if (ret)
                goto __exit_update;

        fprintf(stdout, "src %d,%d, %d x %d %dbpp, %s(0x%x) - %s(0x%x)\n",
                        op->plane.src_x, op->plane.src_y, op->plane.src_w, op->plane.src_h, 
                        op->bpp, hw_format_name(op->hw_format, op->bpp),
                        op->hw_format,
                        util_format_name(op->plane.fourcc),
                        op->plane.fourcc);

        ret = set_plane_image(header, op);
        if (ret)
                goto __exit_update;

        drm_set_plane(&dev, p);

        set_plane_property(header, op);

        /* wait */      
        getchar();
        drm_clear_plane(&dev, p);

	drm_free_resources(dev.resources);

__exit_update:
        if (header)
                free(header);

        if (dev.fd >= 0)
		drm_close(dev.fd);

        return ret;
}

static int print_device(struct op_arg *op)
{
        switch (op->layer) {
        case mlc_layer_rgb0:
        case mlc_layer_rgb1:
                print_mlc_rgb(op->device, op->layer, NULL); 
                break;
        case mlc_layer_video:
                print_mlc_yuv(op->device, NULL); 
                break;
        default:                
                print_mlc(op->device, NULL); 
                break;
        }
        return 0;
}

static int parse_file(struct op_arg *op)
{
        struct raw_header *header = NULL;
        struct mlc_reg mlc;
        int ret;

        header = (struct raw_header *)malloc(RAW_HEADER_SIZE);
        if (!header) {
                fprintf(stderr, "memory allocation failed\n");
	        return -ENOMEM;
	}
        memset(header, 0, RAW_HEADER_SIZE);

        fprintf(stdout, "FILE  - %s\n", op->file);

        op->flag == op_flag_print;
        ret = raw_image_header(header, op);
        if (ret){  
                free(header);
                return -EINVAL;
        }
        fprintf(stdout, "MLC.%d - Layer.%d\n\n", op->device, op->layer);

        print_mlc(op->device, &header->mlc);

        free(header);

        return 0;
}

static int parse_arg(char *arg, struct op_arg *op)
{
        char *end;

        op->device = strtoul(arg, &end, 10);
        if (*end != ',')
                return op->flag == op_flag_print ? 0 : -EINVAL;

        arg = end +  1;
        op->layer = strtoul(arg, &end, 10);
        if (*end != ',')
                return op->flag == op_flag_print ? 0 : -EINVAL;

        arg = end +  1;
        op->file = arg;

        return 0;
}

static void usage(char *name)
{
	fprintf(stdout, "usage: %s \n", name);
	fprintf(stdout, "\n Options:\n");
	fprintf(stdout, "\t-c <dev>,<layer>,<file>\tcapture <dev>'s <layder> to <file>\n");
	fprintf(stdout, "\t-s <file>\tstore <file> with header info\n");
	fprintf(stdout, "\t-p <dev>,<layer>\tprint   <dev> and <layder>\n");
	fprintf(stdout, "\t-i <file>\tprint <file> header\n");
        fprintf(stderr, " Info:\n");
	fprintf(stderr, "\t<dev>\tsupport 0,1\n");
	fprintf(stderr, "\t<layer>\t0=RGB.0, 1=RGB.1, 2=Video layer\n");

	exit(0);
}

int main(int argc, char **argv)
{
	struct op_arg *op = NULL;
	int opt;
	const void *addr;
        void *mem = NULL;
        size_t size = 0;
        int ret = -EINVAL;

        op = malloc(sizeof *op);
        if (op == NULL) {
                fprintf(stderr, "memory allocation failed\n");
	        return -1;
	}
        memset(op, 0, sizeof(*op));
        op->layer = mlc_layer_unknown;
        op->flag = op_flag_unknown;

	while (-1 != (opt = getopt(argc, argv, "hc:s:p:i:"))) {
		switch (opt) {
		case 'c':
                        op->flag = op_flag_capture;
                        ret = parse_arg(optarg, op);
			break;
		case 's':
                        op->flag = op_flag_update;
                        op->file = optarg;
                        ret = 0;
			break;
               case 'p':
                        op->flag = op_flag_print;
                        ret = parse_arg(optarg, op);
			break;
                case 'i':
                        op->file = optarg;
                        ret = parse_file(op);
                        if (!ret)
                                return 0;
                        break;
		case 'h':
			usage(argv[0]);
			exit(0);
		default:
			break;
		}
	}

        if (ret) {
                usage(argv[0]);
                goto __exit;
        }

        addr = hw_mlc_get_base(op->device);
	if (addr == NULL) {
		fprintf(stderr ,"Fail, not support device.%d\n",
                                op->device);
                ret = -EINVAL;
                goto __exit;
	}
        size = hw_mlc_get_size(op->device);

	mem = iomem_map(addr, size, NULL);
	if (mem == NULL){
		fprintf(stderr ,"Fail, device %d, map %p\n",
                                op->device, addr);
                ret = -EINVAL;
                goto __exit;
	}
        hw_mlc_set_base(op->device, mem);

        op->addr = addr;
        op->mem = mem;

       fprintf(stdout, "reg mlc %p -> %p %dbyte\n", addr, mem, size);

        switch (op->flag) {
        case op_flag_print:
                ret = print_device(op);
                break;
        case op_flag_capture:
                ret = capture_device(op);
                break;
        case op_flag_update:
                ret = update_device(op);
                break;
        }
__exit:
        iomem_free(mem, size);
        free(op);

	return ret;
}
