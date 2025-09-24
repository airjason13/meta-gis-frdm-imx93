// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024, JBD
 */

#include "panel-jbd-jbd4020.h"
#include <asm-generic/errno-base.h>

#define JBD4020_PANEL_LEFT 0
#define GPIO_DEVM 0

#if JBD4020_PANEL_LEFT
#define jbd4020_printf(FMT,...) \
	printk("[JBD4020 LEFT %s(%d)-<%s>]: " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define jbd4020_printf(FMT,...) \
	printk("[JBD4020 RIGHT %s(%d)-<%s>]: " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

static uint8_t jbd_panel_status[3] = {0};

static struct jbd4020_cfg_t jbd_panel_cfg[8] = {
    {17, 12, 1, 1, 1775, 250}, // R_panel
    {12, 12, 1, 0, 1448, 10}, // G_panel
    {16, 12, 1, 1, 644, 30}, // B_panel
};

static inline struct jbd4020_panel_info_t *panel_to_jbd4020(struct drm_panel *panel)
{
	return container_of(panel, struct jbd4020_panel_info_t, panel);
};

/*
 * The panel seems to accept some private DCS commands that map
 * directly to registers.
 *
 * It is organised by page, with each page having its own set of
 * registers, and the first page looks like it's holding the standard
 * DCS commands.
 *
 * So before any attempt at sending a command or data, we have to be
 * sure if we're in the right page or not.
 */
#if DSI_CMD_TRANSFER
static int jbd4020_dsi_send_reg_data(struct jbd4020_panel_info_t *ctx, uint32_t addr, uint32_t data)
{
	int ret;
	uint8_t generic_buf[4] = {0xff, addr>>24, addr>>16, addr&0xff};
	uint8_t dcs_buf[5] = {addr>>8, data&0xff, data>>8, data>>16, data>>24};
	
	ret = mipi_dsi_generic_write(ctx->dsi, generic_buf, sizeof(generic_buf));
	ret |= mipi_dsi_dcs_write_buffer(ctx->dsi, dcs_buf, sizeof(dcs_buf));
	if (ret < 0)
		return ret;

	return 0;
}
#else
static int jbd4020_i2c_read_reg_data(struct i2c_adapter *adapter, uint8_t dev_addr,
	uint32_t addr, uint32_t *val)
{
	uint8_t wbuf[4] = { addr >> 24, addr >> 16, addr >> 8, addr & 0xff };
	struct i2c_msg msg[2];
	uint32_t rbuf;
	int ret = 0;

	msg[0].addr = dev_addr;
	msg[0].flags = 0;
	msg[0].len = 4;
	msg[0].buf = wbuf;

	msg[1].addr = dev_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 4;
	msg[1].buf = (uint8_t *)&rbuf;

	ret = i2c_transfer(adapter, msg, 2);
	if(ret > 0) {
		*val = rbuf;
	}

	// jbd4020_printf("ret:%d, dev_addr: 0x%02x, reg 0x%08x, value: 0x%08x",
	// 	ret, dev_addr, addr, *val);

	return ret == 2 ? 0 : ret;
}

static int jbd4020_i2c_write_reg_data(struct i2c_adapter *adapter, panel_id_t panel_id,
	uint32_t addr, uint32_t val)
{
	int ret = 0;

	if(jbd_panel_status[panel_id]) {
		uint8_t wbuf[8] = {0};
		struct i2c_msg msg;

		wbuf[0] = addr >> 24;
		wbuf[1] = addr >> 16;
		wbuf[2] = addr >> 8;
		wbuf[3] = addr & 0xff;
		wbuf[4] = val & 0xff;
		wbuf[5] = val >> 8;
		wbuf[6] = val >> 16;
		wbuf[7] = val >> 24;

		msg.addr = jdb_panel_addr[panel_id];
		msg.flags = 0;
		msg.len = 8;
		msg.buf = wbuf;

		ret = i2c_transfer(adapter, &msg, 1);

		// jbd4020_printf("ret:%d, dev_addr: 0x%02x, reg: 0x%08X : 0x%08x",
		// 	ret, jdb_panel_addr[panel_id], addr, val);
	} else {
		return ret;
	}

	return ret == 1 ? 0 : ret;
}
#endif

static int jbd4020_reg_list_write(struct jbd4020_panel_info_t *ctx, panel_id_t panel_id,
	const struct jbd4020_instr_t *reg_list, uint32_t count)
{
	int ret = 0;
	uint32_t i; 
	for (i = 0; i < count; i++) {
		const struct jbd4020_instr_t *instr = &reg_list[i];
		if(0xFFFFFFFF == instr->reg) {
			msleep(instr->data);
		} else {
			#if DSI_CMD_TRANSFER
			jbd4020_dsi_send_reg_data(ctx, instr->reg,
								instr->data);
			#else
			if(JBD_PANEL_ALL == panel_id) {
				panel_id_t id;
				for(id = 0; id < JBD_PANEL_ALL; id++) {
					ret |= jbd4020_i2c_write_reg_data(ctx->ddc, id, 
									instr->reg, instr->data);
				}
			} else {
				ret |= jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 
									instr->reg, instr->data);
			}
			#endif
		}
	}

	return ret;
}

static int jbd4020_prepare(struct drm_panel *panel)
{
	struct jbd4020_panel_info_t *ctx = panel_to_jbd4020(panel);

	if (ctx->prepared)
		return 0;

	ctx->prepared = true;
	jbd4020_printf("Set flag Done");

	return 0;
}

static int jbd4020_enable(struct drm_panel *panel)
{
	int ret = 0;
	panel_id_t panel_id;
	struct jbd4020_panel_info_t *ctx = panel_to_jbd4020(panel);

	if (ctx->enabled)
		return 0;

	for(panel_id = 0; panel_id < JBD_PANEL_ALL; panel_id++) {
		ret |= jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 
						0x03030100, 0x80000011);
	}

	ctx->enabled = true;
	jbd4020_printf("Set flag Done");

	return ret;
}

static int jbd4020_disable(struct drm_panel *panel)
{
	struct jbd4020_panel_info_t *ctx = panel_to_jbd4020(panel);
	ctx->enabled = false;
	jbd4020_printf("Set flag Done");

	return 0;
}


static int jbd4020_unprepare(struct drm_panel *panel)
{
	struct jbd4020_panel_info_t *ctx = panel_to_jbd4020(panel);
	ctx->prepared = false;
	jbd4020_printf("Set flag Done");

	return 0;
}

static const struct drm_display_mode jbd4020_default_mode = {
	.hdisplay = 640,
	.hsync_start = 640 + 20,
	.hsync_end = 640 + 20 + 4,
	.htotal = 640 + 20 + 4 + 20,

	.vdisplay = 480,
	.vsync_start = 480 + 2,
	.vsync_end = 480 + 2 + 2,
	.vtotal = 480 + 2 + 2 + 30,

	// clock = htotoal * vtotal * fps /1000
	.clock = 21100, // 40FPS 14063, 60.02FPS 21100

};

static int jbd4020_get_modes(struct drm_panel *panel,
                  struct drm_connector *connector)
{
	struct jbd4020_panel_info_t *ctx = panel_to_jbd4020(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->desc->mode);
	if (!mode) {
		jbd4020_printf("failed to add mode %ux%ux@%u",
			ctx->desc->mode->hdisplay,
			ctx->desc->mode->vdisplay,
			drm_mode_vrefresh(ctx->desc->mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs jbd4020_funcs = {
	.prepare	= jbd4020_prepare,
	.unprepare	= jbd4020_unprepare,
	.enable		= jbd4020_enable,
	.disable	= jbd4020_disable,
	.get_modes	= jbd4020_get_modes,
};

static void jbd4020_get_panel_status(struct jbd4020_panel_info_t *ctx)
{
	int ret = 0;
    uint32_t data;
	panel_id_t panel_id;

    for (panel_id = 0; panel_id < 3; panel_id++) {
		ret = jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x02001020, &data);
		if (0 == ret)
		{
			jbd_panel_status[panel_id] = 1;
			jbd4020_printf("panel %d is valid", panel_id);
		} else {
			jbd4020_printf("panel %d is invalid", panel_id);
		}
	}
}

static int jbd4020_panel_reg_init(struct jbd4020_panel_info_t *ctx)
{
	int ret = 0;
	panel_id_t panel_id;
	uint16_t wait_cnt = 0;
	uint32_t reg_len, rdData;

	jbd4020_printf("start initial registers");
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_ALL, ctx->desc->init, ctx->desc->init_length);
	jbd4020_printf("init reg1 length: %ld", ctx->desc->init_length);

	//XDP: resolution, refresh , pixel current, algorithm setting
	reg_len = sizeof(jbd4020_init_part2_r) / sizeof(struct jbd4020_instr_t);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_R, jbd4020_init_part2_r, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_G, jbd4020_init_part2_g, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_B, jbd4020_init_part2_b, reg_len);
	jbd4020_printf("init reg2, len = %d", reg_len);

	//XDP
	reg_len = sizeof(jbd4020_init_part3) / sizeof(struct jbd4020_instr_t);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_ALL, jbd4020_init_part3, reg_len);
	jbd4020_printf("init reg3, len = %d", reg_len);

	//Part4
	reg_len = sizeof(jbd4020_init_part4_r) / sizeof(struct jbd4020_instr_t);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_R, jbd4020_init_part4_r, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_G, jbd4020_init_part4_g, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_B, jbd4020_init_part4_b, reg_len);
	jbd4020_printf("init reg4, len = %d", reg_len);

	//FMC reset 
	reg_len = sizeof(jbd4020_init_part5) / sizeof(struct jbd4020_instr_t);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_ALL, jbd4020_init_part5, reg_len);
	jbd4020_printf("init reg5, len = %d", reg_len);

	jbd4020_printf("read back gamma load status");
	for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			wait_cnt = 0;
			while (wait_cnt < 100)
			{
				ret |= jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x02005068, &rdData); // read back register, if bit[0]=0, operation complete
				if((0 == ret) && (0 == (rdData & 0x1)))
					break;
				wait_cnt += 1;
				msleep(1);
			}
			if(wait_cnt == 100) {
				jbd4020_printf("panel %d load gamma failed", panel_id);
				return -1;
			}
		}
	}

	reg_len = sizeof(jbd4020_init_part6) / sizeof(struct jbd4020_instr_t);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_ALL, jbd4020_init_part6, reg_len);
	jbd4020_printf("init reg6, len = %d", reg_len);

	jbd4020_printf("read back demura load status");
		for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			wait_cnt = 0;
			while (wait_cnt < 100)
			{
				ret |= jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x02005068, &rdData); // read back register, if bit[0]=0, operation complete
				if((0 == ret) && (0 == (rdData & 0x1)))
					break;
				wait_cnt += 1;
				msleep(1);
			}
			if(wait_cnt == 100) {
				jbd4020_printf("panel %d load demura failed", panel_id);
				return -1;
			}
		}
	}

	reg_len = sizeof(jbd4020_init_part7) / sizeof(struct jbd4020_instr_t);
	jbd4020_printf("init reg7, len = %d", reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_ALL, jbd4020_init_part7, reg_len);

	/* Set default Lum */
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_R, jbd4020_flash_id_read, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_G, jbd4020_flash_id_read, reg_len);
	ret |= jbd4020_reg_list_write(ctx, JBD_PANEL_B, jbd4020_flash_id_read, reg_len);

	return ret;
}

#if JBD4020_PANEL_LEFT == 0
#if GPIO_DEVM
static void jbd4020_gpio_release_action(void *data)
{
    struct gpio_desc *desc = data;
    gpiod_put(desc);
}
#endif

static int jbd4020_panel_gpio_get(struct jbd4020_panel_info_t *ctx)
{
#if GPIO_SET_TYPE
	int ret;

	struct device *dev = &ctx->dsi->dev;
#if GPIO_DEVM
	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_LOW);
#else
	ctx->vddi_gpio = gpiod_get(dev, "vddi", GPIOD_OUT_LOW);
#endif
	if (IS_ERR(ctx->vddi_gpio)) {
		ret = PTR_ERR(ctx->vddi_gpio);
		if (ret != -EPROBE_DEFER)
			jbd4020_printf("failed to get vddi gpio: %d", ret);
		return ret;
	}
	jbd4020_printf("get vddi succeed!");

#if GPIO_DEVM
	ctx->avdd_gpio = devm_gpiod_get(dev, "avdd", GPIOD_OUT_LOW);
#else    
	ctx->avdd_gpio = gpiod_get(dev, "avdd", GPIOD_OUT_LOW);
#endif    
	if (IS_ERR(ctx->avdd_gpio)) {
		ret = PTR_ERR(ctx->avdd_gpio);
		if (ret != -EPROBE_DEFER)
			jbd4020_printf("failed to get avdd gpio: %d", ret);
		return ret;
	}
	jbd4020_printf("get avdd succeed!");

#if GPIO_DEVM
	ctx->avee_gpio = devm_gpiod_get(dev, "avee", GPIOD_OUT_LOW);
#else
	ctx->avee_gpio = gpiod_get(dev, "avee", GPIOD_OUT_LOW);
#endif
	if (IS_ERR(ctx->avee_gpio)) {
		ret = PTR_ERR(ctx->avee_gpio);
		if (ret != -EPROBE_DEFER)
			jbd4020_printf("failed to get avee gpio: %d", ret);
		return ret;
	}
	jbd4020_printf("get avee succeed!");
#if GPIO_DEVM
    // add release action to each pin
    ret = devm_add_action_or_reset(dev, jbd4020_gpio_release_action, ctx->vddi_gpio);
    if (ret) {
        jbd4020_printf("vddi release action set failed: %d", ret);
    }
    ret = devm_add_action_or_reset(dev, jbd4020_gpio_release_action, ctx->avdd_gpio);
    if (ret) {
        jbd4020_printf("avdd release action set failed: %d", ret);
    }
    ret = devm_add_action_or_reset(dev, jbd4020_gpio_release_action, ctx->avee_gpio);
    if (ret) {
        jbd4020_printf("avee release action set failed: %d", ret);
    }
#endif    
#else
	ctx->vddi_gpio = of_get_named_gpio(ctx->dsi->dev.of_node, "vddi-gpio", 0);
	if (!gpio_is_valid(ctx->vddi_gpio))
		jbd4020_printf("vddi_gpio value is not valid");
	else
		jbd4020_printf("vddi_gpio value is %d", ctx->vddi_gpio);

	ctx->avdd_gpio = of_get_named_gpio(ctx->dsi->dev.of_node, "avdd-gpio", 0);
	if (!gpio_is_valid(ctx->avdd_gpio))
		jbd4020_printf("avdd_gpio value is not valid");
	else
		jbd4020_printf("avdd_gpio value is %d", ctx->avdd_gpio);

	ctx->avee_gpio = of_get_named_gpio(ctx->dsi->dev.of_node, "avee-gpio", 0);
	if (!gpio_is_valid(ctx->avee_gpio))
		jbd4020_printf("avee_gpio value is not valid");
	else
		jbd4020_printf("avee_gpio value is %d", ctx->avee_gpio);
#endif
	return 0;
}

static void jbd4020_panel_power_en(struct jbd4020_panel_info_t *ctx)
{
#if GPIO_SET_TYPE
	jbd4020_printf("set vddi_gpio enable");
	gpiod_set_value(ctx->vddi_gpio, 1);
	msleep(1);
	jbd4020_printf("set avdd_gpio enable");
	gpiod_set_value(ctx->avdd_gpio, 1);
#else
	int error;
	jbd4020_printf("set vddi_gpio enable");
	if (gpio_is_valid(ctx->vddi_gpio)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(ctx->vddi_gpio, "jbd_vddi_gpio");
		if (error) {
			jbd4020_printf("unable to request vddi_gpio gpio [%d]", ctx->vddi_gpio);
		}

		error = gpio_direction_output(ctx->vddi_gpio, 1);
		if (error) {
			jbd4020_printf("unable to set direction for vddi_gpio gpio [%d]",
			  ctx->vddi_gpio);
		}
	}

	msleep(1);

	jbd4020_printf("set avdd_gpio enable");
	if (gpio_is_valid(ctx->avdd_gpio)) {
		error = gpio_request(ctx->vddi_gpio, "jbd_avdd_gpio");
		if (error) {
			jbd4020_printf("unable to request avdd_gpio gpio [%d]", ctx->avdd_gpio);
		}

		error = gpio_direction_output(ctx->avdd_gpio, 1);
		if (error) {
			jbd4020_printf("unable to set direction for avdd_gpio gpio [%d]",
			  ctx->avdd_gpio);
		}
	}
#endif
};

static void jbd4020_panel_avee_en(struct jbd4020_panel_info_t *ctx)
{
#if GPIO_SET_TYPE
	jbd4020_printf("set avee_gpio enable");
	gpiod_set_value(ctx->avee_gpio, 1);
#else
	int error;
	jbd4020_printf("set avee_gpio enable");
	if (gpio_is_valid(ctx->avee_gpio)) {
		error = gpio_request(ctx->avee_gpio, "jbd_avee_gpio");
		if (error) {
			jbd4020_printf("unable to request avee_gpio gpio [%d]", ctx->avee_gpio);
		}

		error = gpio_direction_output(ctx->avee_gpio, 1);
		if (error) {
			jbd4020_printf("unable to set direction for avee_gpio gpio [%d]",
			  ctx->avee_gpio);
		}
	}
#endif
}

static void jbd4020_panel_power_free(struct jbd4020_panel_info_t *ctx)
{
	// CC: GPIO got by devm_gpiod_get() doesn't need to be freed manually.
#if GPIO_SET_TYPE
	gpiod_set_value(ctx->vddi_gpio, 0);
	gpiod_set_value(ctx->avdd_gpio, 0);
#else
	jbd4020_printf("set vddi_gpio free");
	gpio_free(ctx->vddi_gpio);

	jbd4020_printf("set avdd_gpio free");
	gpio_free(ctx->avdd_gpio);
#endif
}

static void jbd4020_panel_avee_free(struct jbd4020_panel_info_t *ctx)
{
#if GPIO_SET_TYPE
	gpiod_set_value(ctx->avee_gpio, 0);
#else
	jbd4020_printf("set avee_gpio free");
	gpio_free(ctx->avee_gpio);
#endif
}

static void jbd4020_panel_power_down(struct jbd4020_panel_info_t *ctx)
{
#if GPIO_SET_TYPE
	gpiod_set_value(ctx->avee_gpio, 0);
	msleep(1);
	gpiod_set_value(ctx->avdd_gpio, 0);
	msleep(1);
	gpiod_set_value(ctx->vddi_gpio, 0);
#else
	int error;
	jbd4020_printf("set avee_gpio down!");
	if (gpio_is_valid(ctx->avee_gpio)) {
		error = gpio_direction_output(ctx->avee_gpio, 0);

		if (error) {
			jbd4020_printf("jbd4020 unable to set direction for avee gpio [%d]",
			  ctx->avee_gpio);
		}
	}
	msleep(1);

	jbd4020_printf("set avdd_gpio down!");
	if (gpio_is_valid(ctx->avdd_gpio)) {
		error = gpio_direction_output(ctx->avdd_gpio, 0);

		if (error) {
			jbd4020_printf("unable to set direction for avdd gpio [%d]",
			  ctx->avdd_gpio);
		}
	}
	msleep(1);

	jbd4020_printf("set avdd_gpio down!");
	if (gpio_is_valid(ctx->vddi_gpio)) {
		error = gpio_direction_output(ctx->vddi_gpio, 0);

		if (error) {
			jbd4020_printf("unable to set direction for vddi gpio [%d]",
			  ctx->vddi_gpio);
		}
	}

	jbd4020_panel_power_free(ctx);
	jbd4020_panel_avee_free(ctx);
#endif
}
#endif

static void jbd4020_panel_read_default_cfg(struct jbd4020_panel_info_t *ctx, struct jbd4020_cfg_t *jbd4020_cfg)
{
	panel_id_t panel_id;
    uint16_t wait_cnt;
    uint32_t rdData[3] = {0};
    uint8_t slvAddr;

	for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			uint32_t reg_len = 0;

			reg_len = sizeof(jbd4020_flash_id_read) / sizeof(struct jbd4020_instr_t);
			jbd4020_reg_list_write(ctx, JBD_PANEL_R, jbd4020_flash_id_read, reg_len);

			jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0200504C, 0x030006fc);	// Load data to SRAM address 0x30006fc
			jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0200502C, 0x31408);	// the flash addr of src data
			jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x02005040, 0x4);		// the length of data, unit: byte
			jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x02005068, 0x030001);	// start load
		}
	}

	for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			wait_cnt = 0;
			slvAddr = jdb_panel_addr[panel_id];
			while (wait_cnt++ < 100) {
				jbd4020_i2c_read_reg_data(ctx->ddc, slvAddr, 0x02005068, &rdData[panel_id]); // read back register, if bit[0]=0, operation complete
				if ((rdData[panel_id] & 0x1) == 0)
					break;
				msleep(1);
			}
		}
	}

    //读Sram
	for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x030006fc, rdData + panel_id);
			jbd4020_printf("slvAddr:0x%02x, 0x030006fc: 0x%08x", slvAddr, rdData[panel_id]);
		}
	}

    if((rdData[0]==0xffffffff || rdData[0] == 0) || (rdData[1]==0xffffffff || rdData[1] == 0) || (rdData[2]==0xffffffff || rdData[2] == 0))
    {
		jbd4020_printf("Not enough RGB panel or config has not been written");
        return;
    }
    else
    {
        jbd4020_cfg[JBD_PANEL_R].offsetH = (rdData[0] & 0x1f0) >> 4;
        jbd4020_cfg[JBD_PANEL_G].offsetH = (rdData[1] & 0x1f0) >> 4;
        jbd4020_cfg[JBD_PANEL_B].offsetH = (rdData[2] & 0x1f0) >> 4;

        jbd4020_cfg[JBD_PANEL_R].offsetV = (rdData[0] >> 12) & 0x1f;
        jbd4020_cfg[JBD_PANEL_G].offsetV = (rdData[1] >> 12) & 0x1f;
        jbd4020_cfg[JBD_PANEL_B].offsetV = (rdData[2] >> 12) & 0x1f;

        jbd4020_printf("jbd4020_cfg[JBD_PANEL_R].offsetH:%d", jbd4020_cfg[JBD_PANEL_R].offsetH);
        jbd4020_printf("jbd4020_cfg[JBD_PANEL_G].offsetH:%d", jbd4020_cfg[JBD_PANEL_G].offsetH);
        jbd4020_printf("jbd4020_cfg[JBD_PANEL_B].offsetH:%d", jbd4020_cfg[JBD_PANEL_B].offsetH);

        jbd4020_printf("jbd4020_cfg[JBD_PANEL_R].offsetV:%d", jbd4020_cfg[JBD_PANEL_R].offsetV);
        jbd4020_printf("jbd4020_cfg[JBD_PANEL_G].offsetV:%d", jbd4020_cfg[JBD_PANEL_G].offsetV);
        jbd4020_printf("jbd4020_cfg[JBD_PANEL_B].offsetV:%d", jbd4020_cfg[JBD_PANEL_B].offsetV);
    }
}

static int jbd4020_panel_creg_set(struct jbd4020_panel_info_t *ctx, panel_id_t panel_id, uint8_t creg)
{
	int ret = jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0302ae90, 0x00010000 + creg);

	return ret;
}

static int jbd4020_panel_lum_set(struct jbd4020_panel_info_t *ctx, panel_id_t panel_id, uint32_t lum)
{
	int ret = 0;

	ret |= jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0302AE00, 0x00000380); //open Gamma
	ret |= jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0302AE04, 0x0104101E); //close DBV Dimming
	ret |= jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, JBD4020_REG_DBV, lum);

	return ret;
}

static int jbd4020_panel_flip_set(struct jbd4020_panel_info_t *ctx, panel_id_t panel_id, 
		uint8_t flip, uint8_t mirror)
{
	int ret = 0;
	uint32_t data = 0;

	ret = jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x03030004, &data);
	if(ret) {
		jbd4020_printf("current SRAM flip&mirror get failed");
		return ret;
	}
	data = (data & 0xfffffffc) | (flip << 1) | mirror;
	ret = jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x03030004, data);
	jbd4020_printf("Set reg: 0x03030004, data: 0x%08x", data);

	ret = jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x0300015c, &data);
	if(ret) {
		jbd4020_printf("current VO flip&mirror get failed");
		return ret;
	}
	data = (data & 0x3fffffff) | (flip << 31) | (mirror << 30);
	ret = jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0300015c, data);
	jbd4020_printf("Set reg: 0x0300015c, data: 0x%08x", data);

	return ret;
}

static int jbd4020_panel_offset_set(struct jbd4020_panel_info_t *ctx, panel_id_t panel_id, 
		uint8_t offsetH, uint8_t offsetV)
{
	int ret = 0;
	uint32_t data = 0;

	data = (offsetV << 12) | (offsetH << 4) | 1;
	ret = jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x03030008, data);
	jbd4020_printf("Set reg: 0x03030008, data: 0x%08x", data);

	ret = jbd4020_i2c_read_reg_data(ctx->ddc, jdb_panel_addr[panel_id], 0x0300015c, &data);
	if(ret) {
		jbd4020_printf("current VO offset get failed");
		return ret;
	}
	data = (data & 0xd000f000) | (offsetV << 16) | (offsetH) | (2 << 28);
	ret = jbd4020_i2c_write_reg_data(ctx->ddc, panel_id, 0x0300015c, data);
	jbd4020_printf("Set reg: 0x0300015c, data: 0x%08x", data);

	return ret;
}

static int jbd4020_panel_default_config(struct jbd4020_panel_info_t *ctx)
{
	int ret = 0;
	panel_id_t panel_id;

	jbd4020_panel_read_default_cfg(ctx, jbd_panel_cfg);

	for(panel_id = 0; panel_id < 3; panel_id++) {
		if(jbd_panel_status[panel_id]) {
			ret |= jbd4020_panel_lum_set(ctx, panel_id, jbd_panel_cfg[panel_id].dbv);
			ret |= jbd4020_panel_creg_set(ctx, panel_id, jbd_panel_cfg[panel_id].cur);
			ret |= jbd4020_panel_flip_set(ctx, panel_id, jbd_panel_cfg[panel_id].flip,
					jbd_panel_cfg[panel_id].mirror);
			ret |= jbd4020_panel_offset_set(ctx, panel_id, jbd_panel_cfg[panel_id].offsetH,
					jbd_panel_cfg[panel_id].offsetV);
			jbd4020_printf("default config: ret:%d, panel id: %d, dbv:%d, cur: %d, flip:%d, mirror:%d, offsetH:%u, offsetV:%u",
				ret, panel_id, jbd_panel_cfg[panel_id].dbv, jbd_panel_cfg[panel_id].cur,
				jbd_panel_cfg[panel_id].flip, jbd_panel_cfg[panel_id].mirror,
				jbd_panel_cfg[panel_id].offsetH, jbd_panel_cfg[panel_id].offsetV);
		}
	}

	return ret;
}

static int jbd4020_panel_probe(struct mipi_dsi_device *dsi)
{
	int ret;
	struct jbd4020_panel_info_t *ctx;
	struct device_node *ddc;

	jbd4020_printf("start devm kzalloc");
	
	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->desc = of_device_get_match_data(&dsi->dev);
	dsi->mode_flags = ctx->desc->mode_flags;
	dsi->format = ctx->desc->format;
	dsi->lanes = ctx->desc->lanes;
	ctx->dsi = dsi;

	ddc = of_parse_phandle(dsi->dev.of_node, "ddc-i2c-bus", 0);
	if (ddc) {
		ctx->ddc = of_find_i2c_adapter_by_node(ddc);
		of_node_put(ddc);

		if (!ctx->ddc) {
			ret = -EPROBE_DEFER;
			jbd4020_printf("failed to find ddc-i2c-bus: %d", ret);
			return ret;
		}

        jbd4020_printf("DDC I2C adapter name: %s\n", ctx->ddc->name);
        jbd4020_printf("DDC I2C adapter number: %d\n", ctx->ddc->nr);
	}
	jbd4020_printf("ddc-i2c-bus ok");

#if JBD4020_PANEL_LEFT == 0
	ret = jbd4020_panel_gpio_get(ctx);
	if (ret < 0) {
		ret = -EPROBE_DEFER;
		jbd4020_printf("jbd4020_panel_gpio_get failed");
		return ret;
	}
	jbd4020_printf("jbd4020_panel_gpio_get done");

	jbd4020_panel_power_en(ctx);
	jbd4020_printf("jbd4020_panel_power_en done");
#endif

	msleep(20);
	/* Check panel status */
	jbd4020_get_panel_status(ctx);

#if 1	// CC: was 0
	if(0 == jbd_panel_status[0] && 0 == jbd_panel_status[1] && 0 == jbd_panel_status[2]) {
		ret = -ENOPARAM;
#if 1   // CC: was JBD4020_PANEL_LEFT == 0
		jbd4020_panel_power_free(ctx);
#endif
		jbd4020_printf("No Valid panel");
		return ret;
	}
#endif

	/* Config initial regs */
	ret = jbd4020_panel_reg_init(ctx);
	if (ret < 0) {
		jbd4020_printf("jbd4020_panel_reg_init failed");
		return ret;
	}
	jbd4020_printf("jbd4020_panel_reg_init done");

#if JBD4020_PANEL_LEFT == 0
	jbd4020_panel_avee_en(ctx);
	jbd4020_printf("jbd4020_panel_avee_en done");
#endif

	ret = jbd4020_panel_default_config(ctx);
	if (ret < 0) {
		jbd4020_printf("jbd4020_panel_default_config failed");
		return ret;
	}
	jbd4020_printf("jbd4020_panel_default_config done");

	mipi_dsi_set_drvdata(dsi, ctx);
	jbd4020_printf("mipi_dsi_set_drvdata");

	drm_panel_init(&ctx->panel, &dsi->dev, &jbd4020_funcs, DRM_MODE_CONNECTOR_DSI);
	//drm_panel_init(&ctx->panel);

	drm_panel_add(&ctx->panel);
	jbd4020_printf("drm_panel_add");

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		jbd4020_printf("mipi_dsi_attach failed");
#if JBD4020_PANEL_LEFT == 0
		jbd4020_panel_power_down(ctx);
#endif
		drm_panel_remove(&ctx->panel);

		if (ctx->ddc)
			put_device(&ctx->ddc->dev);
	}

    // release gpios, so that could be controlled in user space
#if GPIO_DEVM
    devm_remove_action(&ctx->dsi->dev, jbd4020_gpio_release_action, ctx->vddi_gpio);
    devm_remove_action(&ctx->dsi->dev, jbd4020_gpio_release_action, ctx->avdd_gpio);
    devm_remove_action(&ctx->dsi->dev, jbd4020_gpio_release_action, ctx->avee_gpio);
#else
    gpiod_put(ctx->vddi_gpio);
    gpiod_put(ctx->avdd_gpio);
    gpiod_put(ctx->avee_gpio);
#endif
	jbd4020_printf("mipi_dsi_attach done");

	return ret;
}

static void jbd4020_panel_remove(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	struct jbd4020_panel_info_t *ctx = mipi_dsi_get_drvdata(dsi);
#if JBD4020_PANEL_LEFT == 0
	jbd4020_panel_power_down(ctx);
#endif

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		jbd4020_printf("failed to detach from DSI host: %d", ret);

	drm_panel_remove(&ctx->panel);

	if (ctx->ddc)
		put_device(&ctx->ddc->dev);

	return;
}

static const struct jbd4020_desc_t jbd4020_desc = {
	.init = jbd4020_init,
	.init_length = ARRAY_SIZE(jbd4020_init),
	.mode = &jbd4020_default_mode,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 1,
};

static const struct of_device_id jbd4020_of_match[] = {
	{
		.compatible = "jbd,jbd4020-right",
		.data = &jbd4020_desc,
	},
};
MODULE_DEVICE_TABLE(of, jbd4020_of_match);

static struct mipi_dsi_driver jbd4020_panel_driver = {
	.probe		= jbd4020_panel_probe,
	.remove		= jbd4020_panel_remove,
#if JBD4020_PANEL_LEFT
	.driver = {
		.name		= "panel-jbd4020-left",
		.of_match_table	= jbd4020_of_match,
	},
#else
	.driver = {
		.name		= "panel-jbd4020-right",
		.of_match_table	= jbd4020_of_match,
	},
#endif
};
module_mipi_dsi_driver(jbd4020_panel_driver);

MODULE_AUTHOR("Yubo Zhu<yubo_zhu@jb-display.com>");
MODULE_DESCRIPTION("JBD4020 Panel Driver");
MODULE_LICENSE("GPL v2");
