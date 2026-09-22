// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 GIS / JBD
 * JBD4040 MicroLED Panel Driver for MIPI Command Mode (60Hz)
 */

#include "panel-jbd4040.h"
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/math64.h>
#include <linux/math.h>
#include <linux/string.h>
#include <linux/slab.h>

#define DRV_NAME "panel-jbd4040"

#define jbd4040_info(dev, fmt, ...)  dev_info(dev, "[JBD4040]: " fmt "\n", ##__VA_ARGS__)
#define jbd4040_err(dev, fmt, ...)   dev_err(dev, "[JBD4040 ERR]: " fmt "\n", ##__VA_ARGS__)

static struct jbd4040_panel_info *g_jbd4040_ctx = NULL;
static dev_t jbd4040_dev_num;
static struct cdev jbd4040_cdev;
static struct class *jbd4040_class = NULL;
static struct device *jbd4040_dev = NULL;
static DEFINE_MUTEX(jbd4040_mutex);

static bool bist_enable = false;
module_param(bist_enable, bool, 0644);
MODULE_PARM_DESC(bist_enable, "Enable BIST test pattern on boot (default: false)");

static const uint16_t fps_array[] = {60, 90, 120, 180, 240, 360, 420, 480};

static inline struct jbd4040_panel_info *panel_to_jbd4040(struct drm_panel *panel)
{
	return container_of(panel, struct jbd4040_panel_info, panel);
}

/* =========================================================================
 * I2C Direct Transfer Helper Functions (No dummy client registration needed)
 * ========================================================================= */

static int jbd4040_i2c_write_reg16(struct i2c_adapter *adap, uint8_t dev_addr,
				   uint32_t reg, uint16_t val)
{
	uint8_t buf[5];
	struct i2c_msg msg;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 24-bit register address (Big-Endian) */
	buf[0] = (reg >> 16) & 0xFF;
	buf[1] = (reg >> 8) & 0xFF;
	buf[2] = reg & 0xFF;

	/* 16-bit data (Little-Endian) */
	buf[3] = val & 0xFF;
	buf[4] = (val >> 8) & 0xFF;

	msg.addr = dev_addr;
	msg.flags = 0;
	msg.len = 5;
	msg.buf = buf;

	if (i2c_transfer(adap, &msg, 1) != 1) {
		pr_err("[JBD4040] I2C Write 16-bit failed: reg 0x%06X (addr 0x%02X)\n",
		       reg, dev_addr);
		return -EIO;
	}
	return 0;
}

static int jbd4040_i2c_read_reg16(struct i2c_adapter *adap, uint8_t dev_addr,
				  uint32_t reg, uint16_t *val)
{
	uint8_t addr_buf[3];
	uint8_t data_buf[2] = {0};
	struct i2c_msg msg_write, msg_read;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 24-bit register address */
	addr_buf[0] = (reg >> 16) & 0xFF;
	addr_buf[1] = (reg >> 8) & 0xFF;
	addr_buf[2] = reg & 0xFF;

	/* Stage 1: Send address */
	msg_write.addr = dev_addr;
	msg_write.flags = 0;
	msg_write.len = 3;
	msg_write.buf = addr_buf;

	if (i2c_transfer(adap, &msg_write, 1) != 1)
		return -EIO;

	/* Stage 2: Read 2 bytes */
	msg_read.addr = dev_addr;
	msg_read.flags = I2C_M_RD;
	msg_read.len = 2;
	msg_read.buf = data_buf;

	if (i2c_transfer(adap, &msg_read, 1) != 1)
		return -EIO;

	*val = (data_buf[1] << 8) | data_buf[0];
	return 0;
}

static int jbd4040_i2c_write_reg32(struct i2c_adapter *adap, uint8_t dev_addr,
				   uint32_t reg, uint32_t val)
{
	uint8_t buf[7];
	struct i2c_msg msg;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 24-bit register address (Big-Endian) */
	buf[0] = (reg >> 16) & 0xFF;
	buf[1] = (reg >> 8) & 0xFF;
	buf[2] = reg & 0xFF;

	/* 32-bit data (Little-Endian) */
	buf[3] = val & 0xFF;
	buf[4] = (val >> 8) & 0xFF;
	buf[5] = (val >> 16) & 0xFF;
	buf[6] = (val >> 24) & 0xFF;

	msg.addr = dev_addr;
	msg.flags = 0;
	msg.len = 7;
	msg.buf = buf;

	if (i2c_transfer(adap, &msg, 1) != 1) {
		pr_err("[JBD4040] I2C Write 32-bit failed: reg 0x%06X (addr 0x%02X)\n",
		       reg, dev_addr);
		return -EIO;
	}
	return 0;
}

static int jbd4040_i2c_read_reg32(struct i2c_adapter *adap, uint8_t dev_addr,
				  uint32_t reg, uint32_t *val)
{
	uint8_t addr_buf[3];
	uint8_t data_buf[4] = {0};
	struct i2c_msg msg_write, msg_read;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 24-bit register address (Big-Endian) */
	addr_buf[0] = (reg >> 16) & 0xFF;
	addr_buf[1] = (reg >> 8) & 0xFF;
	addr_buf[2] = reg & 0xFF;

	msg_write.addr = dev_addr;
	msg_write.flags = 0;
	msg_write.len = 3;
	msg_write.buf = addr_buf;

	if (i2c_transfer(adap, &msg_write, 1) != 1)
		return -EIO;

	msg_read.addr = dev_addr;
	msg_read.flags = I2C_M_RD;
	msg_read.len = 4;
	msg_read.buf = data_buf;

	if (i2c_transfer(adap, &msg_read, 1) != 1)
		return -EIO;

	*val = (data_buf[3] << 24) | (data_buf[2] << 16) | (data_buf[1] << 8) | data_buf[0];
	return 0;
}

static int jbd4040_i2c_read_block(struct i2c_adapter *adap, uint8_t dev_addr,
				  uint32_t reg, uint8_t *buf, size_t len)
{
	uint8_t addr_buf[3];
	struct i2c_msg msg_write, msg_read;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 24-bit register address (Big-Endian) */
	addr_buf[0] = (reg >> 16) & 0xFF;
	addr_buf[1] = (reg >> 8) & 0xFF;
	addr_buf[2] = reg & 0xFF;

	/* Stage 1: Send address */
	msg_write.addr = dev_addr;
	msg_write.flags = 0;
	msg_write.len = 3;
	msg_write.buf = addr_buf;

	if (i2c_transfer(adap, &msg_write, 1) != 1) {
		pr_err("[JBD4040] I2C block read addr failed: reg 0x%06X (addr 0x%02X)\n",
		       reg, dev_addr);
		return -EIO;
	}

	/* Stage 2: Read data */
	msg_read.addr = dev_addr;
	msg_read.flags = I2C_M_RD;
	msg_read.len = len;
	msg_read.buf = buf;

	if (i2c_transfer(adap, &msg_read, 1) != 1) {
		pr_err("[JBD4040] I2C block read data failed: reg 0x%06X, len %zu (addr 0x%02X)\n",
		       reg, len, dev_addr);
		return -EIO;
	}

	return 0;
}

static int jbd4040_update_gamma(struct i2c_adapter *adap, uint8_t dev_addr,
				const uint16_t *table)
{
	uint32_t addr = JBD4040_GAMMA_TABLE_0_ADDR;
	int i;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	for (i = 0; i < 256; i++) {
		if (jbd4040_i2c_write_reg16(adap, dev_addr, addr + (i * 2), table[i]))
			return -EIO;
	}
	return jbd4040_i2c_write_reg16(adap, dev_addr, REG_GAM_CFG, 0x0100);
}

/* =========================================================================
 * Panel Status and I2C Communication Health Check
 * ========================================================================= */

static int jbd4040_check_panel_status(struct jbd4040_panel_info *ctx)
{
	struct i2c_adapter *adap = ctx->i2c_adap;
	uint16_t chip_id0 = 0, chip_id1 = 0;
	uint16_t id_r = 0, id_g = 0, id_b = 0;
	int ret;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 
	 * 1. 透過廣播位址 (0x58) 讀取晶片 ID，驗證 I2C 通訊
	 */
	ret = jbd4040_i2c_read_reg16(adap, JBD4040_I2C_ADDR_ALL, REG_CHIP_ID0, &chip_id0);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "I2C read CHIP_ID0 failed on addr 0x58! Check I2C bus wiring.\n");
		return ret;
	}

	ret = jbd4040_i2c_read_reg16(adap, JBD4040_I2C_ADDR_ALL, REG_CHIP_ID1, &chip_id1);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "I2C read CHIP_ID1 failed on addr 0x58!\n");
		return ret;
	}

	dev_info(&ctx->dsi->dev, "I2C communication OK: CHIP_ID0=0x%04X, CHIP_ID1=0x%04X\n",
		 chip_id0, chip_id1);

	/* 驗證晶片 ID 是否符合 JBD4040 規範 (預期 0xBD40, 0x4001) */
	if (chip_id0 != 0xbd40 && chip_id0 != 0x40bd) {
		dev_warn(&ctx->dsi->dev, "Warning: CHIP_ID0 (0x%04X) differs from expected (0xBD40)\n", chip_id0);
	}

	/* 
	 * 2. 逐一檢查 R(0x59), G(0x5A), B(0x5B) 三個光機通道的通訊狀態
	 */
	ret = jbd4040_i2c_read_reg16(adap, JBD4040_I2C_ADDR_RED, REG_CHIP_ID0, &id_r);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Red panel (0x59) I2C check FAILED!\n");
		return ret;
	}
	dev_info(&ctx->dsi->dev, "Red panel (0x59) OK (ID: 0x%04X)\n", id_r);

	ret = jbd4040_i2c_read_reg16(adap, JBD4040_I2C_ADDR_GREEN, REG_CHIP_ID0, &id_g);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Green panel (0x5A) I2C check FAILED!\n");
		return ret;
	}
	dev_info(&ctx->dsi->dev, "Green panel (0x5A) OK (ID: 0x%04X)\n", id_g);

	ret = jbd4040_i2c_read_reg16(adap, JBD4040_I2C_ADDR_BLUE, REG_CHIP_ID0, &id_b);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Blue panel (0x5B) I2C check FAILED!\n");
		return ret;
	}
	dev_info(&ctx->dsi->dev, "Blue panel (0x5B) OK (ID: 0x%04X)\n", id_b);

	return 0;
}

/* =========================================================================
 * JBD4040 Hardware Register Initialization (Command Mode)
 * ========================================================================= */

static int jbd4040_init_registers(struct jbd4040_panel_info *ctx)
{
	struct i2c_adapter *adap = ctx->i2c_adap;
	uint8_t all = JBD4040_I2C_ADDR_ALL;
	int ret = 0;

	if (IS_ERR_OR_NULL(adap))
		return -ENODEV;

	/* 1. Interrupt Masks */
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x201044, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x20104c, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x201054, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x20105c, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x201064, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x20106c, 0x0000ffff);
	ret |= jbd4040_i2c_write_reg16(adap, all, 0x200b0a, 0xffff);

	/* 2. MIPI DSI & DPHY Configuration */
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_CORE_RSTN, 0x00000001);
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_CFG_EOTP, 0x00000000);
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_CFG_PKT_VALID_VC, 0x0000000f);
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_CFG_PKT_CLK_MGR, 0x00000008);
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_DPHY_REG00, 0x0000007d); /* Enable D-PHY clock and lane 0 */
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_DPHY_REG18, 0x00000008); /* THS-SETTLE */
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_CFG_VID_TX_DELAY, 0x000004a2);
	ret |= jbd4040_i2c_write_reg32(adap, all, 0x202128, 0x0000000f);
	ret |= jbd4040_i2c_write_reg32(adap, all, REG_DPHY_REG1D, 0x00000027);

	/* 3. Panel Configuration */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_CUR, 0x0022);      /* Default current: 0x0022 matching i2c-jbd4040 */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_CFG, 0x0008);      /* Default 60Hz, 10-bit scan */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_LUM_REG, 0x1388);     /* Default luminance: 5000 (0x1388) */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_DISP_CTL, 0x0001);    /* Display on */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_SYNC, 0x0002);     /* load_en */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_IMG_XS_ADDR, 0x0000); /* X start = 0 */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_IMG_XE_ADDR, 0x017b); /* X end = 379 */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_IMG_YS_ADDR, 0x0000); /* Y start = 0 */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_IMG_YE_ADDR, 0x01f3); /* Y end = 499 */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_IMG_OFFSET, 0x0a06);  /* x_offset = 10, y_offset = 6 */

	/* 4. Mode Configuration */
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_DCS_EN, 0x0000);      /* Disable DCS command interface */
	if (bist_enable) {
		ret |= jbd4040_i2c_write_reg16(adap, all, REG_VID_MODE, 0x0000);    /* 0 = Command Mode (required for BIST / Global Scan) */
	} else {
		ret |= jbd4040_i2c_write_reg16(adap, all, REG_VID_MODE, 0x0001);    /* 1 = Video Mode */
	}

	/* 5. Algorithm Initialization */
	ret |= jbd4040_i2c_write_reg16(adap, all, 0x200d30, 0x0003);
	ret |= jbd4040_i2c_write_reg16(adap, all, 0x200d3a, 0x0210);
	ret |= jbd4040_i2c_write_reg16(adap, all, 0x200d34, 0x80a1);
	ret |= jbd4040_i2c_write_reg16(adap, all, REG_DMR_REMAP_VAL, 0x03ff);

	/* 6. Frame Sync trigger */
	if (bist_enable) {
		ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_SELF_TEST_CFG, 0x0bff); /* Test mode, 100% APL, 0x3FF brightness */
		ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_SYNC, 0x000f);          /* Global sync, load_1t_en, refresh_loop_en */
	} else {
		ret |= jbd4040_i2c_write_reg16(adap, all, REG_PL_SYNC, 0x000f);          /* Video Mode rolling sync */
	}

	msleep(20);

	/* 7. Load Gamma Table */
	jbd4040_update_gamma(adap, all, jbd4040_gamma_2_2_table);

	/* 8. Individual Color Panel Alignment & Flip (Default: upright normal view) */
	jbd4040_i2c_write_reg16(adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, 0x0003);
	jbd4040_i2c_write_reg16(adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, 0x0002);
	jbd4040_i2c_write_reg16(adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, 0x0003);

	if (ret)
		dev_err(&ctx->dsi->dev, "Failed to initialize JBD4040 registers!\n");
	else
		dev_info(&ctx->dsi->dev, "JBD4040 Command Mode registers initialized successfully\n");

	return ret;
}

/* =========================================================================
 * DRM Panel Callbacks
 * ========================================================================= */

static int jbd4040_prepare(struct drm_panel *panel)
{
	struct jbd4040_panel_info *ctx = panel_to_jbd4040(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	if (IS_ERR_OR_NULL(ctx->i2c_adap)) {
		dev_err(&ctx->dsi->dev, "I2C adapter is not valid! Cannot prepare panel.\n");
		return -ENODEV;
	}

	/* 0. Initial state: All power rails off, RESET asserted (Logical 1 = Physical 0V) */
	if (ctx->avee_gpio)
		gpiod_set_value_cansleep(ctx->avee_gpio, 0);
	if (ctx->avdd_gpio)
		gpiod_set_value_cansleep(ctx->avdd_gpio, 0);
	if (ctx->reset_gpio)
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(200);

	/* 1. Turn on AVDD (+3.3V) */
	if (ctx->avdd_gpio)
		gpiod_set_value_cansleep(ctx->avdd_gpio, 1);
	msleep(100);

	/* 2. RESET Pulse (High -> Low -> High) matching i2c-jbd4040.c
	 * In DTS: reset-gpios = <&gpio2 24 GPIO_ACTIVE_LOW>;
	 * Logical 0 = Inactive (Physical 3.3V / High)
	 * Logical 1 = Active (Physical 0V / Reset Low)
	 */
	if (ctx->reset_gpio) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 0); /* Physical 3.3V */
		msleep(1000);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1); /* Physical 0V (Assert Reset) */
		msleep(50);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0); /* Physical 3.3V (Release Reset) */
		msleep(200);
	}

	/* 3. 寫入暫存器初始化 (MIPI D-PHY, Core Reset, Panel Config, Gamma 等) */
	ret = jbd4040_init_registers(ctx);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Register initialization FAILED! ABORT: AVEE will not be turned ON.\n");
		goto err_power_off;
	}

	msleep(200);

	/* 
	 * 4. 檢查 I2C 通訊與 Panel Status (CHIP_ID0, CHIP_ID1, R/G/B status)
	 */
	ret = jbd4040_check_panel_status(ctx);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Panel I2C/Status check FAILED! ABORT: AVEE will not be turned ON.\n");
		goto err_power_off;
	}

	/* 
	 * 5. I2C 與面板狀態確認完全正常後，才開啟 AVEE (-2.2V)
	 * 延時 1000ms 對應 i2c-jbd4040.c / Python time.sleep(1)
	 */
	msleep(1000);
	if (ctx->avee_gpio) {
		gpiod_set_value_cansleep(ctx->avee_gpio, 1);
		dev_info(&ctx->dsi->dev, "AVEE enabled (-2.2V active)\n");
	}
	msleep(100); /* 等待負壓完全穩定 */

	ctx->prepared = true;
	dev_info(&ctx->dsi->dev, "JBD4040 panel prepared successfully with AVEE ON\n");
	return 0;

err_power_off:
	if (ctx->avee_gpio)
		gpiod_set_value_cansleep(ctx->avee_gpio, 0);
	if (ctx->reset_gpio)
		gpiod_set_value_cansleep(ctx->reset_gpio, 1); /* Assert Reset (Physical 0V) */
	if (ctx->avdd_gpio)
		gpiod_set_value_cansleep(ctx->avdd_gpio, 0);
	return ret;
}

static int jbd4040_enable(struct drm_panel *panel)
{
	struct jbd4040_panel_info *ctx = panel_to_jbd4040(panel);

	if (!ctx->prepared)
		return -EIO;

	if (ctx->enabled)
		return 0;

	/* 
	 * 確保 AVEE 已開啟且電源穩定，才開啟面板顯示開關並觸發同步，
	 * 此時主控端開始傳送 MIPI Command Mode 影像資料
	 */
	if (!IS_ERR_OR_NULL(ctx->i2c_adap)) {
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);
		if (bist_enable) {
			/* BIST self-test mode: Command Mode (VID_MODE = 0) and 60Hz internal refresh */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);  /* 60Hz, 10-bit scan */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x1388); /* 5000 luminance */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CUR, 0x0022);  /* 0x0022 current */
			/* BIST self-test mode: 100% APL, full brightness (0x3FF), load_1t_en loop refresh */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SELF_TEST_CFG, 0x0bff);
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x000f);
			ctx->bist = true;
			ctx->vid_mode = false;
			dev_info(&ctx->dsi->dev, "JBD4040 BIST test pattern enabled (Command Mode, 60Hz loop mode, full white)\n");
		} else {
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SELF_TEST_CFG, 0x0000);
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);  /* 60Hz, 10-bit scan */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x1388); /* 5000 luminance */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CUR, 0x0022);  /* 0x0022 current */
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0001);
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x000f);
			ctx->bist = false;
			ctx->vid_mode = true;
			dev_info(&ctx->dsi->dev, "JBD4040 Video Mode enabled: 60Hz Sync to MIPI Video stream\n");
		}
	}

	ctx->enabled = true;
	dev_info(&ctx->dsi->dev, "JBD4040 panel enabled\n");
	return 0;
}

static int jbd4040_disable(struct drm_panel *panel)
{
	struct jbd4040_panel_info *ctx = panel_to_jbd4040(panel);

	if (!ctx->enabled)
		return 0;

	/* 關閉顯示開關 */
	if (!IS_ERR_OR_NULL(ctx->i2c_adap))
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0000);

	ctx->enabled = false;
	dev_info(&ctx->dsi->dev, "JBD4040 panel disabled\n");
	return 0;
}

static int jbd4040_unprepare(struct drm_panel *panel)
{
	struct jbd4040_panel_info *ctx = panel_to_jbd4040(panel);

	if (!ctx->prepared)
		return 0;

	/* 下電順序：先關閉 AVEE 負壓 */
	if (ctx->avee_gpio)
		gpiod_set_value_cansleep(ctx->avee_gpio, 0);
	msleep(10);

	/* 復位引腳拉低 (Assert Reset, Physical 0V) */
	if (ctx->reset_gpio)
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(10);

	/* 關閉 AVDD 正壓 */
	if (ctx->avdd_gpio)
		gpiod_set_value_cansleep(ctx->avdd_gpio, 0);

	ctx->prepared = false;
	dev_info(&ctx->dsi->dev, "JBD4040 panel unprepared (AVEE turned off)\n");
	return 0;
}

static const struct drm_display_mode jbd4040_60hz_mode = {
	.hdisplay = 380,
	.hsync_start = 380 + 58,
	.hsync_end = 380 + 58 + 4,
	.htotal = 380 + 58 + 4 + 58, /* 500 */

	.vdisplay = 500,
	.vsync_start = 500 + 18,
	.vsync_end = 500 + 18 + 2,
	.vtotal = 500 + 18 + 2 + 20, /* 540 */

	.clock = 16200, /* 500 * 540 * 60 / 1000 = 16.2 MHz */
	.width_mm = 10,
	.height_mm = 13,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int jbd4040_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &jbd4040_60hz_mode);
	if (!mode) {
		dev_err(panel->dev, "Failed to duplicate display mode\n");
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	connector->display_info.bpc = 8;

	return 1;
}

static const struct drm_panel_funcs jbd4040_drm_funcs = {
	.prepare   = jbd4040_prepare,
	.unprepare = jbd4040_unprepare,
	.enable    = jbd4040_enable,
	.disable   = jbd4040_disable,
	.get_modes = jbd4040_get_modes,
};

/* =========================================================================
 * Temperature Calculation & Reading (Horner's Method)
 * ========================================================================= */
static int jbd4040_calc_temp_x100(u16 reg_value, int *temp_x100)
{
	s64 x, y;

	if (!((reg_value >> 12) & 0x01))
		return -EAGAIN;

	x = (s64)(reg_value & 0x0FFF);

	y = -108LL;
	y = (y * x) + 1736650LL;
	y = (y * x) - 14865000000LL;
	y = (y * x) + 93282900000000LL;
	y = (y * x) - 54578800000000000LL;

	*temp_x100 = (int)div64_s64(y, 10000000000000LL);
	return 0;
}

static int jbd4040_read_panel_temp(struct i2c_adapter *adap, uint8_t dev_addr, int *temp_x100)
{
	u16 temp_ctrl = 0, temp_reg = 0;

	if (!adap)
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(adap, dev_addr, REG_PVT_CTL, &temp_ctrl) == 0 && temp_ctrl == 0x0003) {
		if (jbd4040_i2c_read_reg16(adap, dev_addr, REG_PVT_DATA, &temp_reg) == 0)
			return jbd4040_calc_temp_x100(temp_reg, temp_x100);
	}
	return -EIO;
}

/* =========================================================================
 * Sysfs Attributes Implementation
 * ========================================================================= */

static ssize_t luminance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_LUM_REG, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_LUM_REG, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_LUM_REG, &valB);

	return scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
			 valR & 0x3FFF, valG & 0x3FFF, valB & 0x3FFF);
}

static ssize_t luminance_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint8_t dev_addr;
	u32 max_lum;
	uint16_t reg_cfg = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	max_lum = 8300;
	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_CFG, &reg_cfg) == 0) {
		if ((reg_cfg & 0x07) == 7)
			max_lum = 1019;
		else if ((reg_cfg & 0x07) == 6)
			max_lum = 1169;
		else if ((reg_cfg & 0x07) == 5)
			max_lum = 1368;
		else if ((reg_cfg & 0x07) == 4)
			max_lum = 2065;
		else if ((reg_cfg & 0x07) == 3)
			max_lum = 2762;
		else if ((reg_cfg & 0x07) == 2)
			max_lum = 4157;
		else if ((reg_cfg & 0x07) == 1)
			max_lum = 5551;
		else
			max_lum = 8300;
	}

	/* Support single value input: e.g. "echo 500 > luminance" */
	if (argc == 1) {
		if (kstrtou32(argv[0], 10, &val) == 0) {
			if (val > max_lum)
				val = max_lum;
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, (uint16_t)(val & 0x3FFF));
			dev_info(dev, "Set ALL panel luminance to %u\n", val);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 10, &val))
			continue;

		if (val > max_lum)
			val = max_lum;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_LUM_REG, (uint16_t)(val & 0x3FFF));
		dev_info(dev, "Set %s panel luminance to %u\n", color_str, val);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(luminance);

static ssize_t jbd4040_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_CUR, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_CUR, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_CUR, &valB);

	return scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
			 valR & 0xFF, valG & 0xFF, valB & 0xFF);
}

static ssize_t jbd4040_current_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	/* Support single value input: e.g. "echo 50 > current" */
	if (argc == 1) {
		if (kstrtou32(argv[0], 10, &val) == 0) {
			if (val > 255)
				val = 255;
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CUR, (uint16_t)(val & 0xFF));
			dev_info(dev, "Set ALL panel current to %u\n", val & 0xFF);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 10, &val))
			continue;

		if (val > 255)
			val = 255;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else if (!strcasecmp(color_str, "t")) {
			jbd4040_i2c_write_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_CFG_VID_TX_DELAY, val);
			continue;
		} else {
			continue;
		}

		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_PL_CUR, (uint16_t)(val & 0xFF));
		dev_info(dev, "Set %s panel current to %u\n", color_str, val & 0xFF);
	}

	kfree(input_free);
	return count;
}

static struct device_attribute dev_attr_current = {
	.attr = { .name = "current", .mode = 0644 },
	.show = jbd4040_current_show,
	.store = jbd4040_current_store,
};

static ssize_t temperature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	int tempR = 0, tempG = 0, tempB = 0;
	int retR = -1, retG = -1, retB = -1;
	ssize_t count = 0;
	int int_part, frac_part;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PVT_CTL, 0x0003);
	msleep(10);

	retR = jbd4040_read_panel_temp(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, &tempR);
	retG = jbd4040_read_panel_temp(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, &tempG);
	retB = jbd4040_read_panel_temp(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, &tempB);

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PVT_CTL, 0x0000);

	count += scnprintf(buf + count, PAGE_SIZE - count, "R: ");
	if (retR == 0) {
		int_part = tempR / 100;
		frac_part = abs(tempR % 100);
		if (tempR < 0 && int_part == 0)
			count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
		else
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
	} else {
		count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");
	}

	count += scnprintf(buf + count, PAGE_SIZE - count, "G: ");
	if (retG == 0) {
		int_part = tempG / 100;
		frac_part = abs(tempG % 100);
		if (tempG < 0 && int_part == 0)
			count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
		else
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
	} else {
		count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");
	}

	count += scnprintf(buf + count, PAGE_SIZE - count, "B: ");
	if (retB == 0) {
		int_part = tempB / 100;
		frac_part = abs(tempB % 100);
		if (tempB < 0 && int_part == 0)
			count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
		else
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
	} else {
		count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");
	}

	return count;
}
static DEVICE_ATTR_RO(temperature);

static ssize_t flip_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;
	bool enabled = false;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, &valR) == 0 &&
	    jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, &valG) == 0 &&
	    jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, &valB) == 0) {
		/* Normal boot has Bit 1 = 1 (R=0x3, G=0x2, B=0x3). If Bit 1 is cleared, Flip is Enabled */
		if (!(valR & 0x02) && !(valG & 0x02) && !(valB & 0x02))
			enabled = true;
	}

	return scnprintf(buf, PAGE_SIZE, "Flip is %s\n", enabled ? "Enabled" : "Disabled");
}

static ssize_t flip_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint32_t cmd_val;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtou32(buf, 0, &cmd_val))
		return -EINVAL;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, &valB);

	if (cmd_val == 1) {
		/* Enable Flip (invert from normal boot): clear Bit 1 */
		valR &= ~0x0002;
		valG &= ~0x0002;
		valB &= ~0x0002;
	} else if (cmd_val == 0) {
		/* Disable Flip (restore normal boot): set Bit 1 */
		valR |= 0x0002;
		valG |= 0x0002;
		valB |= 0x0002;
	} else {
		return -EINVAL;
	}

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, valR);
	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, valG);
	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, valB);
	return count;
}
static DEVICE_ATTR_RW(flip);

static ssize_t mirror_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;
	bool enabled = false;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, &valR) == 0 &&
	    jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, &valG) == 0 &&
	    jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, &valB) == 0) {
		/* Normal boot has R=bit0:1, G=bit0:0, B=bit0:1. If inverted (R=0, G=1, B=0), Mirror is Enabled */
		if (!(valR & 0x01) && (valG & 0x01) && !(valB & 0x01))
			enabled = true;
	}

	return scnprintf(buf, PAGE_SIZE, "Mirror is %s\n", enabled ? "Enabled" : "Disabled");
}

static ssize_t mirror_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint32_t cmd_val;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtou32(buf, 0, &cmd_val))
		return -EINVAL;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, &valB);

	if (cmd_val == 1) {
		/* Enable Mirror (invert from normal boot): R/B clear bit 0, G set bit 0 */
		valR &= ~0x0001;
		valG |= 0x0001;
		valB &= ~0x0001;
	} else if (cmd_val == 0) {
		/* Disable Mirror (restore normal boot): R/B set bit 0, G clear bit 0 */
		valR |= 0x0001;
		valG &= ~0x0001;
		valB |= 0x0001;
	} else {
		return -EINVAL;
	}

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_FLIP, valR);
	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_FLIP, valG);
	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_FLIP, valB);
	return count;
}
static DEVICE_ATTR_RW(mirror);

static ssize_t offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint32_t offsetR = 0, offsetG = 0, offsetB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_IMG_OFFSET, &offsetR);
	jbd4040_i2c_read_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_IMG_OFFSET, &offsetG);
	jbd4040_i2c_read_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_IMG_OFFSET, &offsetB);

	return scnprintf(buf, PAGE_SIZE,
			 "R(%s) H:%u, V:%u\nG(%s) H:%u, V:%u\nB(%s) H:%u, V:%u\n",
			 (offsetR & 0x1F1F) ? "enabled" : "disabled", (offsetR >> 8) & 0x1F, offsetR & 0x1F,
			 (offsetG & 0x1F1F) ? "enabled" : "disabled", (offsetG >> 8) & 0x1F, offsetG & 0x1F,
			 (offsetB & 0x1F1F) ? "enabled" : "disabled", (offsetB >> 8) & 0x1F, offsetB & 0x1F);
}

static ssize_t offset_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i = 0;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	while (i < argc) {
		char *color_str = argv[i++];
		char *en_str;
		u32 en = 0, val_h = 0, val_v = 0;
		int has_hv = 0;
		uint32_t offset_val = 0;

		if (i >= argc)
			break;
		en_str = argv[i++];
		if (kstrtou32(en_str, 10, &en))
			continue;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else
			continue;

		if (i < argc && strcasecmp(argv[i], "r") && strcasecmp(argv[i], "g") && strcasecmp(argv[i], "b")) {
			char *h_str = argv[i++];
			if (i < argc && strcasecmp(argv[i], "r") && strcasecmp(argv[i], "g") && strcasecmp(argv[i], "b")) {
				char *v_str = argv[i++];
				if (!kstrtou32(h_str, 10, &val_h) && !kstrtou32(v_str, 10, &val_v))
					has_hv = 1;
			}
		}

		if (en && has_hv) {
			offset_val = ((val_h & 0x1F) << 8) | (val_v & 0x1F);
			jbd4040_i2c_write_reg32(ctx->i2c_adap, dev_addr, REG_IMG_OFFSET, offset_val);
			dev_info(dev, "Set %s offset: 0x%04X (H:%u, V:%u)\n", color_str, offset_val, val_h, val_v);
		}
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(offset);

static ssize_t fps_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t reg_val = 0;
	uint8_t freq_idx;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_CFG, &reg_val))
		return scnprintf(buf, PAGE_SIZE, "Error\n");

	freq_idx = reg_val & 0x0007;
	if (freq_idx < ARRAY_SIZE(fps_array))
		return scnprintf(buf, PAGE_SIZE, "%u\n", fps_array[freq_idx]);

	return scnprintf(buf, PAGE_SIZE, "Unknown(%u)\n", freq_idx);
}

static ssize_t fps_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint32_t target_fps;
	uint8_t target_idx = 0xFF;
	uint16_t reg_val = 0;
	int i;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtou32(buf, 10, &target_fps))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(fps_array); i++) {
		if (fps_array[i] == target_fps) {
			target_idx = i;
			break;
		}
	}

	if (target_idx == 0xFF) {
		dev_err(dev, "Unsupported FPS: %u (Supported: 60, 90, 120, 180, 240, 360, 420, 480)\n", target_fps);
		return -EINVAL;
	}

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_CFG, &reg_val) == 0) {
		reg_val &= ~0x0007;
		reg_val |= (target_idx & 0x07);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, reg_val);
		dev_info(dev, "Set refresh rate to %u FPS\n", target_fps);
	}

	return count;
}
static DEVICE_ATTR_RW(fps);

static ssize_t bist_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;
	bool r_en = false, g_en = false, b_en = false;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SELF_TEST_CFG, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_SELF_TEST_CFG, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_SELF_TEST_CFG, &valB);

	r_en = (valR & BIT(11)) ? true : false;
	g_en = (valG & BIT(11)) ? true : false;
	b_en = (valB & BIT(11)) ? true : false;

	if (r_en && g_en && b_en)
		return scnprintf(buf, PAGE_SIZE, "White (R+G+B enabled, 60Hz)\n");
	else if (r_en && !g_en && !b_en)
		return scnprintf(buf, PAGE_SIZE, "Red (R enabled, 60Hz)\n");
	else if (!r_en && g_en && !b_en)
		return scnprintf(buf, PAGE_SIZE, "Green (G enabled, 60Hz)\n");
	else if (!r_en && !g_en && b_en)
		return scnprintf(buf, PAGE_SIZE, "Blue (B enabled, 60Hz)\n");
	else if (!r_en && !g_en && !b_en)
		return scnprintf(buf, PAGE_SIZE, "Disabled (60Hz Video Mode)\n");
	else
		return scnprintf(buf, PAGE_SIZE, "Custom (R:%d G:%d B:%d)\n", r_en, g_en, b_en);
}

static ssize_t bist_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char cmd[16];
	size_t len;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	len = min(count, sizeof(cmd) - 1);
	memcpy(cmd, buf, len);
	cmd[len] = '\0';
	strim(cmd);

	if (sysfs_streq(cmd, "r") || sysfs_streq(cmd, "red")) {
		/* Command Mode + 60Hz internal refresh */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x03e8);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);

		/* Solid Red only */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SELF_TEST_CFG, 0x0bff);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SYNC, 0x000f);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DISP_CTL, 0x0000);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DISP_CTL, 0x0000);

		ctx->bist = true;
		ctx->vid_mode = false;
		dev_info(dev, "BIST: Pure RED enabled (60Hz loop mode)\n");
	} else if (sysfs_streq(cmd, "g") || sysfs_streq(cmd, "green")) {
		/* Command Mode + 60Hz internal refresh */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x03e8);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);

		/* Solid Green only */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DISP_CTL, 0x0000);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_SELF_TEST_CFG, 0x0bff);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_SYNC, 0x000f);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DISP_CTL, 0x0000);

		ctx->bist = true;
		ctx->vid_mode = false;
		dev_info(dev, "BIST: Pure GREEN enabled (60Hz loop mode)\n");
	} else if (sysfs_streq(cmd, "b") || sysfs_streq(cmd, "blue")) {
		/* Command Mode + 60Hz internal refresh */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x03e8);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);

		/* Solid Blue only */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DISP_CTL, 0x0000);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DISP_CTL, 0x0000);

		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_SELF_TEST_CFG, 0x0bff);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_PL_SYNC, 0x000f);

		ctx->bist = true;
		ctx->vid_mode = false;
		dev_info(dev, "BIST: Pure BLUE enabled (60Hz loop mode)\n");
	} else if (sysfs_streq(cmd, "1") || sysfs_streq(cmd, "w") ||
		   sysfs_streq(cmd, "white") || sysfs_streq(cmd, "all") ||
		   sysfs_streq(cmd, "true") || sysfs_streq(cmd, "on")) {
		/* Solid White (all channels in Command Mode at 60Hz) */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x03e8);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SELF_TEST_CFG, 0x0bff);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x000f);

		ctx->bist = true;
		ctx->vid_mode = false;
		dev_info(dev, "BIST: All channels enabled (WHITE, 60Hz loop mode)\n");
	} else if (sysfs_streq(cmd, "0") || sysfs_streq(cmd, "off") ||
		   sysfs_streq(cmd, "false") || sysfs_streq(cmd, "disable")) {
		/* Turn off BIST and restore 60Hz Video Mode */
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SELF_TEST_CFG, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_CFG, 0x0008);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_LUM_REG, 0x03e8);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_DISP_CTL, 0x0001);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0001);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x000f);

		ctx->bist = false;
		ctx->vid_mode = true;
		dev_info(dev, "BIST: disabled, restored 60Hz Video Mode\n");
	} else {
		dev_err(dev, "Invalid BIST command '%s'. Supported: r, g, b, w (or 1), 0 (off)\n", cmd);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(bist);

static ssize_t vid_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t val = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_VID_MODE, &val);

	return scnprintf(buf, PAGE_SIZE, "%u (%s)\n", val & 1, (val & 1) ? "Video Mode" : "Command Mode");
}

static ssize_t vid_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	u32 val;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	if (val) {
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0001);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x000f);
		ctx->vid_mode = true;
		dev_info(dev, "Switched to Video Mode\n");
	} else {
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_VID_MODE, 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, 0x0003);
		ctx->vid_mode = false;
		dev_info(dev, "Switched to Command Mode\n");
	}
	return count;
}
static DEVICE_ATTR_RW(vid_mode);

static ssize_t refresh_loop_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t val = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SYNC, &val)) {
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, &val))
			return -EIO;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", (val >> 2) & 1);
}

static ssize_t refresh_loop_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	bool en;
	uint16_t reg_val = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtobool(buf, &en))
		return -EINVAL;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_PL_SYNC, &reg_val)) {
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, &reg_val))
			reg_val = en ? 0x000f : 0x000b;
	}

	if (en)
		reg_val |= BIT(2);
	else
		reg_val &= ~BIT(2);

	reg_val |= BIT(0); /* glb_sync pulse to latch new sync config */

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_PL_SYNC, reg_val);
	dev_info(dev, "Set refresh_loop to %u (REG_PL_SYNC=0x%04X)\n", en ? 1 : 0, reg_val);

	return count;
}
static DEVICE_ATTR_RW(refresh_loop);

static void jbd4040_trigger_sync(struct jbd4040_panel_info *ctx, uint8_t dev_addr)
{
	uint16_t sync_val = 0;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, dev_addr, REG_PL_SYNC, &sync_val) || sync_val == 0)
		sync_val = 0x000f;

	jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_PL_SYNC, sync_val | BIT(0));
}

static ssize_t glb_scan_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_GLB_SCAN_VAL, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_GLB_SCAN_VAL, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_GLB_SCAN_VAL, &valB);

	if (valR == valG && valR == valB)
		return scnprintf(buf, PAGE_SIZE, "%u (0x%04X)\n", valR, valR);

	return scnprintf(buf, PAGE_SIZE, "R: %u (0x%04X)\nG: %u (0x%04X)\nB: %u (0x%04X)\n",
			 valR, valR, valG, valG, valB, valB);
}

static ssize_t glb_scan_val_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	/* Support single value input: e.g. "echo 1000 > glb_scan_val" or "echo 0x03E8 > glb_scan_val" */
	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &val) == 0) {
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_GLB_SCAN_VAL, (uint16_t)(val & 0xFFFF));
			jbd4040_trigger_sync(ctx, JBD4040_I2C_ADDR_ALL);
			dev_info(dev, "Set ALL panel glb_scan_val to %u (0x%04X) and triggered glb_sync\n",
				 val & 0xFFFF, val & 0xFFFF);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &val))
			continue;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_GLB_SCAN_VAL, (uint16_t)(val & 0xFFFF));
		jbd4040_trigger_sync(ctx, dev_addr);
		dev_info(dev, "Set %s panel glb_scan_val to %u (0x%04X) and triggered glb_sync\n",
			 color_str, val & 0xFFFF, val & 0xFFFF);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(glb_scan_val);

/* =========================================================================
 * Gamma Control (0: disable, 1: gamma 1.0, 2.2: gamma 2.2)
 * ========================================================================= */

static ssize_t gamma_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t gam_cfg = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_GAM_CFG, &gam_cfg)) {
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_GAM_CFG, &gam_cfg))
			return -EIO;
	}

	/* Bit 8 is gam_en: 1 = enable, 0 = disable */
	if (!(gam_cfg & 0x0100))
		return scnprintf(buf, PAGE_SIZE, "0\n");

	if (ctx->gamma_val == 10)
		return scnprintf(buf, PAGE_SIZE, "1\n");
	else
		return scnprintf(buf, PAGE_SIZE, "2.2\n");
}

static ssize_t gamma_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char str[16];
	size_t len;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	strscpy(str, buf, sizeof(str));
	len = strlen(str);
	while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' '))
		str[--len] = '\0';

	if (!strcmp(str, "0") || !strcasecmp(str, "off") || !strcasecmp(str, "disable")) {
		jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_GAM_CFG, 0x0000);
		ctx->gamma_val = 0;
		dev_info(dev, "Gamma disabled (REG_GAM_CFG=0x0000)\n");
	} else if (!strcmp(str, "1") || !strcmp(str, "1.0")) {
		if (jbd4040_update_gamma(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, jbd4040_gamma_1_0_table))
			return -EIO;
		ctx->gamma_val = 10;
		dev_info(dev, "Gamma set to 1.0 (linear)\n");
	} else if (!strcmp(str, "2.2") || !strcmp(str, "2")) {
		if (jbd4040_update_gamma(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, jbd4040_gamma_2_2_table))
			return -EIO;
		ctx->gamma_val = 22;
		dev_info(dev, "Gamma set to 2.2\n");
	} else {
		dev_err(dev, "Invalid gamma value: %s (supported: 0, 1, 2.2)\n", str);
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(gamma);

/* =========================================================================
 * OSC Trim Switch & Trim Value Control (EFUSE_REG_SW & REG_OSC_TRIM)
 * ========================================================================= */

static ssize_t pl_osc_trim_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t val = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_EFUSE_REG_SW, &val)) {
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_EFUSE_REG_SW, &val))
			return -EIO;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", val & 1);
}

static ssize_t pl_osc_trim_en_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	bool en;
	uint16_t val = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (kstrtobool(buf, &en))
		return -EINVAL;

	if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_EFUSE_REG_SW, &val)) {
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_EFUSE_REG_SW, &val))
			val = 0x0002; /* default pvt_trim_sw=1 */
	}

	if (en)
		val |= BIT(0);
	else
		val &= ~BIT(0);

	jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_EFUSE_REG_SW, val);
	dev_info(dev, "Set pl_osc_trim_en to %u (REG_EFUSE_REG_SW 0x200d30 = 0x%04X)\n",
		 en ? 1 : 0, val);

	return count;
}
static DEVICE_ATTR_RW(pl_osc_trim_en);

static ssize_t pl_osc_trim_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_OSC_TRIM, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_OSC_TRIM, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_OSC_TRIM, &valB);

	if ((valR & 0xFF) == (valG & 0xFF) && (valR & 0xFF) == (valB & 0xFF))
		return scnprintf(buf, PAGE_SIZE, "%u (0x%02X)\n", valR & 0xFF, valR & 0xFF);

	return scnprintf(buf, PAGE_SIZE, "R: %u (0x%02X)\nG: %u (0x%02X)\nB: %u (0x%02X)\n",
			 valR & 0xFF, valR & 0xFF,
			 valG & 0xFF, valG & 0xFF,
			 valB & 0xFF, valB & 0xFF);
}

static ssize_t pl_osc_trim_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t trim_val;
	uint16_t cur_val = 0;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	/* Support single value input: e.g. "echo 16 > pl_osc_trim" or "echo 0x10 > pl_osc_trim" */
	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &trim_val) == 0) {
			trim_val &= 0xFF;
			if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_OSC_TRIM, &cur_val))
				cur_val = 0x0210;
			cur_val = (cur_val & 0xFF00) | (uint16_t)trim_val;
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, REG_OSC_TRIM, cur_val);
			dev_info(dev, "Set ALL panel pl_osc_trim to %u (0x%02X, full reg=0x%04X)\n",
				 trim_val, trim_val, cur_val);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &trim_val))
			continue;

		trim_val &= 0xFF;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		cur_val = 0x0210;
		jbd4040_i2c_read_reg16(ctx->i2c_adap, dev_addr, REG_OSC_TRIM, &cur_val);
		cur_val = (cur_val & 0xFF00) | (uint16_t)trim_val;
		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_OSC_TRIM, cur_val);
		dev_info(dev, "Set %s panel pl_osc_trim to %u (0x%02X, full reg=0x%04X)\n",
			 color_str, trim_val, trim_val, cur_val);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(pl_osc_trim);

/* =========================================================================
 * Demura Attributes & Table Sysfs Interface
 * ========================================================================= */

static ssize_t demura_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DMR_CFG, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DMR_CFG, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DMR_CFG, &valB);

	return scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
			 (valR >> 8) & 1, (valG >> 8) & 1, (valB >> 8) & 1);
}

static ssize_t demura_en_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t en_val;
	uint16_t cur_val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &en_val) == 0) {
			static const uint8_t addrs[3] = {
				JBD4040_I2C_ADDR_RED,
				JBD4040_I2C_ADDR_GREEN,
				JBD4040_I2C_ADDR_BLUE
			};
			int j;
			for (j = 0; j < 3; j++) {
				cur_val = 0;
				jbd4040_i2c_read_reg16(ctx->i2c_adap, addrs[j], REG_DMR_CFG, &cur_val);
				cur_val = (cur_val & ~0x0100) | (en_val ? 0x0100 : 0x0000);
				jbd4040_i2c_write_reg16(ctx->i2c_adap, addrs[j], REG_DMR_CFG, cur_val);
			}
			dev_info(dev, "Set ALL panel demura_en to %u\n", en_val ? 1 : 0);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &en_val))
			continue;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w")) {
			static const uint8_t addrs[3] = {
				JBD4040_I2C_ADDR_RED,
				JBD4040_I2C_ADDR_GREEN,
				JBD4040_I2C_ADDR_BLUE
			};
			int j;
			for (j = 0; j < 3; j++) {
				cur_val = 0;
				jbd4040_i2c_read_reg16(ctx->i2c_adap, addrs[j], REG_DMR_CFG, &cur_val);
				cur_val = (cur_val & ~0x0100) | (en_val ? 0x0100 : 0x0000);
				jbd4040_i2c_write_reg16(ctx->i2c_adap, addrs[j], REG_DMR_CFG, cur_val);
			}
			dev_info(dev, "Set ALL panel demura_en to %u\n", en_val ? 1 : 0);
			continue;
		} else {
			continue;
		}

		cur_val = 0;
		jbd4040_i2c_read_reg16(ctx->i2c_adap, dev_addr, REG_DMR_CFG, &cur_val);
		cur_val = (cur_val & ~0x0100) | (en_val ? 0x0100 : 0x0000);
		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DMR_CFG, cur_val);
		dev_info(dev, "Set %s panel demura_en to %u\n", color_str, en_val ? 1 : 0);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(demura_en);

static ssize_t demura_remap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DMR_REMAP_VAL, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DMR_REMAP_VAL, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DMR_REMAP_VAL, &valB);

	return scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
			 valR & 0x07FF, valG & 0x07FF, valB & 0x07FF);
}

static ssize_t demura_remap_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &val) == 0) {
			if (val > 2047)
				val = 2047;
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL,
						REG_DMR_REMAP_VAL, (uint16_t)(val & 0x07FF));
			dev_info(dev, "Set ALL panel demura_remap to %u\n", val);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &val))
			continue;

		if (val > 2047)
			val = 2047;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DMR_REMAP_VAL,
					(uint16_t)(val & 0x07FF));
		dev_info(dev, "Set %s panel demura_remap to %u\n", color_str, val);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(demura_remap);

static ssize_t demura_dsc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DSC_CFG, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DSC_CFG, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DSC_CFG, &valB);

	return scnprintf(buf, PAGE_SIZE,
			 "R: en=%u data=%u (raw: 0x%04x)\n"
			 "G: en=%u data=%u (raw: 0x%04x)\n"
			 "B: en=%u data=%u (raw: 0x%04x)\n",
			 (valR >> 12) & 1, valR & 0x01FF, valR,
			 (valG >> 12) & 1, valG & 0x01FF, valG,
			 (valB >> 12) & 1, valB & 0x01FF, valB);
}

static ssize_t demura_dsc_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint16_t cur_val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &val) == 0) {
			if (val > 0x1FF) {
				jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL,
							REG_DSC_CFG, (uint16_t)(val & 0x11FF));
			} else {
				static const uint8_t addrs[3] = {
					JBD4040_I2C_ADDR_RED,
					JBD4040_I2C_ADDR_GREEN,
					JBD4040_I2C_ADDR_BLUE
				};
				int j;
				for (j = 0; j < 3; j++) {
					cur_val = 0x0100;
					jbd4040_i2c_read_reg16(ctx->i2c_adap, addrs[j], REG_DSC_CFG, &cur_val);
					cur_val = (cur_val & ~0x01FF) | (val & 0x01FF);
					jbd4040_i2c_write_reg16(ctx->i2c_adap, addrs[j], REG_DSC_CFG, cur_val);
				}
			}
			dev_info(dev, "Set ALL panel demura_dsc to 0x%04x\n", val);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &val))
			continue;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		if (dev_addr == JBD4040_I2C_ADDR_ALL) {
			if (val > 0x1FF) {
				jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DSC_CFG,
							(uint16_t)(val & 0x11FF));
			} else {
				static const uint8_t addrs[3] = {
					JBD4040_I2C_ADDR_RED,
					JBD4040_I2C_ADDR_GREEN,
					JBD4040_I2C_ADDR_BLUE
				};
				int j;
				for (j = 0; j < 3; j++) {
					cur_val = 0x0100;
					jbd4040_i2c_read_reg16(ctx->i2c_adap, addrs[j], REG_DSC_CFG, &cur_val);
					cur_val = (cur_val & ~0x01FF) | (val & 0x01FF);
					jbd4040_i2c_write_reg16(ctx->i2c_adap, addrs[j], REG_DSC_CFG, cur_val);
				}
			}
		} else {
			if (val > 0x1FF) {
				jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DSC_CFG,
							(uint16_t)(val & 0x11FF));
			} else {
				cur_val = 0x0100;
				jbd4040_i2c_read_reg16(ctx->i2c_adap, dev_addr, REG_DSC_CFG, &cur_val);
				cur_val = (cur_val & ~0x01FF) | (val & 0x01FF);
				jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DSC_CFG, cur_val);
			}
		}
		dev_info(dev, "Set %s panel demura_dsc to 0x%04x\n", color_str, val);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(demura_dsc);

static ssize_t demura_seg_th_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t valR = 0, valG = 0, valB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_DMR_SEG_TH, &valR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_DMR_SEG_TH, &valG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_DMR_SEG_TH, &valB);

	return scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
			 valR & 0x03FF, valG & 0x03FF, valB & 0x03FF);
}

static ssize_t demura_seg_th_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	char *input, *input_free, *token;
	char *argv[DEF_I2C_JBD4040_MAX_ARGS];
	int argc = 0, i;
	uint32_t val;
	uint8_t dev_addr;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	input = kstrndup(buf, count, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	input_free = input;

	while ((token = strsep(&input, " \t\n:")) != NULL) {
		if (*token == '\0')
			continue;
		if (argc >= DEF_I2C_JBD4040_MAX_ARGS)
			break;
		argv[argc++] = token;
	}

	if (argc == 1) {
		if (kstrtou32(argv[0], 0, &val) == 0) {
			if (val > 1023)
				val = 1023;
			jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL,
						REG_DMR_SEG_TH, (uint16_t)(val & 0x03FF));
			dev_info(dev, "Set ALL panel demura_seg_th to %u\n", val);
			kfree(input_free);
			return count;
		}
	}

	if (argc == 0 || argc % 2 != 0) {
		kfree(input_free);
		return -EINVAL;
	}

	for (i = 0; i < argc; i += 2) {
		char *color_str = argv[i];
		char *val_str = argv[i+1];

		if (kstrtou32(val_str, 0, &val))
			continue;

		if (val > 1023)
			val = 1023;

		if (!strcasecmp(color_str, "r"))
			dev_addr = JBD4040_I2C_ADDR_RED;
		else if (!strcasecmp(color_str, "g"))
			dev_addr = JBD4040_I2C_ADDR_GREEN;
		else if (!strcasecmp(color_str, "b"))
			dev_addr = JBD4040_I2C_ADDR_BLUE;
		else if (!strcasecmp(color_str, "all") || !strcasecmp(color_str, "w"))
			dev_addr = JBD4040_I2C_ADDR_ALL;
		else
			continue;

		jbd4040_i2c_write_reg16(ctx->i2c_adap, dev_addr, REG_DMR_SEG_TH,
					(uint16_t)(val & 0x03FF));
		dev_info(dev, "Set %s panel demura_seg_th to %u\n", color_str, val);
	}

	kfree(input_free);
	return count;
}
static DEVICE_ATTR_RW(demura_seg_th);

static ssize_t demura_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	uint16_t statR = 0, statG = 0, statB = 0;
	uint16_t codeR = 0, codeG = 0, codeB = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_FMC_STATUS, &statR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_FMC_STATUS, &statG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_FMC_STATUS, &statB);

	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, REG_ST_CODE3, &codeR);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_GREEN, REG_ST_CODE3, &codeG);
	jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_BLUE, REG_ST_CODE3, &codeB);

	return scnprintf(buf, PAGE_SIZE,
			 "R: status=0x%04x (flash_done=%u, dmr1_err=%u, dmr2_err=%u), cks_dmr1=0x%02x, cks_dmr2=0x%02x\n"
			 "G: status=0x%04x (flash_done=%u, dmr1_err=%u, dmr2_err=%u), cks_dmr1=0x%02x, cks_dmr2=0x%02x\n"
			 "B: status=0x%04x (flash_done=%u, dmr1_err=%u, dmr2_err=%u), cks_dmr1=0x%02x, cks_dmr2=0x%02x\n",
			 statR, statR & 1, (statR >> 4) & 1, (statR >> 5) & 1, codeR & 0xFF, (codeR >> 8) & 0xFF,
			 statG, statG & 1, (statG >> 4) & 1, (statG >> 5) & 1, codeG & 0xFF, (codeG >> 8) & 0xFF,
			 statB, statB & 1, (statB >> 4) & 1, (statB >> 5) & 1, codeB & 0xFF, (codeB >> 8) & 0xFF);
}
static DEVICE_ATTR_RO(demura_status);

/* Demura Binary Table Attributes */
static ssize_t jbd4040_demura_table_read_channel(struct file *filp, struct kobject *kobj,
						 struct bin_attribute *bin_attr,
						 char *buf, loff_t off, size_t count,
						 uint8_t dev_addr)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	size_t remaining;
	size_t done = 0;
	int ret = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	if (off >= JBD4040_DEMURA_TABLE_SIZE)
		return 0;

	if (off + count > JBD4040_DEMURA_TABLE_SIZE)
		count = JBD4040_DEMURA_TABLE_SIZE - off;

	remaining = count;
	while (remaining > 0) {
		size_t chunk_len = min_t(size_t, remaining, 1024);
		uint32_t reg_addr = JBD4040_DEMURA_TABLE_ADDR + off + done;

		ret = jbd4040_i2c_read_block(ctx->i2c_adap, dev_addr,
					     reg_addr, (uint8_t *)(buf + done), chunk_len);
		if (ret < 0) {
			pr_err("[JBD4040] Demura table read failed at off 0x%llx (dev 0x%02x): %d\n",
			       off + done, dev_addr, ret);
			return done ? done : ret;
		}

		done += chunk_len;
		remaining -= chunk_len;
	}

	return done;
}

static ssize_t demura_table_r_read(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr,
				   char *buf, loff_t off, size_t count)
{
	return jbd4040_demura_table_read_channel(filp, kobj, bin_attr, buf, off, count,
						JBD4040_I2C_ADDR_RED);
}
static BIN_ATTR_RO(demura_table_r, JBD4040_DEMURA_TABLE_SIZE);

static ssize_t demura_table_g_read(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr,
				   char *buf, loff_t off, size_t count)
{
	return jbd4040_demura_table_read_channel(filp, kobj, bin_attr, buf, off, count,
						JBD4040_I2C_ADDR_GREEN);
}
static BIN_ATTR_RO(demura_table_g, JBD4040_DEMURA_TABLE_SIZE);

static ssize_t demura_table_b_read(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr,
				   char *buf, loff_t off, size_t count)
{
	return jbd4040_demura_table_read_channel(filp, kobj, bin_attr, buf, off, count,
						JBD4040_I2C_ADDR_BLUE);
}
static BIN_ATTR_RO(demura_table_b, JBD4040_DEMURA_TABLE_SIZE);

static struct bin_attribute *jbd4040_bin_attrs[] = {
	&bin_attr_demura_table_r,
	&bin_attr_demura_table_g,
	&bin_attr_demura_table_b,
	NULL,
};

static struct attribute *jbd4040_attrs[] = {
	&dev_attr_luminance.attr,
	&dev_attr_current.attr,
	&dev_attr_temperature.attr,
	&dev_attr_flip.attr,
	&dev_attr_mirror.attr,
	&dev_attr_offset.attr,
	&dev_attr_fps.attr,
	&dev_attr_bist.attr,
	&dev_attr_vid_mode.attr,
	&dev_attr_refresh_loop.attr,
	&dev_attr_glb_scan_val.attr,
	&dev_attr_gamma.attr,
	&dev_attr_pl_osc_trim_en.attr,
	&dev_attr_pl_osc_trim.attr,
	&dev_attr_demura_en.attr,
	&dev_attr_demura_remap.attr,
	&dev_attr_demura_dsc.attr,
	&dev_attr_demura_seg_th.attr,
	&dev_attr_demura_status.attr,
	NULL,
};

static const struct attribute_group jbd4040_attr_group = {
	.attrs = jbd4040_attrs,
	.bin_attrs = jbd4040_bin_attrs,
};

/* =========================================================================
 * Character Device /dev/jbd4040 Operations
 * ========================================================================= */

static long jbd4040_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct jbd4040_panel_info *ctx = g_jbd4040_ctx;
	struct i2c_ioctl_data data;
	uint16_t val16 = 0;
	uint32_t val32 = 0;
	long ret = 0;

	if (!ctx || IS_ERR_OR_NULL(ctx->i2c_adap))
		return -ENODEV;

	mutex_lock(&jbd4040_mutex);

	switch (cmd) {
	case IOCTL_READ_REG32:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			ret = -EFAULT;
			break;
		}
		if (jbd4040_i2c_read_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, data.reg, &val32)) {
			ret = -EIO;
			break;
		}
		data.val = val32;
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			ret = -EFAULT;
		break;

	case IOCTL_WRITE_REG32:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			ret = -EFAULT;
			break;
		}
		if (jbd4040_i2c_write_reg32(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, data.reg, data.val))
			ret = -EIO;
		break;

	case IOCTL_READ_REG16:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			ret = -EFAULT;
			break;
		}
		if (jbd4040_i2c_read_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_RED, data.reg, &val16)) {
			ret = -EIO;
			break;
		}
		data.val = val16;
		if (copy_to_user((void __user *)arg, &data, sizeof(data)))
			ret = -EFAULT;
		break;

	case IOCTL_WRITE_REG16:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			ret = -EFAULT;
			break;
		}
		if (jbd4040_i2c_write_reg16(ctx->i2c_adap, JBD4040_I2C_ADDR_ALL, data.reg, (uint16_t)data.val))
			ret = -EIO;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&jbd4040_mutex);
	return ret;
}

static int jbd4040_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int jbd4040_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations jbd4040_fops = {
	.owner          = THIS_MODULE,
	.open           = jbd4040_open,
	.release        = jbd4040_release,
	.unlocked_ioctl = jbd4040_ioctl,
};

static struct i2c_client *jbd4040_find_i2c_client(struct device *dev)
{
	struct device_node *i2c_bus_np = NULL;
	struct device_node *child = NULL;
	struct i2c_client *client = NULL;

	if (dev->of_node) {
		i2c_bus_np = of_parse_phandle(dev->of_node, "ddc-i2c-bus", 0);
		if (!i2c_bus_np)
			i2c_bus_np = of_parse_phandle(dev->of_node, "i2c-bus", 0);
	}

	if (!i2c_bus_np)
		return NULL;

	for_each_child_of_node(i2c_bus_np, child) {
		if (of_device_is_compatible(child, "gis,jbd4040_i2c_neo")) {
			client = of_find_i2c_device_by_node(child);
			of_node_put(child);
			break;
		}
	}
	of_node_put(i2c_bus_np);
	return client;
}

/* =========================================================================
 * Probe & Remove
 * ========================================================================= */

static int jbd4040_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct jbd4040_panel_info *ctx;
	struct device_node *i2c_node;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->gamma_val = 22; /* Default is gamma 2.2 */
	g_jbd4040_ctx = ctx;

	/* 1. Request GPIOs from Device Tree */
	ctx->avdd_gpio = devm_gpiod_get_optional(dev, "avdd", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->avdd_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->avdd_gpio), "Failed to get avdd gpio\n");

	ctx->avee_gpio = devm_gpiod_get_optional(dev, "avee", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->avee_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->avee_gpio), "Failed to get avee gpio\n");

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "Failed to get reset gpio\n");

	/* 2. Bind I2C bus via ddc-i2c-bus property */
	i2c_node = of_parse_phandle(dev->of_node, "ddc-i2c-bus", 0);
	if (i2c_node) {
		ctx->i2c_adap = of_find_i2c_adapter_by_node(i2c_node);
		of_node_put(i2c_node);
		if (IS_ERR_OR_NULL(ctx->i2c_adap))
			return dev_err_probe(dev, -EPROBE_DEFER, "Waiting for I2C adapter\n");
	} else {
		dev_warn(dev, "No ddc-i2c-bus specified in DTS\n");
	}

	/* 3. Configure DSI Device for Video Mode */
	dsi->lanes = 1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM;

	/* 4. Register DRM Panel */
	drm_panel_init(&ctx->panel, dev, &jbd4040_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	/* 5. Attach DSI Device to Host */
	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		if (ctx->i2c_adap) put_device(&ctx->i2c_adap->dev);
		return ret;
	}

	/* 6. Register /dev/jbd4040 character device */
	ret = alloc_chrdev_region(&jbd4040_dev_num, 0, 1, JBD4040_DEV_NAME);
	if (ret >= 0) {
		cdev_init(&jbd4040_cdev, &jbd4040_fops);
		ret = cdev_add(&jbd4040_cdev, jbd4040_dev_num, 1);
		if (ret < 0) {
			dev_err(dev, "Failed to add cdev: %d\n", ret);
			unregister_chrdev_region(jbd4040_dev_num, 1);
		} else {
			jbd4040_class = class_create(JBD4040_CLASS_NAME);
			if (IS_ERR(jbd4040_class)) {
				dev_err(dev, "Failed to create class %s\n", JBD4040_CLASS_NAME);
				cdev_del(&jbd4040_cdev);
				unregister_chrdev_region(jbd4040_dev_num, 1);
			} else {
				jbd4040_dev = device_create(jbd4040_class, NULL, jbd4040_dev_num, NULL, JBD4040_DEV_NAME);
				if (IS_ERR(jbd4040_dev)) {
					dev_err(dev, "Failed to create /dev/%s\n", JBD4040_DEV_NAME);
				} else {
					ret = sysfs_create_group(&jbd4040_dev->kobj, &jbd4040_attr_group);
					if (ret)
						dev_warn(dev, "Failed to create sysfs group on cdev: %d\n", ret);
					dev_info(dev, "Created /dev/%s character device\n", JBD4040_DEV_NAME);
				}
			}
		}
	}

	/* 7. Expose sysfs attributes on DSI device */
	ret = sysfs_create_group(&dev->kobj, &jbd4040_attr_group);
	if (ret)
		dev_warn(dev, "Failed to create sysfs group on DSI device: %d\n", ret);

	/* 8. Attach sysfs attributes to I2C client (/sys/bus/i2c/devices/2-0058/) */
	ctx->i2c_client = jbd4040_find_i2c_client(dev);
	if (ctx->i2c_client) {
		ret = sysfs_create_group(&ctx->i2c_client->dev.kobj, &jbd4040_attr_group);
		if (ret)
			dev_warn(dev, "Failed to attach sysfs to I2C client %s\n", dev_name(&ctx->i2c_client->dev));
		else
			dev_info(dev, "Attached sysfs to I2C client %s\n", dev_name(&ctx->i2c_client->dev));
	}

	dev_info(dev, "JBD4040 DRM MIPI Video Mode panel driver probed successfully\n");
	return 0;
}

static void jbd4040_panel_remove(struct mipi_dsi_device *dsi)
{
	struct jbd4040_panel_info *ctx = mipi_dsi_get_drvdata(dsi);

	if (ctx->i2c_client) {
		sysfs_remove_group(&ctx->i2c_client->dev.kobj, &jbd4040_attr_group);
		put_device(&ctx->i2c_client->dev);
		ctx->i2c_client = NULL;
	}
	sysfs_remove_group(&dsi->dev.kobj, &jbd4040_attr_group);

	if (!IS_ERR_OR_NULL(jbd4040_dev)) {
		sysfs_remove_group(&jbd4040_dev->kobj, &jbd4040_attr_group);
		device_destroy(jbd4040_class, jbd4040_dev_num);
		jbd4040_dev = NULL;
	}
	if (!IS_ERR_OR_NULL(jbd4040_class)) {
		class_destroy(jbd4040_class);
		jbd4040_class = NULL;
	}
	cdev_del(&jbd4040_cdev);
	unregister_chrdev_region(jbd4040_dev_num, 1);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	if (ctx->i2c_adap) put_device(&ctx->i2c_adap->dev);
	g_jbd4040_ctx = NULL;
}

static const struct of_device_id jbd4040_of_match[] = {
	{ .compatible = "gis,jbd4040-cm-60hz" },
	{ .compatible = "gis,jbd4040_60hz" },
	{ .compatible = "gis,jbd4040-60hz" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jbd4040_of_match);

static struct mipi_dsi_driver jbd4040_panel_driver = {
	.probe  = jbd4040_panel_probe,
	.remove = jbd4040_panel_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = jbd4040_of_match,
	},
};
module_mipi_dsi_driver(jbd4040_panel_driver);

MODULE_AUTHOR("GIS / JBD");
MODULE_DESCRIPTION("JBD4040 MicroLED DRM MIPI Video Mode Panel Driver");
MODULE_LICENSE("GPL");

