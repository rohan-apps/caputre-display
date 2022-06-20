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
	int module, layer;
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

	uint32_t connector_id;
	uint32_t encoder_id;
	int mode_index;

	struct util_image_info image;
};

enum op_mode {
	op_mode_print,
	op_mode_capture,
	op_mode_update,
};

#define FLAG_GAMMAN_OFF (1)

struct op_arg {
	int module;
	enum mlc_layer layer;
	const void *addr, *mem;
	unsigned int hw_format;
	int bpp;
	char *file;
	FILE *fp;
	enum op_mode mode;
	unsigned int flags;
	struct plane_opt plane;
};

struct format_name {
	unsigned int format;
	const char *name;
};

static const struct format_name mlc_format_name[] = {
	/* rgb */
	{ mlc_rgbfmt_r5g6b5, "R5G6B5" },
	{ mlc_rgbfmt_b5g6r5, "B5G6R5" },
	{ mlc_rgbfmt_x1r5g5b5, "X1R5G5B5" },
	{ mlc_rgbfmt_x1b5g5r5, "X1B5G5R5" },
	{ mlc_rgbfmt_x4r4g4b4, "X4R4G4B4" },
	{ mlc_rgbfmt_x4b4g4r4, "X4B4G4R4" },
	{ mlc_rgbfmt_x8r3g3b2, "X8R3G3B2" },
	{ mlc_rgbfmt_x8b3g3r2, "X8B3G3R2" },
	{ mlc_rgbfmt_a1r5g5b5, "A1R5G5B5" },
	{ mlc_rgbfmt_a1b5g5r5, "A1B5G5R5" },
	{ mlc_rgbfmt_a4r4g4b4, "A4R4G4B4" },
	{ mlc_rgbfmt_a4b4g4r4, "A4B4G4R4" },
	{ mlc_rgbfmt_a8r3g3b2, "A8R3G3B2" },
	{ mlc_rgbfmt_a8b3g3r2, "A8B3G3R2" },
	{ mlc_rgbfmt_r8g8b8, "R8G8B8"   },
	{ mlc_rgbfmt_b8g8r8, "B8G8R8"   },
	{ mlc_rgbfmt_x8r8g8b8, "X8R8G8B8" },
	{ mlc_rgbfmt_x8b8g8r8, "X8B8G8R8" },
	{ mlc_rgbfmt_a8r8g8b8, "A8R8G8B8" },
	{ mlc_rgbfmt_a8b8g8r8, "A8B8G8R8" },
	/* video */
	{ mlc_yuvfmt_420, "YUV420"   },
	{ mlc_yuvfmt_422, "YUV422"   },
	{ mlc_yuvfmt_444, "YUV444"   },
	{ mlc_yuvfmt_yuyv, "YUYV"     },
};

static const char *hw_format_name(unsigned int format, int bpp);

static void print_mlc_rgb(int module, int layer, struct mlcrgblayer *r)
{
	struct mlc_reg reg;

	if (r == NULL) {
		hw_mlc_dump(module, &reg);
		r = &reg.rgb[layer];
	}

	fprintf(stdout, "MLC.%d - RGB.%d\n", module, layer);
	fprintf(stdout, " leftright                : 0x%08x\n",
		r->mlcleftright);
	fprintf(stdout, " topbottom                : 0x%08x\n",
		r->mlctopbottom);
	fprintf(stdout, " invalidleftright0        : 0x%08x\n",
		r->mlcinvalidleftright0);
	fprintf(stdout, " invalidtopbottom0        : 0x%08x\n",
		r->mlcinvalidtopbottom0);
	fprintf(stdout, " invalidleftright1        : 0x%08x\n",
		r->mlcinvalidleftright1);
	fprintf(stdout, " invalidtopbottom1        : 0x%08x\n",
		r->mlcinvalidtopbottom1);
	fprintf(stdout, " control                  : 0x%08x\n", r->mlccontrol);
	fprintf(stdout, " hstride                  : 0x%08x\n", r->mlchstride);
	fprintf(stdout, " vstride                  : 0x%08x\n", r->mlcvstride);
	fprintf(stdout, " tpcolor                  : 0x%08x\n", r->mlctpcolor);
	fprintf(stdout, " invcolor                 : 0x%08x\n", r->mlcinvcolor);
	fprintf(stdout, " address                  : 0x%08x\n", r->mlcaddress);
}

static void print_mlc_yuv(int module, struct mlcyuvlayer *r)
{
	struct mlc_reg reg;

	if (r == NULL) {
		hw_mlc_dump(module, &reg);
		r = &reg.yuv;
	}

	fprintf(stdout, "MLC.%d - YUV\n", module);
	fprintf(stdout, " leftright                : 0x%08x\n",
		r->mlcleftright);
	fprintf(stdout, " topbottom                : 0x%08x\n",
		r->mlctopbottom);
	fprintf(stdout, " control                  : 0x%08x\n", r->mlccontrol);
	fprintf(stdout, " vstride                  : 0x%08x\n", r->mlcvstride);
	fprintf(stdout, " tpcolor                  : 0x%08x\n", r->mlctpcolor);
	fprintf(stdout, " invcolor                 : 0x%08x\n", r->mlcinvcolor);
	fprintf(stdout, " address                  : 0x%08x\n", r->mlcaddress);
	fprintf(stdout, " addresscb                : 0x%08x\n",
		r->mlcaddresscb);
	fprintf(stdout, " addresscr                : 0x%08x\n",
		r->mlcaddresscr);
	fprintf(stdout, " vstridecb                : 0x%08x\n",
		r->mlcvstridecb);
	fprintf(stdout, " vstridecr                : 0x%08x\n",
		r->mlcvstridecr);
	fprintf(stdout, " hscale                   : 0x%08x\n", r->mlchscale);
	fprintf(stdout, " vscale                   : 0x%08x\n", r->mlcvscale);

	fprintf(stdout, " uenh                     : 0x%08x\n", r->mlcluenh);
	fprintf(stdout, " chenh[0]                 : 0x%08x\n", r->mlcchenh[0]);
	fprintf(stdout, " chenh[1]                 : 0x%08x\n", r->mlcchenh[1]);
	fprintf(stdout, " chenh[2]                 : 0x%08x\n", r->mlcchenh[2]);
	fprintf(stdout, " chenh[3]                 : 0x%08x\n", r->mlcchenh[2]);
}

static void print_mlc(int module, struct mlc_reg *r)
{
	struct mlc_reg reg;
	int i;

	if (r == NULL) {
		hw_mlc_dump(module, &reg);
		r = &reg;
	}

	fprintf(stdout, "MLC.%d\n", module);
	fprintf(stdout, " controlt                 : 0x%08x\n", r->mlccontrolt);
	fprintf(stdout, " screensize               : 0x%08x\n",
		r->mlcscreensize);
	fprintf(stdout, " bgcolor                  : 0x%08x\n", r->mlcbgcolor);

	for (i = 0; i < 2; i++)
		print_mlc_rgb(module, i, &r->rgb[i]);

	print_mlc_yuv(module, &r->yuv);

	fprintf(stdout, "MLC.%d Gamma\n", module);
	fprintf(stdout, " paletetable2             : 0x%08x\n",
		r->mlcpaletetable2);
	fprintf(stdout, " gammacont                : 0x%08x\n",
		r->mlcgammacont);
	fprintf(stdout, " rgammatablewrite         : 0x%08x\n",
		r->mlcrgammatablewrite);
	fprintf(stdout, " ggammatablewrite         : 0x%08x\n",
		r->mlcggammatablewrite);
	fprintf(stdout, " bgammatablewrite         : 0x%08x\n",
		r->mlcbgammatablewrite);
	fprintf(stdout, " yuvlayergammatable_red   : 0x%08x\n",
		r->yuvlayergammatable_red);
	fprintf(stdout, " yuvlayergammatable_green : 0x%08x\n",
		r->yuvlayergammatable_green);
	fprintf(stdout, " yuvlayergammatable_blue  : 0x%08x\n",
		r->yuvlayergammatable_blue);
	fprintf(stdout, "\n\n");
	fprintf(stdout, "TOP\n");
	fprintf(stdout, " pwr:%-3s, prior:%d, mlc:%-3s, field:%-3s\n",
		_getbits(r->mlccontrolt, 10, 2) == 0x3 ? "ON" : "OFF",
		_getbits(r->mlccontrolt,  8, 2),
		_getbits(r->mlccontrolt,  1, 1) ? "ON" : "OFF",
		_getbits(r->mlccontrolt,  0, 1) ? "interalace" : "progressive");
	fprintf(stdout, " screen:%dx%d, bgcolor:0x%x\n",
		_getbits(r->mlcscreensize,  0, 12),
		_getbits(r->mlcscreensize, 16, 12),
		_getbits(r->mlcbgcolor, 0, 24));

	for (i = 0; i < 2; i++) {
		fprintf(stdout, "RGB.%d\n", i);
		fprintf(stdout,
			" %-3s, %d x %d (l:%d, t:%d, r:%d, b:%d), stride:%d/%d, %dbpp, %s(0x%x)\n",
			_getbits(r->rgb[i].mlccontrol, 5, 1) ? "ON" : "OFF",
			_getbits(r->rgb[i].mlcleftright,  0, 11) - 
				_getbits(r->rgb[i].mlcleftright, 16, 11) + 1,
			_getbits(r->rgb[i].mlctopbottom,  0, 11) - 
				_getbits(r->rgb[i].mlctopbottom, 16, 11) + 1,
			_getbits(r->rgb[i].mlcleftright, 16, 11),
			_getbits(r->rgb[i].mlctopbottom, 16, 11),
			_getbits(r->rgb[i].mlcleftright,  0, 11),
			_getbits(r->rgb[i].mlctopbottom,  0, 11),
			r->rgb[i].mlchstride, r->rgb[i].mlcvstride,
			r->rgb[i].mlchstride * 8,
			hw_format_name(r->rgb[i].mlccontrol & _maskbit(16, 16),
				       r->rgb[i].mlchstride * 8),
			r->rgb[i].mlccontrol & _maskbit(16, 16));
		fprintf(stdout, " blend:%-3s, invcolor:%-3s, tpcolor:%-3s\n",
			_getbits(r->rgb[i].mlccontrol, 2, 1) ? "ON" : "OFF",
			_getbits(r->rgb[i].mlccontrol, 1, 1) ? "ON" : "OFF",
			_getbits(r->rgb[i].mlccontrol, 0, 1) ? "ON" : "OFF");
		if ((module == 1) && (i == 1))
			break;
	}
	fprintf(stdout, "YUV\n");
	fprintf(stdout, " %-3s, %d x %d (l:%d, t:%d, r:%d, b:%d), %s(0x%x)\n",
		_getbits(r->yuv.mlccontrol, 5, 1) ? "ON" : "OFF",
		_getbits(r->yuv.mlcleftright,  0, 11) -
			_getbits(r->yuv.mlcleftright, 16, 11) + 1,
		_getbits(r->yuv.mlctopbottom,  0, 11) -
			_getbits(r->yuv.mlctopbottom, 16, 11) + 1,
		_getbits(r->yuv.mlcleftright, 16, 11),
		_getbits(r->yuv.mlctopbottom, 16, 11),
		_getbits(r->yuv.mlcleftright,  0, 11),
		_getbits(r->yuv.mlctopbottom,  0, 11),
		hw_format_name(r->yuv.mlccontrol & _maskbit(16, 2), 8),
		r->yuv.mlccontrol & _maskbit(16, 16));
	fprintf(stdout,
		" stride:%d/%d/%d, scale:%d,%d, %d x %d -> %d x %d, hvfilter: %-3s/%-3s\n",
		r->yuv.mlcvstride, r->yuv.mlcvstridecb, r->yuv.mlcvstridecr,
		r->yuv.mlchscale, r->yuv.mlcvscale,
		_getbits(r->yuv.mlchscale, 0, 23) * (_getbits(r->yuv.mlcleftright, 0, 11) -
			_getbits(r->yuv.mlcleftright, 16, 11) + 1) / MLC_YUV_SCALE_CONSTANT,
		_getbits(r->yuv.mlcvscale, 0, 23) * (_getbits(r->yuv.mlctopbottom, 0, 11) -
			_getbits(r->yuv.mlctopbottom, 16, 11) + 1) / MLC_YUV_SCALE_CONSTANT,
		_getbits(r->yuv.mlcleftright, 0, 11) -
			_getbits(r->yuv.mlcleftright, 16, 11) + 1,
		_getbits(r->yuv.mlctopbottom, 0, 11) -
			_getbits(r->yuv.mlctopbottom, 16, 11) + 1,
		_getbits(r->yuv.mlchscale, 28, 2) == 0x3 ? " ON" : "OFF",
		_getbits(r->yuv.mlcvscale, 28, 2) == 0x3 ? " ON" : "OFF");
	fprintf(stdout, " blend:%-3s, invcolor:%-3s, tpcolor:%-3s\n",
		_getbits(r->yuv.mlccontrol, 2, 1) ? "ON" : "OFF",
		_getbits(r->yuv.mlccontrol, 1, 1) ? "ON" : "OFF",
		_getbits(r->yuv.mlccontrol, 0, 1) ? "ON" : "OFF");
	fprintf(stdout, " luminance contrast:%d, bright:%d\n",
		_getbits(r->yuv.mlcluenh, 0, 3),
		_getbits(r->yuv.mlcluenh, 8, 8));
	fprintf(stdout, " chrominamce 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		r->yuv.mlcchenh[0], r->yuv.mlcchenh[1],
		r->yuv.mlcchenh[2], r->yuv.mlcchenh[3]);
	fprintf(stdout, "Gamma\n");
	fprintf(stdout,
		" TABLE R:%-3s, G:%-3s, B:%-3s, Region[alpha:%-3s, yuv:%-3s, rgb:%-3s] Dither:%-3s\n",
		_getbits(r->mlcgammacont,  2, 2) == 3 ? "ON" : "OFF",
		_getbits(r->mlcgammacont,  8, 2) == 3 ? "ON" : "OFF",
		_getbits(r->mlcgammacont, 10, 2) == 3 ? "ON" : "OFF",
		_getbits(r->mlcgammacont,  5, 1) ? "YUV": "RGB",
		_getbits(r->mlcgammacont,  4, 1) ? "ON" : "OFF",
		_getbits(r->mlcgammacont,  1, 1) ? "ON" : "OFF",
		_getbits(r->mlcgammacont,  0, 1) ? "ON" : "OFF");
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

	for (i = 0; i < (int)ARRAY_SIZE(mlc_format_name); i++)
		if (format == mlc_format_name[i].format)
			return mlc_format_name[i].name;

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

static unsigned int format_convert_yuv(unsigned int format,
				       unsigned int *fourcc)
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

static int get_drm_ids(struct op_arg *op, struct device *dev)
{
	struct plane_opt *p = &op->plane;
	int counts = 0;
	int i;

	p->mode_index = 0;

	/* connector id */
	for (i = 0; i < (int)dev->resources->res->count_connectors; i++) {
		drmModeConnector *connector = dev->resources->connectors[i].connector;
		if (i == op->module) {
			if (!connector)
				return -EINVAL;

			p->connector_id = connector->connector_id;
			p->encoder_id = connector->encoder_id;
			break;
		}

	}

	/* crtc id */
	for (i = 0; i < dev->resources->res->count_crtcs; i++) {
		struct crtc *_crtc = &dev->resources->crtcs[i];
		drmModeCrtc *crtc = _crtc->crtc;

		if (i == op->module) {
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

		if (!(plane->possible_crtcs & (1 << op->module)))
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

static int format_to_fourcc(struct op_arg *op, struct raw_header *header)
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

static int set_plane_rect(struct op_arg *op, struct raw_header *header)
{
	struct plane_opt *p = &op->plane;
	int x, y;

	if (op->layer == mlc_layer_video) {
		struct mlcyuvlayer *r = &header->mlc.yuv;
		unsigned int format = r->mlccontrol & _maskbit(16, 3);
		int div = format == mlc_yuvfmt_yuyv ? 2 : 1;
		int w, h;

		x = _getbits(r->mlcleftright, 16, 12);
		y = _getbits(r->mlctopbottom, 16, 12);
		/* Note. get width with scale factor */
		w = _getbits(r->mlcleftright, 0, 11) - _getbits(r->mlcleftright,
								16, 11) + 1;
		h = _getbits(r->mlctopbottom, 0, 11) - _getbits(r->mlctopbottom,
								16, 11) + 1;

		/* Note. get width with stride for the aligned image */
#if 0
		p->src_w =
			_getbits(r->mlchscale, 0,
				 23) * w / MLC_YUV_SCALE_CONSTANT;
#endif
		p->src_x = 0, p->src_y = 0;
		p->src_w = r->mlcvstride / div;
		p->src_h =
			_getbits(r->mlcvscale, 0,
				 23) * h / MLC_YUV_SCALE_CONSTANT;

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

static int set_mlc_property(struct op_arg *op, struct mlc_reg *reg)
{
	struct mlc_reg *r = (struct mlc_reg *)op->mem;
	int i;

	/* top */
	/* - priority */
	writel_bits(reg->mlccontrolt, &r->mlccontrolt, 8, 2);
	writel(reg->mlcbgcolor, &r->mlcbgcolor);

	/* rgb0/1 */
	/* - blend, invcolor, tpcolor */
	for (i = 0; i < 2; i++) {
		if (!readl_bits(&r->rgb[i].mlccontrol, 5, 1))
				continue;
		writel_bits(reg->rgb[i].mlccontrol, &r->rgb[i].mlccontrol, 0, 7);
		writel(reg->rgb[i].mlctpcolor, &r->rgb[i].mlctpcolor);
		writel(reg->rgb[i].mlcinvcolor, &r->rgb[i].mlcinvcolor);
		writel_bits(1, &r->rgb[i].mlccontrol, 4, 1);
	}

	/* video */
	/* - blend */
	if (readl_bits(&r->yuv.mlccontrol, 5, 1)) {
		writel_bits(reg->yuv.mlccontrol, &r->yuv.mlccontrol, 2, 1);
		writel(reg->yuv.mlcinvcolor, &r->yuv.mlcinvcolor);
		writel(reg->yuv.mlcluenh, &r->yuv.mlcluenh);
		writel(reg->yuv.mlcchenh[0], &r->yuv.mlcchenh[0]);
		writel(reg->yuv.mlcchenh[1], &r->yuv.mlcchenh[1]);
		writel(reg->yuv.mlcchenh[2], &r->yuv.mlcchenh[2]);
		writel(reg->yuv.mlcchenh[3], &r->yuv.mlcchenh[3]);
		writel_bits(1, &r->yuv.mlccontrol, 4, 1);
	}

	/* gamma */
	if (op->flags & FLAG_GAMMAN_OFF)
		writel(0, &r->mlcgammacont);

	return 0;
}

static int set_plane_image(struct op_arg *op, struct raw_header *header)
{
	struct util_image_info *image = &op->plane.image;

	image->file = op->file;
	image->offset = RAW_HEADER_SIZE;
	image->type = UTIL_IMAGE_RAW;

	return 0;
}

static drmModeModeInfo *find_crtc_and_mode(struct device *dev,
						int connector_id, int index,
						int vrefresh)
{
	drmModeModeInfo *mode = drm_connector_find_mode(dev,
					connector_id, index, vrefresh);
	if (mode == NULL) {
		fprintf(stderr,
			"failed to find mode index.%d for connector.%d\n",
			index, connector_id);
		return NULL;
	}

	return mode;
}

static int drm_set_crtc(struct device *dev, struct plane_opt *p,
			struct crtc *crtc)
{
	int ret = 0;
	drmModeModeInfo *mode = find_crtc_and_mode(dev,
					p->connector_id, p->mode_index, 0);

	if (p->encoder_id == 0) {
		fprintf(stdout, "set crtc.%d. connector.%d\n",
				crtc->crtc->crtc_id, p->connector_id);

		ret = drmModeSetCrtc(dev->fd, crtc->crtc->crtc_id,
				p->fb_id, 0, 0, &p->connector_id, 1, mode);
		if (ret) {
			fprintf(stderr, "failed to set mode: %s\n", strerror(errno));
			return -1;
		}
		 /* XXX: Actually check if this is needed */
		drmModeDirtyFB(dev->fd, p->fb_id, NULL, 0);
	}

	return 0;
}

static int drm_set_plane(struct device *dev, struct plane_opt *p)
{
	drmModePlane *ovr = NULL;
	unsigned int handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	unsigned int plane_id;
	struct bo *plane_bo;
	unsigned int plane_flags = 0;
	int crtc_x, crtc_y, crtc_w, crtc_h;
	struct crtc *crtc = NULL;
	unsigned int pipe;
	unsigned int i;
	int ret;

	/* Find an unused plane which can be connected to our CRTC. Find the
	 * CRTC index first, then iterate over available planes.
	 */
	for (i = 0; i < (unsigned int)dev->resources->res->count_crtcs; i++)
		if (p->crtc_id == dev->resources->res->crtcs[i]) {
			crtc = &dev->resources->crtcs[i];
			pipe = i;
			break;
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

		if (plane_id && (plane_id != ovr->plane_id))
			continue;

		if (!drm_format_support(ovr, p->fourcc))
			continue;

		if ((ovr->possible_crtcs & (1 << pipe)) &&
		    ((ovr->crtc_id == 0) || (ovr->crtc_id == p->crtc_id))) {
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
		p->src_x, p->src_y, p->src_w, p->src_h,
		util_format_name(p->fourcc), plane_id);

	plane_bo = util_bo_create_image(dev->fd, p->fourcc, p->src_w, p->src_h,
					handles,
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

	ret = drm_set_crtc(dev, p, crtc);
	if (ret) {
		fprintf(stderr, "failed to set crt !!!\n");
		return -1;
	}

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
	const char *mode = op->mode == op_mode_capture ? "ab" : "rb";
	void *addr, *mem, *mapped = NULL;
	int x, y, width, height, linestride, size;
	unsigned int format;
	int bpp, i;

	if ((fp == NULL) || (reg == NULL))
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

	fprintf(stdout,
		"get mlc.%d, rgb.%d %s(0x%x), %d,%d, %d x %d, line: %d, size: %dbyte\n",
		op->module, op->layer, hw_format_name(format, bpp),
		format, x, y, width, height, linestride, size);

	mem = iomem_map(addr, size, mapped);
	if (mem == NULL) {
		fprintf(stderr, "Fail, module %d, map %p\n",
			op->module, addr);
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
	int div = 1, i, ret;

	if ((fp == NULL) || (reg == NULL))
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
	height =
		_getbits(reg->mlcvscale, 0,
			 23) * height / MLC_YUV_SCALE_CONSTANT;
	x = _getbits(reg->mlcleftright, 16, 12);
	y = _getbits(reg->mlctopbottom, 16, 12);

	fprintf(stdout,
		"get mlc.%d, yuv.%d %s(0x%x), %d,%d, %d x %d, line: %d\n",
		op->module, op->layer, hw_format_name(format, 8),
		format, x, y, width, height, reg->mlcvstride);
	fprintf(stdout, "get mlc.%d, yuv.%d scale(h:%s,%x, v:%s(%x), %d x %d\n",
		op->module, op->layer,
		_getbits(reg->mlchscale, 28, 2) == 3 ? "on" : "off",
		_getbits(reg->mlchscale, 28, 2),
		_getbits(reg->mlcvscale, 28, 2) == 3 ? "on" : "off",
		_getbits(reg->mlcvscale, 28, 2),
		_getbits(reg->mlchscale, 0, 23),
		_getbits(reg->mlcvscale, 0, 23));

	for (i = 0, div = 1; i < 3; i++, div = 1) {
		int n;

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
			i, stride, height / div, size[i]);

		mem[i] = iomem_map(addr, size[i], mapped[i]);
		if (mem[i] == NULL) {
			fprintf(stderr, "Fail, module %d, map %p\n",
				op->module, addr);
			ret = -EINVAL;
			goto __exit;
		}
		fprintf(stdout, "- map %p -> %p %dbyte\n", addr, mem[i],
			size[i]);

		for (n = 0; n < (height / div); n++) {
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

static int raw_image_header(struct op_arg *op, struct raw_header *header)
{
	const char sign[4] = RAW_HEADER_SIGN;
	const char *mode = op->mode == op_mode_capture ? "wb" : "rb";
	FILE *fp = NULL;

	fp = fopen(op->file, mode);
	if (!fp) {
		fprintf(stderr, "Error file %s\n", op->file);
		perror("- error");
		return errno;
	}

	op->fp = fp;

	if (op->mode == op_mode_capture) {
		header->sign[0] = sign[0];
		header->sign[1] = sign[1];
		header->sign[2] = sign[2];
		header->sign[3] = sign[3];

		header->module = op->module;
		header->layer = op->layer;

		hw_mlc_dump(op->module, &header->mlc);

		fwrite((void *)header, 1, RAW_HEADER_SIZE, fp);
	} else {
		fread((void *)header, 1, RAW_HEADER_SIZE, fp);
		if (strncmp(header->sign, sign, 4)) {
			fprintf(stderr, "Not found signature !!!\n");
			return -EINVAL;
		}
		op->module = header->module;
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

	ret = raw_image_header(op, header);
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

	ret = raw_image_header(op, header);
	if (ret)
		goto __exit_update;

	dev.resources = drm_get_resources(&dev);
	if (!dev.resources) {
		ret = -EINVAL;
		goto __exit_update;
	}

	ret = get_drm_ids(op, &dev);
	if (ret)
		goto __exit_update;

	fprintf(stdout, "set mlc.%d layer.%d -> crt.%d plane.%d\n",
		op->module, op->layer,
		op->plane.crtc_id, op->plane.plane_id);

	ret = format_to_fourcc(op, header);
	if (ret)
		goto __exit_update;

	ret = set_plane_rect(op, header);
	if (ret)
		goto __exit_update;

	fprintf(stdout, "src %d,%d, %d x %d %dbpp, %s(0x%x) - %s(0x%x)\n",
		op->plane.src_x, op->plane.src_y, op->plane.src_w,
		op->plane.src_h,
		op->bpp, hw_format_name(op->hw_format, op->bpp),
		op->hw_format,
		util_format_name(op->plane.fourcc),
		op->plane.fourcc);

	ret = set_plane_image(op, header);
	if (ret)
		goto __exit_update;

	drm_set_plane(&dev, p);

	set_mlc_property(op, &header->mlc);

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
		print_mlc_rgb(op->module, op->layer, NULL);
		break;
	case mlc_layer_video:
		print_mlc_yuv(op->module, NULL);
		break;
	case mlc_layer_unknown:
	default:
		print_mlc(op->module, NULL);
		break;
	}
	return 0;
}

static int parse_file(struct op_arg *op)
{
	struct raw_header *header = NULL;
	int ret;

	header = (struct raw_header *)malloc(RAW_HEADER_SIZE);
	if (!header) {
		fprintf(stderr, "memory allocation failed\n");
		return -ENOMEM;
	}
	memset(header, 0, RAW_HEADER_SIZE);

	fprintf(stdout, "FILE  - %s\n", op->file);

	op->mode = op_mode_print;
	ret = raw_image_header(op, header);
	if (ret) {
		free(header);
		return -EINVAL;
	}
	fprintf(stdout, "MLC.%d - Layer.%d\n\n", op->module, op->layer);

	print_mlc(op->module, &header->mlc);

	free(header);

	return 0;
}

static int parse_arg(char *arg, struct op_arg *op)
{
	char *end;

	op->module = strtoul(arg, &end, 10);
	if (*end != ',')
		return op->mode == op_mode_print ? 0 : -EINVAL;

	arg = end +  1;
	op->layer = strtoul(arg, &end, 10);
	if (*end != ',')
		return op->mode == op_mode_print ? 0 : -EINVAL;

	arg = end +  1;
	op->file = arg;

	return 0;
}

static void usage(char *name)
{
	fprintf(stdout, "usage: %s\n", name);
	fprintf(stdout, "\n Options:\n");
	fprintf(stdout,
		"\t-c <dev>,<layer>,<file>\tcapture <dev>'s <layer> to <file>\n");
	fprintf(stdout, "\t-s <file>\t\tstore <file> with header info\n");
	fprintf(stdout,
		"\t-p <dev>,<layer>\tprint <dev> and <layer>'s hw register\n");
	fprintf(stdout, "\t-i <file>\t\tprint <file>'s hw register\n");
	fprintf(stdout, "\t-g \t\tdisable gamma\n");
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

	op = malloc(sizeof(*op));
	if (op == NULL) {
		fprintf(stderr, "memory allocation failed\n");
		return -1;
	}
	memset(op, 0, sizeof(*op));
	op->layer = mlc_layer_unknown;

	while (-1 != (opt = getopt(argc, argv, "hc:s:p:i:g")))
		switch (opt) {
		case 'c':
			op->mode = op_mode_capture;
			ret = parse_arg(optarg, op);
			break;
		case 's':
			op->mode = op_mode_update;
			op->file = optarg;
			ret = 0;
			break;
		case 'p':
			op->mode = op_mode_print;
			ret = parse_arg(optarg, op);
			break;
		case 'i':
			op->file = optarg;
			ret = parse_file(op);
			if (!ret)
				return 0;
			break;
		case 'g':
			op->flags |= FLAG_GAMMAN_OFF;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
		default:
			break;
		}

	if (ret) {
		usage(argv[0]);
		goto __exit;
	}

	addr = hw_mlc_get_base(op->module);
	if (addr == NULL) {
		fprintf(stderr, "Fail, not support module.%d\n",
			op->module);
		ret = -EINVAL;
		goto __exit;
	}
	size = hw_mlc_get_size(op->module);

	mem = iomem_map(addr, size, NULL);
	if (mem == NULL) {
		fprintf(stderr, "Fail, module %d, map %p\n",
			op->module, addr);
		ret = -EINVAL;
		goto __exit;
	}
	hw_mlc_set_base(op->module, mem);

	op->addr = addr;
	op->mem = mem;

	fprintf(stdout, "reg mlc %p -> %p %dbyte\n", addr, mem, size);

	switch (op->mode) {
	case op_mode_print:
		ret = print_device(op);
		break;
	case op_mode_capture:
		ret = capture_device(op);
		break;
	case op_mode_update:
		ret = update_device(op);
		break;
	}
__exit:
	iomem_free(mem, size);
	free(op);

	return ret;
}
