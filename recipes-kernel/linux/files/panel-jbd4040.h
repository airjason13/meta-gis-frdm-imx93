/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 GIS / JBD
 * JBD4040 MicroLED Panel Driver Header (MIPI Command Mode)
 */

#ifndef __PANEL_JBD4040_H__
#define __PANEL_JBD4040_H__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <video/mipi_display.h>

/* JBD4040 I2C slave addresses */
#define JBD4040_I2C_ADDR_ALL    0x58  /* Broadcast address */
#define JBD4040_I2C_ADDR_RED    0x59  /* Red panel */
#define JBD4040_I2C_ADDR_GREEN  0x5A  /* Green panel */
#define JBD4040_I2C_ADDR_BLUE   0x5B  /* Blue panel */

/* Register Definitions */
#define REG_CHIP_ID0            0x200000
#define REG_CHIP_ID1            0x200002
#define REG_SOFT_RESET          0x200004
#define REG_PL_CUR              0x200100
#define REG_GAM_CFG             0x200200
#define REG_DMR_CFG             0x200202
#define REG_DMR_REMAP_VAL       0x200204
#define REG_PL_SELF_TEST_CFG    0x200300
#define REG_IMG_FLIP            0x20020e
#define REG_PL_CLEAR            0x200304
#define REG_PVT_CTL             0x200402
#define REG_PVT_DATA            0x200404
#define REG_FLAG_MASK           0x200500
#define REG_PL_CFG              0x200a00
#define REG_DISP_CTL            0x200a02
#define REG_PL_SYNC             0x200a04
#define REG_LUM_REG             0x200a14
#define REG_GLB_SCAN_VAL        0x200a1a
#define REG_IMG_XS_ADDR         0x200a1c
#define REG_IMG_XE_ADDR         0x200a1e
#define REG_IMG_YS_ADDR         0x200a20
#define REG_IMG_YE_ADDR         0x200a22
#define REG_IMG_OFFSET          0x200a24
#define REG_DCS_EN              0x200a34
#define REG_VID_MODE            0x200b06
#define REG_CORE_RSTN           0x201004
#define REG_CFG_EOTP            0x20100c
#define REG_CFG_PKT_VALID_VC    0x201028
#define REG_CFG_PKT_CLK_MGR     0x20102c
#define REG_CFG_VID_TX_DELAY    0x201038
#define REG_DPHY_REG00          0x202000
#define REG_DPHY_REG18          0x2021e0
#define REG_DPHY_REG1D          0x2021f4
#define REG_EFUSE_REG_SW        0x200d30
#define REG_OSC_TRIM            0x200d3a

#define JBD4040_GAMMA_TABLE_0_ADDR 0x250000

#define JBD4040_DEV_NAME        "jbd4040"
#define JBD4040_CLASS_NAME      "gis"
#define DEF_I2C_JBD4040_MAX_ARGS 16

/* IOCTL Definitions matching jbd4040_reg userspace tool */
struct i2c_ioctl_data {
	uint32_t reg;
	uint32_t val;
};

#define IOCTL_READ_REG32    _IOR('J', 0xF1, struct i2c_ioctl_data)
#define IOCTL_READ_REG16    _IOR('J', 0xF2, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG32   _IOW('J', 0xF3, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG16   _IOW('J', 0xF4, struct i2c_ioctl_data)

/* Gamma 2.2 Table (10-bit precision, 256 entries, max 1023) */
static const uint16_t jbd4040_gamma_2_2_table[256] = {
       0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    1,    1,    1,    2,    2,
       2,    3,    3,    3,    4,    4,    5,    5,    6,    6,    7,    7,    8,    9,    9,   10,
      11,   11,   12,   13,   14,   15,   16,   16,   17,   18,   19,   20,   21,   23,   24,   25,
      26,   27,   28,   30,   31,   32,   34,   35,   36,   38,   39,   41,   42,   44,   46,   47,
      49,   51,   52,   54,   56,   58,   60,   61,   63,   65,   67,   69,   71,   73,   76,   78,
      80,   82,   84,   87,   89,   91,   94,   96,   98,  101,  103,  106,  109,  111,  114,  117,
     119,  122,  125,  128,  130,  133,  136,  139,  142,  145,  148,  151,  155,  158,  161,  164,
     167,  171,  174,  177,  181,  184,  188,  191,  195,  198,  202,  206,  209,  213,  217,  221,
     225,  228,  232,  236,  240,  244,  248,  252,  257,  261,  265,  269,  274,  278,  282,  287,
     291,  295,  300,  304,  309,  314,  318,  323,  328,  333,  337,  342,  347,  352,  357,  362,
     367,  372,  377,  382,  387,  393,  398,  403,  408,  414,  419,  425,  430,  436,  441,  447,
     452,  458,  464,  470,  475,  481,  487,  493,  499,  505,  511,  517,  523,  529,  535,  542,
     548,  554,  561,  567,  573,  580,  586,  593,  599,  606,  613,  619,  626,  633,  640,  647,
     653,  660,  667,  674,  681,  689,  696,  703,  710,  717,  725,  732,  739,  747,  754,  762,
     769,  777,  784,  792,  800,  807,  815,  823,  831,  839,  847,  855,  863,  871,  879,  887,
     895,  903,  912,  920,  928,  937,  945,  954,  962,  971,  979,  988,  997, 1005, 1014, 1023,
};

/* Gamma 1.0 Linear Table (10-bit precision, 256 entries, max 1023) */
static const uint16_t jbd4040_gamma_1_0_table[256] = {
       0,    4,    8,   12,   16,   20,   24,   28,   32,   36,   40,   44,   48,   52,   56,   60,
      64,   68,   72,   76,   80,   84,   88,   92,   96,  100,  104,  108,  112,  116,  120,  124,
     128,  132,  136,  140,  144,  148,  152,  156,  160,  164,  168,  173,  177,  181,  185,  189,
     193,  197,  201,  205,  209,  213,  217,  221,  225,  229,  233,  237,  241,  245,  249,  253,
     257,  261,  265,  269,  273,  277,  281,  285,  289,  293,  297,  301,  305,  309,  313,  317,
     321,  325,  329,  333,  337,  341,  345,  349,  353,  357,  361,  365,  369,  373,  377,  381,
     385,  389,  393,  397,  401,  405,  409,  413,  417,  421,  425,  429,  433,  437,  441,  445,
     449,  453,  457,  461,  465,  469,  473,  477,  481,  485,  489,  493,  497,  501,  505,  509,
     514,  518,  522,  526,  530,  534,  538,  542,  546,  550,  554,  558,  562,  566,  570,  574,
     578,  582,  586,  590,  594,  598,  602,  606,  610,  614,  618,  622,  626,  630,  634,  638,
     642,  646,  650,  654,  658,  662,  666,  670,  674,  678,  682,  686,  690,  694,  698,  702,
     706,  710,  714,  718,  722,  726,  730,  734,  738,  742,  746,  750,  754,  758,  762,  766,
     770,  774,  778,  782,  786,  790,  794,  798,  802,  806,  810,  814,  818,  822,  826,  830,
     834,  838,  842,  846,  850,  855,  859,  863,  867,  871,  875,  879,  883,  887,  891,  895,
     899,  903,  907,  911,  915,  919,  923,  927,  931,  935,  939,  943,  947,  951,  955,  959,
     963,  967,  971,  975,  979,  983,  987,  991,  995,  999, 1003, 1007, 1011, 1015, 1019, 1023,
};

struct jbd4040_panel_info {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	/* GPIO power supplies */
	struct gpio_desc *avdd_gpio;
	struct gpio_desc *avee_gpio;
	struct gpio_desc *reset_gpio;

	/* I2C adapter for direct transfer */
	struct i2c_adapter *i2c_adap;
	struct i2c_client *i2c_client; /* 2-0058 */

	bool prepared;
	bool enabled;
	bool bist;
	bool vid_mode;
	uint8_t gamma_val; /* 0: disabled, 10: 1.0, 22: 2.2 */
};

#endif /* __PANEL_JBD4040_H__ */
