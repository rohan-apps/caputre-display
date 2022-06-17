#ifndef __MLC_REG_H__
#define __MLC_REG_H__

#include <stdint.h>

#define NUMBER_OF_MLC_MODULE            2
#define PHY_BASEADDR_MLC0	        0xC0102000
#define PHY_BASEADDR_MLC1	        0xC0102400
#define MLC_YUV_SCALE_CONSTANT          2048

struct mlc_reg {
	uint32_t mlccontrolt;
	uint32_t mlcscreensize;
	uint32_t mlcbgcolor;

	struct mlcrgblayer {
		uint32_t mlcleftright;
		uint32_t mlctopbottom;
		uint32_t mlcinvalidleftright0;
		uint32_t mlcinvalidtopbottom0;
		uint32_t mlcinvalidleftright1;
		uint32_t mlcinvalidtopbottom1;
		uint32_t mlccontrol;
		int32_t  mlchstride;
		int32_t  mlcvstride;
		uint32_t mlctpcolor;
		uint32_t mlcinvcolor;
		uint32_t mlcaddress;
		uint32_t __reserved0;
	} rgb[2];

	struct mlcyuvlayer {
		uint32_t mlcleftright;
		uint32_t mlctopbottom;
		uint32_t mlccontrol;
		uint32_t mlcvstride;
		uint32_t mlctpcolor;

		uint32_t mlcinvcolor;
		uint32_t mlcaddress;
		uint32_t mlcaddresscb;
		uint32_t mlcaddresscr;
		int32_t  mlcvstridecb;
		int32_t  mlcvstridecr;
		uint32_t mlchscale;
		uint32_t mlcvscale;
		uint32_t mlcluenh;
		uint32_t mlcchenh[4];
	} yuv;

	struct mlcrgblayer2 {
		uint32_t mlcleftright;
		uint32_t mlctopbottom;
		uint32_t mlcinvalidleftright0;
		uint32_t mlcinvalidtopbottom0;
		uint32_t mlcinvalidleftright1;
		uint32_t mlcinvalidtopbottom1;
		uint32_t mlccontrol;
		int32_t  mlchstride;
		int32_t  mlcvstride;
		uint32_t mlctpcolor;
		uint32_t mlcinvcolor;
		uint32_t mlcaddress;
	} rgb2;

	uint32_t mlcpaletetable2;
	uint32_t mlcgammacont;
	uint32_t mlcrgammatablewrite;
	uint32_t mlcggammatablewrite;
	uint32_t mlcbgammatablewrite;
	uint32_t yuvlayergammatable_red;
	uint32_t yuvlayergammatable_green;
	uint32_t yuvlayergammatable_blue;

	uint32_t dimctrl;
	uint32_t dimlut0;
	uint32_t dimlut1;
	uint32_t dimbusyflag;
	uint32_t dimprdarrr0;
	uint32_t dimprdarrr1;
	uint32_t dimram0rddata;
	uint32_t dimram1rddata;
	uint32_t __reserved2[(0x3c0 - 0x12c) / 4];
	uint32_t mlcclkenb;
};

enum mlc_priority {
	mlc_priority_videofirst = 0ul,
	mlc_priority_videosecond = 1ul,
	mlc_priority_videothird = 2ul,
	mlc_priority_videofourth = 3ul
};

enum mlc_format {
        /* RGB */
	mlc_rgbfmt_r5g6b5 = 0x44320000ul,
	mlc_rgbfmt_b5g6r5 = 0xc4320000ul,
	mlc_rgbfmt_x1r5g5b5 = 0x43420000ul,
	mlc_rgbfmt_x1b5g5r5 = 0xc3420000ul,
	mlc_rgbfmt_x4r4g4b4 = 0x42110000ul,
	mlc_rgbfmt_x4b4g4r4 = 0xc2110000ul,
	mlc_rgbfmt_x8r3g3b2 = 0x41200000ul,
	mlc_rgbfmt_x8b3g3r2 = 0xc1200000ul,
	mlc_rgbfmt_a1r5g5b5 = 0x33420000ul,
	mlc_rgbfmt_a1b5g5r5 = 0xb3420000ul,
	mlc_rgbfmt_a4r4g4b4 = 0x22110000ul,
	mlc_rgbfmt_a4b4g4r4 = 0xa2110000ul,
	mlc_rgbfmt_a8r3g3b2 = 0x11200000ul,
	mlc_rgbfmt_a8b3g3r2 = 0x91200000ul,
	mlc_rgbfmt_r8g8b8 = 0x46530000ul,
	mlc_rgbfmt_b8g8r8 = 0xc6530000ul,
	mlc_rgbfmt_x8r8g8b8 = 0x46530001ul,
	mlc_rgbfmt_x8b8g8r8 = 0xc6530001ul,
	mlc_rgbfmt_a8r8g8b8 = 0x06530000ul,
	mlc_rgbfmt_a8b8g8r8 = 0x86530000ul,
        /* video */
	mlc_yuvfmt_420 = 0ul << 16,
	mlc_yuvfmt_422 = 1ul << 16,
	mlc_yuvfmt_444 = 3ul << 16,
	mlc_yuvfmt_yuyv = 2ul << 16,
	mlc_yuvfmt_422_cbcr = 4ul << 16,
	mlc_yuvfmt_420_cbcr = 5ul << 16,
};

enum mlc_layer {
        mlc_layer_rgb0,
        mlc_layer_rgb1,
        mlc_layer_video,
        mlc_layer_unknown,
};

const void *hw_mlc_get_base(int dev);
unsigned int hw_mlc_get_size(int dev);
int hw_mlc_set_base(int dev, void *base);

void hw_mlc_save(int dev, struct mlc_reg *mlc);
void hw_mlc_dump(int dev, struct mlc_reg *mlc);


#endif
