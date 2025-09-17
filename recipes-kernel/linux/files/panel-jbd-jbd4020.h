// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024, JBD
 */

#ifndef __PANEL_JBD_JBD4020_H__
#define __PANEL_JBD_JBD4020_H__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/i2c.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define DSI_CMD_TRANSFER 0	// 1:MIPI_DSI 0:I2C
#define GPIO_SET_TYPE 1		// CC: was 0

#define JBD4020_I2C_ADDR 0x58
#define JBD4020_I2C_R_ADDR 0x59
#define JBD4020_I2C_G_ADDR 0x5a
#define JBD4020_I2C_B_ADDR 0x5b
#define JBD4020_REG_DBV    0x0302AE38

#define JBD4020_COMMAND_INSTR(_reg, _data)		\
	{						\
		.reg = (_reg),		\
		.data = (_data),	\
	}


typedef enum {
    JBD_PANEL_R = 0,
    JBD_PANEL_G,
    JBD_PANEL_B,
    JBD_PANEL_ALL,
} panel_id_t;

struct jbd4020_instr_t {
	uint32_t	reg;
	uint32_t	data;
};

struct jbd4020_desc_t {
	const struct jbd4020_instr_t *init;
	const size_t init_length;
	const struct drm_display_mode *mode;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};

struct jbd4020_panel_info_t {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct jbd4020_desc_t *desc;

#if GPIO_SET_TYPE
	struct gpio_desc *vddi_gpio;
	struct gpio_desc *avdd_gpio;
	struct gpio_desc *avee_gpio;
#else
	int vddi_gpio;
	int avdd_gpio;
	int avee_gpio;
#endif
	bool prepared;
	bool enabled;

	struct i2c_adapter *ddc;
};

struct jbd4020_cfg_t
{
    int offsetV; //Vertical offset
    int offsetH; //Horizontal offset
    int flip;    //Vertical flip
    int mirror;  //Horizontal Flip
    int dbv;     //DBV register
    int cur;     //Current register
};

static const uint8_t jdb_panel_addr[] = {
	JBD4020_I2C_R_ADDR,
	JBD4020_I2C_G_ADDR,
	JBD4020_I2C_B_ADDR
};

static const struct jbd4020_instr_t jbd4020_init[] = {
	// SRAM init
	JBD4020_COMMAND_INSTR(0x03030300, 0x00000010),   // Switch off PWM CLK
	JBD4020_COMMAND_INSTR(0x03030144, 0x40000000),   //Panel initialization
	JBD4020_COMMAND_INSTR(0x030001C8, 0x0029F1FF),  //MUTE set max resolution
	JBD4020_COMMAND_INSTR(0x030001B8, 0x00000000),  //Background gray value set to 0
	JBD4020_COMMAND_INSTR(0x030001C4, 0x00000001),  //MUTE enable
	JBD4020_COMMAND_INSTR(0x03000180, 0x2003d01B),  //Display background color mode
	JBD4020_COMMAND_INSTR(0x03030100, 0x80000011),  //Display port enable
	JBD4020_COMMAND_INSTR(0x03040008, 0x00000001),
	
	JBD4020_COMMAND_INSTR(0xFFFFFFFF, 0x0000001E),	// wait for 30ms

	JBD4020_COMMAND_INSTR(0x0100003C, 0x20021F3F),	//XDP reset
	JBD4020_COMMAND_INSTR(0x0100003C, 0x2002003F),	//XDP reset release

	// Clear MIPI interrupt
	JBD4020_COMMAND_INSTR(0x02006720, 0xFFFFFFFF),
	JBD4020_COMMAND_INSTR(0x02006724, 0xFFFFFFFF),
	JBD4020_COMMAND_INSTR(0x02006728, 0xFFFFFFFF),
	JBD4020_COMMAND_INSTR(0x02006740, 0xFFFFFFFF),	//Error count clear

	//MIPI PCS
	JBD4020_COMMAND_INSTR(0x02007228, 0x000C33FF),
	JBD4020_COMMAND_INSTR(0x02007010, 0x00000004),
	JBD4020_COMMAND_INSTR(0x02007DFC, 0xFFFFFFFF),				 					 
	JBD4020_COMMAND_INSTR(0x020072C0, 0x00000001),
	JBD4020_COMMAND_INSTR(0x020072C0, 0x00000000),
	//MIPI CTRL
	JBD4020_COMMAND_INSTR(0x0200600C, 0x00000001),
	JBD4020_COMMAND_INSTR(0x020060A8, 0x00002263),
	JBD4020_COMMAND_INSTR(0x02006014, 0x00000200),
	JBD4020_COMMAND_INSTR(0x02006200, 0x00000001),
	JBD4020_COMMAND_INSTR(0x02006230, 0x00000000),	//0x00000000:video_mode, 0x00000001:command_mode
	JBD4020_COMMAND_INSTR(0x02006700, 0x001FFFFF),
	JBD4020_COMMAND_INSTR(0x02006704, 0x003FFFFF),
	JBD4020_COMMAND_INSTR(0x02006708, 0x00181BFF),
	JBD4020_COMMAND_INSTR(0x02006160, 0x0001FFFF),
	JBD4020_COMMAND_INSTR(0x02006788, 0x0200001F),
	JBD4020_COMMAND_INSTR(0x02006000, 0x00020002),
	JBD4020_COMMAND_INSTR(0x01000090, 0x00600F86),
	JBD4020_COMMAND_INSTR(0x02006000, 0x00000002),
};

static const struct jbd4020_instr_t jbd4020_init_part2_r[] = {
	//XDP: resolution, refresh , pixel current, algorithm setting
	JBD4020_COMMAND_INSTR(0x03040000, 0x00000103) //9 bit:MIPI signal select 0-video mode;1-commond mode;
												 //14-15 bit：0-R,1-G, 2-B
};

static const struct jbd4020_instr_t jbd4020_init_part2_g[] = {
	//XDP: resolution, refresh , pixel current, algorithm setting
	JBD4020_COMMAND_INSTR(0x03040000, 0x00004103) // G
};

static const struct jbd4020_instr_t jbd4020_init_part2_b[] = {
	//XDP: resolution, refresh , pixel current, algorithm setting
	JBD4020_COMMAND_INSTR(0x03040000, 0x00008103) // B
};

static const struct jbd4020_instr_t jbd4020_init_part3[] = {
	JBD4020_COMMAND_INSTR(0x030000D0, 0x01DF027f),  //480*640
	JBD4020_COMMAND_INSTR(0x03000080, 0x00000100),
	JBD4020_COMMAND_INSTR(0x03030300, 0x00000000),
	JBD4020_COMMAND_INSTR(0x03041004, 0x00000001),
	JBD4020_COMMAND_INSTR(0x0100003C, 0x2243003F),  //54M(60Hz)
	JBD4020_COMMAND_INSTR(0x02000030, 0x0000FF03),  //Current Mirror set 1X											
	JBD4020_COMMAND_INSTR(0x0302AE90, 0x0001005A),  //Pixel Current 1.6uA

	// CRG：DLL output colock
	JBD4020_COMMAND_INSTR(0x010000AC, 0x00080005),
	JBD4020_COMMAND_INSTR(0x010000AC, 0x00080001),
	JBD4020_COMMAND_INSTR(0x010000E4, 0x00080005),
	JBD4020_COMMAND_INSTR(0x010000E4, 0x00080001),
	JBD4020_COMMAND_INSTR(0x010000EC, 0x00080005),
	JBD4020_COMMAND_INSTR(0x010000EC, 0x00080001),
	JBD4020_COMMAND_INSTR(0x010000AC, 0x00080003),
	JBD4020_COMMAND_INSTR(0x010000E4, 0x00080003),
	JBD4020_COMMAND_INSTR(0x010000EC, 0x00080003),
	JBD4020_COMMAND_INSTR(0x01000002, 0x00000000),
	JBD4020_COMMAND_INSTR(0x0100002c, 0x00000000),
	// CORE CTRL：VDD、MVDD、OSCVDD selection
	JBD4020_COMMAND_INSTR(0x02000040, 0x0020C209),
	JBD4020_COMMAND_INSTR(0x02000044, 0x0000448A),
	JBD4020_COMMAND_INSTR(0x02000048, 0x00002088),
	// Auto initialization check enable 
	JBD4020_COMMAND_INSTR(0x02000084, 0x00000002),
	// CMD TOP：DCS&MCS unlock
	JBD4020_COMMAND_INSTR(0x02001020, 0x5a5a5a5a),
	// LTC on   Brightnesss compensation with temperature variations
	JBD4020_COMMAND_INSTR(0x0302AE68, 0x00000001),
	// PMC, Over temperature monitor
	JBD4020_COMMAND_INSTR(0x02003004, 0x0003635B),  //Set the default value
	JBD4020_COMMAND_INSTR(0x02003004, 0x05B1DBA5), // Set the min and max temperature value	
	// Error interruption configuration
	JBD4020_COMMAND_INSTR(0x02000004, 0x00000202),
	JBD4020_COMMAND_INSTR(0x0200501C, 0x00006020),   //FMC transfer overtime interruption
	JBD4020_COMMAND_INSTR(0x03000090, 0xC0000000),   // BCMG, Demura parameters check error interruption
};

static const struct jbd4020_instr_t jbd4020_init_part4_r[] = {
	JBD4020_COMMAND_INSTR(0x03040000, 0x00011103),  // Red. Bit 12 and 16 are used for interruption configurations. They can only be configured once.
};

static const struct jbd4020_instr_t jbd4020_init_part4_g[] = {
	JBD4020_COMMAND_INSTR(0x03040000, 0x00015103),  //Green set
};

static const struct jbd4020_instr_t jbd4020_init_part4_b[] = {
	JBD4020_COMMAND_INSTR(0x03040000, 0x00019103),  //Blue set
};

static const struct jbd4020_instr_t jbd4020_init_part5[] = {
	JBD4020_COMMAND_INSTR(0x02000004, 0x00000200), //ESD interrupt config
	JBD4020_COMMAND_INSTR(0x01001038, 0x00000001), //Error flag IO multiplexing setting

	// OSC trim
	JBD4020_COMMAND_INSTR(0x010031A0, 0x00010000),
	JBD4020_COMMAND_INSTR(0x010031c8, 0x00000164),
    JBD4020_COMMAND_INSTR(0x010031A0, 0x00010000),
    JBD4020_COMMAND_INSTR(0x010031A0, 0x00030000),

	// FMC reset 
	JBD4020_COMMAND_INSTR(0x01001800, 0x00001100),   // Enhance the panel drive capability to flash IO
	JBD4020_COMMAND_INSTR(0x01001804, 0x00001100),
	JBD4020_COMMAND_INSTR(0x01001808, 0x00001100),
	JBD4020_COMMAND_INSTR(0x0100180c, 0x00001100),
	JBD4020_COMMAND_INSTR(0x01001810, 0x00001100),
	JBD4020_COMMAND_INSTR(0x01001814, 0x00001100),
	JBD4020_COMMAND_INSTR(0x01000018, 0x00010000),
	JBD4020_COMMAND_INSTR(0x01000018, 0x00000005),
	JBD4020_COMMAND_INSTR(0x02005000, 0x00000001),
	JBD4020_COMMAND_INSTR(0x02005030, 0x00000030),
	// Flash write enable
	JBD4020_COMMAND_INSTR(0x02005024, 0x00000006),
	JBD4020_COMMAND_INSTR(0x0200503c, 0x00000081),
	// Gamma load
	JBD4020_COMMAND_INSTR(0x0200504c, 0x0302AF00),
	JBD4020_COMMAND_INSTR(0x0200502c, 0x00001000),
	JBD4020_COMMAND_INSTR(0x02005040, 0x00001154),
	JBD4020_COMMAND_INSTR(0x02005068, 0x00030001)
};

static const struct jbd4020_instr_t jbd4020_init_part6[] = {
	//Gamma on
	JBD4020_COMMAND_INSTR(0x0302AE00, 0x00000380),
	JBD4020_COMMAND_INSTR(0x0302AE04, 0x0104101D),  //Dimming off

	//Demura load
	JBD4020_COMMAND_INSTR(0x0200504c, 0x03000400),
	JBD4020_COMMAND_INSTR(0x0200502c, 0x00003000),
	JBD4020_COMMAND_INSTR(0x02005040, 0x00028CB0),
	JBD4020_COMMAND_INSTR(0x02005068, 0x00030001)
};

static const struct jbd4020_instr_t jbd4020_init_part7[] = {
	// Demura
	JBD4020_COMMAND_INSTR(0x030001D0, 0x00201837), //data remapping
	JBD4020_COMMAND_INSTR(0x030001D4, 0x00000730), //data remapping gain
	JBD4020_COMMAND_INSTR(0x03000200, 0x00000019), //Demura on
	JBD4020_COMMAND_INSTR(0x03000204, 0x00000003), //Demura gain

	// Dither on
	JBD4020_COMMAND_INSTR(0x0302C600, 0x001CC880),

	// Random scan
	JBD4020_COMMAND_INSTR(0x0303031C, 0x00000000),
	JBD4020_COMMAND_INSTR(0x03030320, 0x00000000),
	JBD4020_COMMAND_INSTR(0x03030324, 0x00000000),
	JBD4020_COMMAND_INSTR(0x03030328, 0x00000000),
	JBD4020_COMMAND_INSTR(0x0303032C, 0x00000000),

	// state = CFG_END
	JBD4020_COMMAND_INSTR(0x02000070, 0x00000002)
};

static const struct jbd4020_instr_t jbd4020_flash_id_read[] = {
	JBD4020_COMMAND_INSTR(0x02005020, 0xff),
	JBD4020_COMMAND_INSTR(0x02005024, 0x9F),
	JBD4020_COMMAND_INSTR(0x02005030, 0x30),
	JBD4020_COMMAND_INSTR(0x02005038, 0x3),
	JBD4020_COMMAND_INSTR(0x0200503c, 0x85)
};

#endif
