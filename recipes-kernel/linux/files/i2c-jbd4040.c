#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/math64.h> /* 必須加入，提供 64位元安全的 div64_s64 */
#include <linux/math.h>   /* 提供 abs() 函數 */
#include <linux/delay.h>  /* 提供 msleep() */

#include <linux/gpio/consumer.h> /* 【新增】GPIO 描述符子系統 */
#include <linux/delay.h>         /* 【新增】提供 msleep 延時 */

/* 【新增】用來包裝 5 根 GPIO 描述符的私有結構體 */
struct jbd4040_priv {
    struct gpio_desc *gpiod_dvdd;
    struct gpio_desc *gpiod_vddi;
    struct gpio_desc *gpiod_avdd;
    struct gpio_desc *gpiod_avee;
    struct gpio_desc *gpiod_reset;
};

/* 全域指標，方便給 probe 以外的地方存取（或維持你原有的設計） */
static struct jbd4040_priv *g_priv = NULL;

/* ========================================================
 * 提前宣告 I2C 讀寫函式，避免 Implicit Declaration 錯誤
 * ======================================================== */
static int i2c_read_reg16(struct i2c_client *client, uint32_t reg, uint16_t *val);
static int i2c_write_reg16(struct i2c_client *client, uint32_t reg, uint16_t val);


#define DEVICE_NAME "jbd4040"
#define CLASS_NAME "gis"

#define IOCTL_READ_REG32    _IOR('M', 0x01, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG32   _IOW('M', 0x02, struct i2c_ioctl_data)

#define IOCTL_LUMIN_GET     _IOR('R', 0x01, struct jbd4040_luminance)
#define IOCTL_CURRENT_GET   _IOR('R', 0x02, struct jbd4040_current)
#define IOCTL_TEMP_READ     _IOR('R', 0x03, struct jbd4040_temp)
#define IOCTL_FLIP_GET      _IOR('R', 0x04, struct jbd4040_flip)
#define IOCTL_MIRROR_GET    _IOR('R', 0x05, struct jbd4040_mirror)
#define IOCTL_OFFSET_GET    _IOR('R', 0x06, struct jbd4040_offset)

#define IOCTL_LUMIN_SET     _IOW('W', 0x01, struct jbd4040_luminance)
#define IOCTL_CURRENT_SET   _IOW('W', 0x02, struct jbd4040_current)
#define IOCTL_FLIP_SET      _IOW('W', 0x04, struct jbd4040_flip)
#define IOCTL_MIRROR_SET    _IOW('W', 0x05, struct jbd4040_mirror)
#define IOCTL_OFFSET_SET    _IOW('W', 0x06, struct jbd4040_offset)


/* 面板群組 I2C 位址定義 */
#define JBD4040_I2C_ADDR_ALL    0x58  /* 廣播位址 */
#define JBD4040_I2C_ADDR_RED    0x59  /* 紅光面板 */
#define JBD4040_I2C_ADDR_GREEN  0x5A  /* 綠光面板 */
#define JBD4040_I2C_ADDR_BLUE   0x5B  /* 藍光面板 */

/* 晶片 ID 暫存器位址 (24-bit) */
#define REG_CHIP_ID0            0x200000
#define REG_CHIP_ID1            0x200002

#define JBD4040_GAMMA_TABLE_0_ADDR 0x250000
#define JBD4040_GAMMA_EN_REG       0x200200

/* * JBD4040 Gamma 2.2 Table (10-bit precision, Max 1023)
 * 由 Python 公式預先計算: round(((i / 255.0) ** 2.2) * 1023)
 */
static const uint16_t gamma_2_2_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 
    4, 4, 5, 5, 6, 7, 7, 8, 9, 10, 11, 11, 12, 13, 14, 15, 
    17, 18, 19, 20, 21, 23, 24, 25, 27, 28, 30, 31, 33, 34, 36, 38, 
    40, 41, 43, 45, 47, 49, 51, 53, 56, 58, 60, 62, 65, 67, 70, 72, 
    75, 77, 80, 83, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 115, 118, 
    122, 125, 128, 132, 135, 139, 142, 146, 150, 153, 157, 161, 165, 169, 173, 177, 
    181, 185, 189, 193, 198, 202, 206, 211, 215, 220, 224, 229, 234, 238, 243, 248, 
    253, 258, 263, 268, 273, 278, 283, 288, 294, 299, 304, 310, 315, 321, 327, 332, 
    338, 344, 349, 355, 361, 367, 373, 379, 385, 391, 397, 403, 410, 416, 422, 429, 
    435, 442, 448, 455, 461, 468, 475, 482, 488, 495, 502, 509, 516, 524, 531, 538, 
    545, 553, 560, 567, 575, 583, 590, 598, 606, 614, 621, 629, 637, 645, 653, 661, 
    669, 678, 686, 694, 702, 711, 719, 728, 736, 745, 754, 762, 771, 780, 789, 798, 
    807, 816, 825, 834, 843, 852, 861, 871, 880, 889, 899, 908, 918, 928, 937, 947, 
    957, 966, 976, 986, 996, 1006, 1016, 1023
};



/*
 * 將 JBD4040 暫存器 0x200404 的原始值轉換為溫度
 * 回傳值: 攝氏溫度乘以 100 (例如 25.43度 會回傳 2543)
 */
static int jbd4040_calc_temp_x100(u16 reg_value, int *temp_x100)
{
    s64 x, y;

    /* 1. 檢查有效位 (Bit 12: PVT_DONE / Valid) */
    if (!((reg_value >> 12) & 0x01)) {
        return -EAGAIN; /* 溫度資料尚未準備好 */
    }

    /* 2. 提取原始碼 (Bit 11:0) */
    x = (s64)(reg_value & 0x0FFF);

    /* 3. Horner's Method 多項式運算 (係數放大 10^15 倍)
     * a4 = -1.08168e-13 -> -108
     * a3 = 1.73665e-09  -> 1736650
     * a2 = -1.48650e-05 -> -14865000000
     * a1 = 9.32829e-02  -> 93282900000000
     * a0 = -5.45788e+01 -> -54578800000000000
     */
    y = -108LL;
    y = (y * x) + 1736650LL;
    y = (y * x) - 14865000000LL;
    y = (y * x) + 93282900000000LL;
    y = (y * x) - 54578800000000000LL;

    /* 4. 除以 10^13，得出放大了 100 倍的溫度值 (保留兩位小數精度) */
    *temp_x100 = (int)div64_s64(y, 10000000000000LL);

    return 0;
}

/* * 讀取單一面板溫度的流程封裝
 */
static int read_panel_temp(struct i2c_client *client, int *temp_x100)
{
    u16 temp_ctrl = 0, temp_reg = 0;

    if (!client) return -ENODEV;

    /* 確認 enable 狀態 (這時應該要是 0x0003) */
    if (i2c_read_reg16(client, 0x200402, &temp_ctrl) == 0 && temp_ctrl == 0x0003) {
        /* 讀取溫度原始數據 */
        if (i2c_read_reg16(client, 0x200404, &temp_reg) == 0) {
            /* 進行轉換 */
            return jbd4040_calc_temp_x100(temp_reg, temp_x100);
        }
    }
    return -EIO;
}


// old jbd4020 start
#if 0
#define REG_BRIGHTNESS      0x0302ae38
#define REG_CURRENT         0x0302ae90
#define REG_TEMPERATURE     0x0200300c
#define REG_FLIP_SRAM       0x03030004
#define REG_MIRROR_SRAM     0x03030004
#define REG_OFFSET_SRAM     0x03030008
#define REG_FMO_VO          0x0300015c

#define MASK_BRIGHTNESS     0x00001fff
#define MASK_CURRENT        0x000000ff
#define MASK_TEMPERATURE    0x000003ff
#define MASK_MIRROR_SRAM    0x00000001
#define MASK_FLIP_SRAM      0x00000002
#define MASK_OFFSET_SRAM    0x00000001
#define MASK_OFFSET_H_SRAM  0x000001f0
#define MASK_OFFSET_V_SRAM  0x0001f000
#define MASK_OFFSET_HV_SRAM (MASK_OFFSET_V_SRAM | MASK_OFFSET_H_SRAM)
#define MASK_FLIP_VO        0x80000000
#define MASK_MIRROR_VO      0x40000000
#define MASK_OFFSET_VO      0x20000000
#define MASK_OFFSET_H_VO    0x00000fff
#define MASK_OFFSET_V_VO    0x0fff0000
#define MASK_OFFSET_HV_VO   (MASK_OFFSET_V_VO | MASK_OFFSET_H_VO)
#endif
// old jbd4020 end

// Structure for IOCTL data transfer
struct i2c_ioctl_data {
    uint32_t reg;
    uint32_t val;
};

struct jbd4040_luminance {
    u16 r;
    u16 g;
    u16 b;
    u8 status;
};

struct jbd4040_current {
    u8 r;
    u8 g;
    u8 b;
    u8 status;
};

struct jbd4040_temp {
    u32 r;
    u32 g;
    u32 b;
    u8 status;
};

struct jbd4040_flip {
    u8 en;
    u8 status;
};

struct jbd4040_mirror {
    u8 en;
    u8 status;
};

struct jbd4040_offset {
    u8 en;
    u16 rh;
    u16 rv;
    u16 gh;
    u16 gv;
    u16 bh;
    u16 bv;
    u8 status;
};

static struct i2c_client clientR;
static struct i2c_client clientG;
static struct i2c_client clientB;

static struct i2c_client *gClientR;
static struct i2c_client *gClientG;
static struct i2c_client *gClientB;
static dev_t dev_num;
static struct cdev jbd4040_cdev;
static struct class *jbd4040_class;
static DEFINE_MUTEX(jbd4040_mutex);


#include <linux/i2c.h>

/* =========================================================================
 * 16-bit 暫存器讀寫 (符合 Figure 3-23 / Figure 3-24 時序圖)
 * ========================================================================= */

/* * I2C 16-bit Write
 * 時序：[Addr 23:16] [Addr 15:8] [Addr 7:0] [Data 7:0] [Data 15:8]
 */
static int i2c_write_reg16(struct i2c_client *client, uint32_t reg, uint16_t val)
{
    uint8_t buf[5];
    struct i2c_msg msg;

    /* 1. 24-bit 暫存器位址 (Big-Endian) */
    buf[0] = (reg >> 16) & 0xFF;  
    buf[1] = (reg >> 8) & 0xFF;   
    buf[2] = reg & 0xFF;          

    /* 2. 16-bit 資料 (Little-Endian，低位組先發) */
    buf[3] = val & 0xFF;          
    buf[4] = (val >> 8) & 0xFF;   

    msg.addr = client->addr;
    msg.flags = 0;                /* Write */
    msg.len = 5;                  
    msg.buf = buf;

    if (i2c_transfer(client->adapter, &msg, 1) != 1) {
        dev_err(&client->dev, "I2C Write 16-bit failed! reg: 0x%06X\n", reg);
        return -EIO;
    }
    return 0;
}

/* * I2C 16-bit Read
 * 時序：Write [Addr 23:16, 15:8, 7:0] -> Sr (Repeated Start) -> Read [Data 7:0, 15:8]
 */
/* 強制拆分傳輸的 16-bit 讀取 */
static int i2c_read_reg16(struct i2c_client *client, uint32_t reg, uint16_t *val)
{
    uint8_t addr_buf[3];
    uint8_t data_buf[2] = {0};
    struct i2c_msg msg_write, msg_read;

    /* 1. 準備 24-bit 位址 */
    addr_buf[0] = (reg >> 16) & 0xFF;  
    addr_buf[1] = (reg >> 8) & 0xFF;   
    addr_buf[2] = reg & 0xFF;          

    /* 2. 第一階段：寫入暫存器位址 */
    msg_write.addr = client->addr;
    msg_write.flags = 0;                 /* 0 代表 Write */
    msg_write.len = 3;                   
    msg_write.buf = addr_buf;

    /* 💡 關鍵：只傳入 1 個 msg，結束時核心會自動發出 STOP (P) 訊號 */
    if (i2c_transfer(client->adapter, &msg_write, 1) != 1) {
        dev_err(&client->dev, "I2C Read16 Stage 1 (Write Addr) failed!\n");
        return -EIO;
    }

    /* 3. 第二階段：發起全新交易讀取資料 */
    msg_read.addr = client->addr;
    msg_read.flags = I2C_M_RD;          /* I2C_M_RD 代表 Read */
    msg_read.len = 2;                   
    msg_read.buf = data_buf;

    /* 💡 關鍵：發起全新 START (S)，讀取完發出 STOP (P) */
    if (i2c_transfer(client->adapter, &msg_read, 1) != 1) {
        dev_err(&client->dev, "I2C Read16 Stage 2 (Read Data) failed!\n");
        return -EIO;
    }

    /* 4. 組合 16-bit 數值 (Little-Endian) */
    *val = (data_buf[1] << 8) | data_buf[0];
    printk("val : 0x%x\n", val);
    return 0;
}

/* =========================================================================
 * 32-bit 暫存器讀寫 (對應 Python 中的 write_32bit_data)
 * ========================================================================= */

/* * I2C 32-bit Write
 * 時序：[Addr 23:16] [Addr 15:8] [Addr 7:0] [Data 7:0] [Data 15:8] [Data 23:16] [Data 31:24]
 */
static int i2c_write_reg32(struct i2c_client *client, uint32_t reg, uint32_t val)
{
    uint8_t buf[7]; 
    struct i2c_msg msg;

    /* 1. 24-bit 暫存器位址 (Big-Endian) */
    buf[0] = (reg >> 16) & 0xFF;
    buf[1] = (reg >> 8) & 0xFF;
    buf[2] = reg & 0xFF;

    /* 2. 32-bit 資料 (Little-Endian，低位組先發) */
    buf[3] = val & 0xFF;
    buf[4] = (val >> 8) & 0xFF;
    buf[5] = (val >> 16) & 0xFF;
    buf[6] = (val >> 24) & 0xFF;

    msg.addr = client->addr;
    msg.flags = 0;                /* Write */
    msg.len = 7;
    msg.buf = buf;

    if (i2c_transfer(client->adapter, &msg, 1) != 1) {
        dev_err(&client->dev, "I2C Write 32-bit failed! reg: 0x%06X\n", reg);
        return -EIO;
    }
    return 0;
}

/* * I2C 32-bit Read
 * 時序：Write [Addr 23:16, 15:8, 7:0] -> Sr -> Read [Data 7:0, 15:8, 23:16, 31:24]
 */
/* 強制拆分傳輸的 32-bit 讀取 */
static int i2c_read_reg32(struct i2c_client *client, uint32_t reg, uint32_t *val)
{
    uint8_t addr_buf[3];
    uint8_t data_buf[4] = {0};
    struct i2c_msg msg_write, msg_read;

    /* 1. 準備 24-bit 位址 */
    addr_buf[0] = (reg >> 16) & 0xFF;
    addr_buf[1] = (reg >> 8) & 0xFF;
    addr_buf[2] = reg & 0xFF;

    /* 2. 第一階段：寫入暫存器位址 */
    msg_write.addr = client->addr;
    msg_write.flags = 0;                 
    msg_write.len = 3;                   
    msg_write.buf = addr_buf;

    /* 發送並產生 STOP (P) */
    if (i2c_transfer(client->adapter, &msg_write, 1) != 1) {
        dev_err(&client->dev, "I2C Read32 Stage 1 (Write Addr) failed!\n");
        return -EIO;
    }

    /* 3. 第二階段：發起全新交易讀取資料 */
    msg_read.addr = client->addr;
    msg_read.flags = I2C_M_RD;          
    msg_read.len = 4;                   
    msg_read.buf = data_buf;

    /* 發起全新 START (S) */
    if (i2c_transfer(client->adapter, &msg_read, 1) != 1) {
        dev_err(&client->dev, "I2C Read32 Stage 2 (Read Data) failed!\n");
        return -EIO;
    }

    /* 4. 組合 32-bit 數值 (Little-Endian) */
    *val = (data_buf[3] << 24) | (data_buf[2] << 16) | (data_buf[1] << 8) | data_buf[0];
    return 0;
}


// --------- Character Device IOCTL ---------
static long jbd4040_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    mutex_lock(&jbd4040_mutex);

    switch (cmd) {
        case IOCTL_READ_REG32: 
            {
                struct i2c_ioctl_data data;

                if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_read_reg32(gClientR, data.reg, &data.val)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
            }
            break;

        case IOCTL_WRITE_REG32:
            {
                struct i2c_ioctl_data data;

                if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_write_reg32(gClientR, data.reg, data.val)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
            }
            break;

        case IOCTL_LUMIN_GET:
            {
                struct jbd4040_luminance lumin;
                uint32_t temp;
#if 0
                lumin.status = 0;
                if (i2c_read_reg32(gClientR, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                }
                lumin.r = temp & MASK_BRIGHTNESS;

                if (i2c_read_reg32(gClientG, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                }
                lumin.g = temp & MASK_BRIGHTNESS;

                if (i2c_read_reg32(gClientB, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                }
                lumin.b = temp & MASK_BRIGHTNESS;

                if (copy_to_user((void __user*)arg, &lumin, sizeof(lumin))) {
                    ret = -EFAULT;
                }
                
                if (0 != ret) {
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_LUMIN_SET:
            {
                struct jbd4040_luminance lumin;
                uint32_t temp;
#if 0
                lumin.status = 0;
                if (copy_from_user(&lumin, (void __user *)arg, sizeof(lumin))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.r & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientR, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_read_reg32(gClientG, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.g & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientG, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_read_reg32(gClientB, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.b & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientB, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &lumin, sizeof(lumin))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif 
            }
            break;

        case IOCTL_CURRENT_GET:
            {
                struct jbd4040_current curr;
                uint32_t temp;
#if 0
                curr.status = 0;
                if (i2c_read_reg32(gClientR, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                curr.r = temp & MASK_CURRENT;

                if (i2c_read_reg32(gClientG, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                curr.g = temp & MASK_CURRENT;

                if (i2c_read_reg32(gClientB, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                curr.b = temp & MASK_CURRENT;

                if (copy_to_user((void __user*)arg, &curr, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif 
            }
            break;

        case IOCTL_CURRENT_SET:
            {
                struct jbd4040_current curr;
                uint32_t temp;
#if 0
                curr.status = 0;
                if (copy_from_user(&curr, (void __user *)arg, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.r & MASK_CURRENT;
                if (i2c_write_reg32(gClientR, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_read_reg32(gClientG, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.g & MASK_CURRENT;
                if (i2c_write_reg32(gClientG, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (i2c_read_reg32(gClientB, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.b & MASK_CURRENT;
                if (i2c_write_reg32(gClientB, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &curr, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_TEMP_READ:
            {
                struct jbd4040_temp stemp;
                uint32_t temp;
#if 0
                stemp.status = 0;
                if (i2c_read_reg32(gClientR, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                stemp.r = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (i2c_read_reg32(gClientG, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                stemp.g = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (i2c_read_reg32(gClientB, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                stemp.b = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (copy_to_user((void __user *)arg, &stemp, sizeof(stemp))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_FLIP_GET:
            {
                struct jbd4040_flip flip;
                uint32_t temp;
#if 0
                flip.status = 0;
                if (i2c_read_reg32(gClientR, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                flip.en = (temp & MASK_FLIP_SRAM) == MASK_FLIP_SRAM;

                if (copy_to_user((void __user*)arg, &flip, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_FLIP_SET:
            {
                struct jbd4040_flip flip;
                uint32_t temp;
#if 0
                if (copy_from_user(&flip, (void __user *)arg, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
                
                flip.status = 0;
                // SRAM
                if (i2c_read_reg32(gClientR, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientR, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientG, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientG, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientG, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientB, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientB, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientB, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &flip, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_MIRROR_GET:
            {
                struct jbd4040_mirror mirror;
                uint32_t temp;
#if 0
                mirror.status = 0;
                if (i2c_read_reg32(gClientR, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                mirror.en = (temp & MASK_MIRROR_SRAM) == MASK_MIRROR_SRAM;

                if (copy_to_user((void __user*)arg, &mirror, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_MIRROR_SET:
            {
                struct jbd4040_mirror mirror;
                uint32_t temp;
#if 0
                if (copy_from_user(&mirror, (void __user *)arg, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
                
                // SRAM
                if (i2c_read_reg32(gClientR, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientR, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientG, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (0 == mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientG, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (0 == mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientG, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientB, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientB, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientB, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                mirror.status = 0;

                if (copy_to_user((void __user*)arg, &mirror, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_OFFSET_GET:
            {
                struct jbd4040_offset offset;
                uint32_t temp;
#if 0
                offset.status = 0;
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                offset.en = (temp & MASK_OFFSET_VO) == MASK_OFFSET_VO;
                offset.rh = temp & MASK_OFFSET_H_VO;
                offset.rv = (temp & MASK_OFFSET_V_VO) >> 16;
                
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                offset.gh = temp & MASK_OFFSET_H_VO;
                offset.gv = (temp & MASK_OFFSET_V_VO) >> 16;
                
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                offset.bh = temp & MASK_OFFSET_H_VO;
                offset.bv = (temp & MASK_OFFSET_V_VO) >> 16;

                if (copy_to_user((void __user*)arg, &offset, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        case IOCTL_OFFSET_SET:
            {
                struct jbd4040_offset offset;
                uint32_t temp;
#if 0
                if (copy_from_user(&offset, (void __user *)arg, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }

                // Offset value is limited to 5 bits
                offset.rh &= 0x1f;
                offset.rv &= 0x1f;
                offset.gh &= 0x1f;
                offset.gv &= 0x1f;
                offset.bh &= 0x1f;
                offset.bv &= 0x1f;

                offset.status = 0;
                if (i2c_read_reg32(gClientR, REG_OFFSET_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_HV_SRAM | MASK_OFFSET_SRAM);
                    temp |= MASK_OFFSET_SRAM;
                    temp |= offset.rh << 4;
                    temp |= offset.rv << 12;
                }
                else {
                    temp &= ~MASK_OFFSET_SRAM;
                }
                if (i2c_write_reg32(gClientR, REG_OFFSET_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_VO | MASK_OFFSET_HV_VO);
                    temp |= MASK_OFFSET_VO;
                    temp |= offset.rh;
                    temp |= offset.rv << 16;
                }
                else {
                    temp &= MASK_OFFSET_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientG, REG_OFFSET_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_HV_SRAM | MASK_OFFSET_SRAM);
                    temp |= MASK_OFFSET_SRAM;
                    temp |= offset.gh << 4;
                    temp |= offset.gv << 12;
                }
                else {
                    temp &= ~MASK_OFFSET_SRAM;
                }
                if (i2c_write_reg32(gClientG, REG_OFFSET_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_VO | MASK_OFFSET_HV_VO);
                    temp |= MASK_OFFSET_VO;
                    temp |= offset.gh;
                    temp |= offset.gv << 16;
                }
                else {
                    temp &= MASK_OFFSET_VO;
                }
                if (i2c_write_reg32(gClientG, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientB, REG_OFFSET_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_HV_SRAM | MASK_OFFSET_SRAM);
                    temp |= MASK_OFFSET_SRAM;
                    temp |= offset.bh << 4;
                    temp |= offset.bv << 12;
                }
                else {
                    temp &= ~MASK_OFFSET_SRAM;
                }
                if (i2c_write_reg32(gClientB, REG_OFFSET_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }
                if (offset.en) {
                    temp &= ~(MASK_OFFSET_VO | MASK_OFFSET_HV_VO);
                    temp |= MASK_OFFSET_VO;
                    temp |= offset.bh;
                    temp |= offset.bv << 16;
                }
                else {
                    temp &= MASK_OFFSET_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4040_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &offset, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4040_ioctl_exit;
                }
#endif
            }
            break;

        default:
            ret = -EINVAL;
            goto jbd4040_ioctl_exit;
    }
jbd4040_ioctl_exit:
    mutex_unlock(&jbd4040_mutex);
    return ret;
}

static int jbd4040_open(struct inode *inode, struct file *file) {
    return 0;
}

static int jbd4040_release(struct inode *inode, struct file *file) {
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = jbd4040_ioctl,
    .open = jbd4040_open,
    .release = jbd4040_release,
};

// --------- Sysfs Attributes ---------
static uint32_t sysfs_reg = 0;
static uint32_t sysfs_val = 0;

static ssize_t reg_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", sysfs_reg);
}

static ssize_t reg_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    kstrtou32(buf, 0, &sysfs_reg);
    return count;
}

static ssize_t val_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    uint32_t val;
    if (i2c_read_reg32(gClientR, sysfs_reg, &val) == 0)
        sysfs_val = val;
    return scnprintf(buf, PAGE_SIZE, "0x%08x\n", sysfs_val);
}

static ssize_t val_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    kstrtou32(buf, 0, &sysfs_val);
    i2c_write_reg32(gClientR, sysfs_reg, sysfs_val);
    return count;
}

// Define sysfs attributes (read/write)
static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(val);

// Dynamic
struct my_dev_attr {
    struct device_attribute dev_attr;
    u32 addr;
    u32 bitmask;
    char label[32];
    struct device *dev;
};


#define DEF_I2C_JBD4040_MAX_ARGS        8

// Functions for handling "cat"
static ssize_t show_luminance(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t valR, valG, valB;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &valR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &valG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &valB)) {
        count = scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
                            valR & itemattr->bitmask, valG & itemattr->bitmask, valB & itemattr->bitmask);
    }
    
    return count;
}

static ssize_t show_current(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t valR, valG, valB;
    
    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &valR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &valG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &valB)) {
        count = scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n",
                            valR & itemattr->bitmask, valG & itemattr->bitmask, valB & itemattr->bitmask);
    }

    return count;
}

static ssize_t show_temperature(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{

    int tempR = 0, tempG = 0, tempB = 0;
    int retR = -1, retG = -1, retB = -1;
    ssize_t count = 0;
    int int_part, frac_part;

    /* 1. 平行下達量測指令：同時向三個面板寫入 0x0003 */
    if (gClientR) i2c_write_reg16(gClientR, 0x200402, 0x0003);
    if (gClientG) i2c_write_reg16(gClientG, 0x200402, 0x0003);
    if (gClientB) i2c_write_reg16(gClientB, 0x200402, 0x0003);

    /* 2. 只需要睡 1 次 1 秒，等待所有面板 ADC 轉換完成 */
    msleep(1000);

    /* 3. 讀取並計算各面板的溫度 */
    if (gClientR) retR = read_panel_temp(gClientR, &tempR);
    if (gClientG) retG = read_panel_temp(gClientG, &tempG);
    if (gClientB) retB = read_panel_temp(gClientB, &tempB);

    /* 4. 關閉溫度量測功能以節省功耗 (寫入 0x0000) */
    if (gClientR) i2c_write_reg16(gClientR, 0x200402, 0x0000);
    if (gClientG) i2c_write_reg16(gClientG, 0x200402, 0x0000);
    if (gClientB) i2c_write_reg16(gClientB, 0x200402, 0x0000);

    /* 5. 格式化輸出，完美模擬 Python 的 %.2f，同時處理負數的小數點顯示 */

    /* 處理 Red */
    count += scnprintf(buf + count, PAGE_SIZE - count, "R: ");
    if (retR == 0) {
        int_part = tempR / 100;
        frac_part = abs(tempR % 100);
        if (tempR < 0 && int_part == 0) count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
        else count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
    } else count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");

    /* 處理 Green */
    count += scnprintf(buf + count, PAGE_SIZE - count, "G: ");
    if (retG == 0) {
        int_part = tempG / 100;
        frac_part = abs(tempG % 100);
        if (tempG < 0 && int_part == 0) count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
        else count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
    } else count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");

    /* 處理 Blue */
    count += scnprintf(buf + count, PAGE_SIZE - count, "B: ");
    if (retB == 0) {
        int_part = tempB / 100;
        frac_part = abs(tempB % 100);
        if (tempB < 0 && int_part == 0) count += scnprintf(buf + count, PAGE_SIZE - count, "-0.%02d\n", frac_part);
        else count += scnprintf(buf + count, PAGE_SIZE - count, "%d.%02d\n", int_part, frac_part);
    } else count += scnprintf(buf + count, PAGE_SIZE - count, "Error\n");

    return count;

#if 0 //old jbd4020    
    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &tempR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &tempG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &tempB)) {
        tempR &= itemattr->bitmask;
        tempG &= itemattr->bitmask;
        tempB &= itemattr->bitmask;
        tempR = ((tempR - 118) * 165) / 815 - 40;
        tempG = ((tempG - 118) * 165) / 815 - 40;
        tempB = ((tempB - 118) * 165) / 815 - 40;
        count = scnprintf(buf, PAGE_SIZE, "R: %u\nG: %u\nB: %u\n", tempR, tempG, tempB);
    }

    return count;
#endif
}



static ssize_t show_flip(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t res_flip = 0, valR = 0, valG = 0, valB = 0;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &valR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &valG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &valB)) {
	dev_info(dev, "%s valR:0x%08x, valG:0x%08x, valB:0x%08x\n", __func__, valR, valG, valB);
        if ( (valR&0x02) == 1 && (valG&0x02) == 1 && (valB&0x02) == 1){ 
		res_flip = 1;
	}else{
		res_flip = 0;
	}
	count = scnprintf(buf, PAGE_SIZE, "Flip is %s\n", res_flip ? "Enabled" : "Disabled");

    }


    return count;
}

static ssize_t show_mirror(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t res_mirror = 0, valR = 0, valG = 0, valB = 0;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &valR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &valG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &valB)) {
	dev_info(dev, "%s valR:0x%08x, valG:0x%08x, valB:0x%08x\n", __func__, valR, valG, valB);
        if ( (valR&0x01) == 1 && (valG&0x01) == 0 && (valB&0x01) == 1){ 
		res_mirror = 1;
	}else{
		res_mirror = 0;
	}
	count = scnprintf(buf, PAGE_SIZE, "Mirror is %s\n", res_mirror ? "Enabled" : "Disabled");

    }

    return count;
}

static ssize_t show_offset(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t offsetR = 0, offsetG = 0, offsetB = 0;
    //uint32_t rh = 0, rv = 0, gh = 0, gv = 0, bh = 0, bv = 0;

    dev_info(dev, "itemattr->addr:  0x%02x\n", itemattr->addr);
    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &offsetR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &offsetG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &offsetB)) {
        count = scnprintf(buf, PAGE_SIZE, "R(%s) H:%u, V:%u\nG(%s) H:%u, V:%u\nB(%s) H:%u, V:%u\n",
                offsetR & 0x1F1F ? "enabled" : "disabled", (offsetR >> 8) & 0x1F, (offsetR) & 0x1F,
                offsetG & 0x1F1F ? "enabled" : "disabled", (offsetG >> 8) & 0x1F, (offsetG) & 0x1F,
                offsetB & 0x1F1F ? "enabled" : "disabled", (offsetB >> 8) & 0x1F, (offsetB) & 0x1F);
    }

    dev_info(dev, "offsetR:  0x%08x\n", offsetR);
    dev_info(dev, "offsetG:  0x%08x\n", offsetG);
    dev_info(dev, "offsetB:  0x%08x\n", offsetB);
    return count;
}

// Functions for handling "echo"
static ssize_t store_luminance(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *input_free, *token;
    char *argv[DEF_I2C_JBD4040_MAX_ARGS];
    int argc = 0;
    int ret = 0;
    struct i2c_client *client;
    uint32_t val;
    uint32_t temp;
    uint32_t i;

    /* 複製字串並保留原始指標 (為了最後能正確 kfree) */
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;
    input_free = input; 

    /* 將冒號 ':' 加入分隔符號中 */
    while ((token = strsep(&input, " \t\n:")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4040_MAX_ARGS) {
            dev_warn(dev, "Too many arguments\n");
            break;
        }
        argv[argc++] = token;
    }

    /* 檢查參數是否成對 */
    if (argc == 0 || argc % 2 != 0) {
        dev_warn(dev, "Invalid arguments format. Expected pairs like 'R 4000'. Got %d tokens.\n", argc);
        goto out;
    }

    /* 一次處理所有傳進來的 RGB 設定 */
    for (i = 0; i < argc; i += 2) {
        char *color_str = argv[i];
        char *val_str = argv[i+1];

        /* 將字串轉為數字 */
        if (kstrtou32(val_str, 10, &val)) {
            dev_warn(dev, "Invalid number for %s: %s\n", color_str, val_str);
            continue;
        }

        if (val > 8191) {
            dev_info(dev, "Value %u greater than limitation (8191), will be truncated\n", val);
        }

        /* 判斷面板 (支援大小寫) */
        if (!strcasecmp(color_str, "r")) {
            client = gClientR;
        } else if (!strcasecmp(color_str, "g")) {
            client = gClientG;
        } else if (!strcasecmp(color_str, "b")) {
            client = gClientB;
        } else {
            dev_warn(dev, "Unknown panel tag: %s\n", color_str);
            continue;
        }

        if (!client) {
            dev_err(dev, "Client %s is not initialized\n", color_str);
            continue;
        }

        /* 執行 I2C 讀取 -> 修改 -> 寫入 (Read-Modify-Write, 32-bit) */
        ret = i2c_read_reg32(client, itemattr->addr, &temp);
        if (ret < 0) {
            dev_err(dev, "Failed to read %s panel (addr: 0x%02x)\n", color_str, itemattr->addr);
            continue;
        }

        /* 套用遮罩，保留其他位元，只修改亮度位元 */
        temp &= ~itemattr->bitmask;
        temp |= (val & itemattr->bitmask);
        
        ret = i2c_write_reg32(client, itemattr->addr, temp);
        if (ret < 0) {
            dev_err(dev, "Failed to write %u to %s panel (addr: 0x%02x)\n", val, color_str, itemattr->addr);
        } else {
            dev_info(dev, "Successfully set %s panel luminance to %u\n", color_str, val);
        }
    }

out:
    /* 必須釋放剛開始分配的原始記憶體位址 */
    kfree(input_free);
    return count;
}


static ssize_t store_current(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *input_free, *token;
    char *argv[DEF_I2C_JBD4040_MAX_ARGS];
    int argc = 0;
    int ret = 0;
    struct i2c_client *client;
    uint32_t val;
    uint32_t i;

    /* 複製字串並保留原始指標 (為了最後能正確 kfree) */
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;
    input_free = input; 

    /* 💡 關鍵修改 1：把冒號 ':' 也加入分隔符號中 */
    while ((token = strsep(&input, " \t\n:")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4040_MAX_ARGS) {
            dev_warn(dev, "Too many arguments\n");
            break;
        }
        argv[argc++] = token;
    }

    /* 💡 關鍵修改 2：檢查參數是否成對 (例如 2個、4個或6個) */
    if (argc == 0 || argc % 2 != 0) {
        dev_warn(dev, "Invalid arguments format. Expected pairs like 'R 39'. Got %d tokens.\n", argc);
        goto out;
    }

    /* 💡 關鍵修改 3：一次處理所有傳進來的 RGB 設定 */
    for (i = 0; i < argc; i += 2) {
        char *color_str = argv[i];
        char *val_str = argv[i+1];

        /* 將字串轉為數字 */
        if (kstrtou32(val_str, 10, &val)) {
            dev_warn(dev, "Invalid number for %s: %s\n", color_str, val_str);
            continue;
        }

        /* 判斷面板 (使用 strcasecmp 可以同時支援大小寫，如 "r" 或 "R") */
        if (!strcasecmp(color_str, "r")) {
            client = gClientR;
        } else if (!strcasecmp(color_str, "g")) {
            client = gClientG;
        } else if (!strcasecmp(color_str, "b")) {
            client = gClientB;
        } else if (!strcasecmp(color_str, "t")) {
            client = to_i2c_client(dev);
	    pr_info("jbd4040 tx timing : %x\n", val);
	    ret = i2c_write_reg32(client, 0x201038, val);
	    if (ret < 0) {
	    	dev_err(dev, "jbd4040 tx timing write error!\n");
	    } else {
	    	dev_err(dev, "jbd4040 tx timing write ok!\n");
	    }
	    goto out;
        } else {
            dev_warn(dev, "Unknown panel tag: %s\n", color_str);
            continue;
        }

        if (!client) {
            dev_err(dev, "Client %s is not initialized\n", color_str);
            continue;
        }

        /* 執行 I2C 寫入 (請替換為你實際使用的暫存器寫入函式) */
        ret = i2c_write_reg16(client, itemattr->addr, val);
        if (ret < 0) {
            dev_err(dev, "Failed to write %u to %s panel (addr: 0x%02x)\n", val, color_str, itemattr->addr);
        } else {
            dev_info(dev, "Successfully set %s panel to %u\n", color_str, val);
        }
    }

out:
    /* 必須釋放剛開始分配的原始記憶體位址 */
    kfree(input_free);
    return count;
}

static ssize_t store_flip(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *input_free, *token;
    uint32_t cmd_val;
    uint16_t valR = 0, valG = 0, valB = 0;
    int ret;

    /* 1. 複製字串並保留原始指標以防 Memory Leak */
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;
    input_free = input; 

    /* 2. 解析傳入的數值 */
    token = strsep(&input, " \t\n");
    if (!token || kstrtou32(token, 10, &cmd_val)) {
        dev_err(dev, "Invalid flip value received\n");
        kfree(input_free);
        return -EINVAL;
    }
    kfree(input_free);

    dev_info(dev, "Received store_flip command value: %u\n", cmd_val);

    if (cmd_val == 1) {
        /* ========================================================
         * 狀況 A：收到 1 -> 開啟 Flip (R/G/B 皆設為 Bit 1)
         * ======================================================== */
        if (gClientR && i2c_read_reg16(gClientR, 0x20020e, &valR) == 0) {
            valR |= 0x0002; /* Bit 1 置 1 */
            i2c_write_reg16(gClientR, 0x20020e, valR);
        }
        if (gClientG && i2c_read_reg16(gClientG, 0x20020e, &valG) == 0) {
            valG |= 0x0002;
            i2c_write_reg16(gClientG, 0x20020e, valG);
        }
        if (gClientB && i2c_read_reg16(gClientB, 0x20020e, &valB) == 0) {
            valB |= 0x0002;
            i2c_write_reg16(gClientB, 0x20020e, valB);
        }
        dev_info(dev, "Flip Mode Enabled: R/G/B set bit 1\n");

    } else if (cmd_val == 0) {
        /* ========================================================
         * 狀況 B：收到 0 -> 關閉 Flip (R/G/B 皆清零 Bit 1)
         * ======================================================== */
        if (gClientR && i2c_read_reg16(gClientR, 0x20020e, &valR) == 0) {
            valR &= ~0x0002; /* Bit 1 清零 */
            i2c_write_reg16(gClientR, 0x20020e, valR);
        }
        if (gClientG && i2c_read_reg16(gClientG, 0x20020e, &valG) == 0) {
            valG &= ~0x0002;
            i2c_write_reg16(gClientG, 0x20020e, valG);
        }
        if (gClientB && i2c_read_reg16(gClientB, 0x20020e, &valB) == 0) {
            valB &= ~0x0002;
            i2c_write_reg16(gClientB, 0x20020e, valB);
        }
        dev_info(dev, "Flip Mode Disabled: R/G/B cleared bit 1\n");

    } else {
        dev_warn(dev, "Unsupported flip value: %u. Use 0 or 1.\n", cmd_val);
        return -EINVAL;
    }

    return count;
}


static ssize_t store_flip_dep(struct device *dev, struct my_dev_attr *itemattr,
                            const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4040_MAX_ARGS];
    int argc = 0;
    int ret;
    uint32_t temp;


#if 1
    return count;
#else
    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4040_MAX_ARGS) {
            dev_warn(dev, "Too many arguments\n");
            goto out;
        }
        argv[argc++] = token;
    }

    // 
    if (argc != 2) {
        dev_warn(dev, "Expected 2 arguments, got %d\n", argc);
        goto out;
    }

    // 
    if (!strcmp(argv[0], "r") || !strcmp(argv[0], "R")) {
    }
    else if (!strcmp(argv[0], "g") || !strcmp(argv[0], "G")) {
    }
    else if (!strcmp(argv[0], "b") || !strcmp(argv[0], "B")) {
    }
    else {
        dev_warn(dev, "Panel incorrect: %s\n", argv[0]);
        goto out;
    }

    // 
    unsigned int en;
    ret = kstrtouint(argv[1], 0, &en);
    if (ret) {
        dev_warn(dev, "Invalid value: %s\n", argv[1]);
        goto out;
    }

    // 
    en &= 0x01;
    dev_info(dev, "Parsed: panel=%s, en=%d", argv[0], en);

    // TODO: I2C
    u32 value = en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientR, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientR, itemattr->addr, temp);

        // VO path setting
        value = en << 31;
        if (!i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
            temp &= ~MASK_FLIP_VO;
            temp |= (value & MASK_FLIP_VO);
            i2c_write_reg32(gClientR, REG_FMO_VO, temp);
        }
    }
    value = en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientG, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientG, itemattr->addr, temp);

        // VO path setting
        value = en << 31;
        if (!i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
            temp &= ~MASK_FLIP_VO;
            temp |= (value & MASK_FLIP_VO);
            i2c_write_reg32(gClientG, REG_FMO_VO, temp);
        }
    }
    value = en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientB, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientB, itemattr->addr, temp);

        // VO path setting
        value = en << 31;
        if (!i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
            temp &= ~MASK_FLIP_VO;
            temp |= (value & MASK_FLIP_VO);
            i2c_write_reg32(gClientB, REG_FMO_VO, temp);
        }
    }

out:
    kfree(input);
    return count;
#endif
}

static ssize_t store_mirror(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *input_free, *token;
    uint32_t cmd_val;
    uint16_t valR = 0, valG = 0, valB = 0;
    int ret;

    /* 1. 複製字串並保留原始指標以防 Memory Leak */
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;
    input_free = input; 

    /* 2. 解析傳入的數值 (支援 echo 1 > mirror 或帶換行字元) */
    token = strsep(&input, " \t\n");
    if (!token || kstrtou32(token, 10, &cmd_val)) {
        dev_err(dev, "Invalid mirror value received\n");
        kfree(input_free);
        return -EINVAL;
    }
    kfree(input_free);

    dev_info(dev, "Received store_mirror command value: %u\n", cmd_val);

    /* 💡 註：此處暫存器位址 0x20020E 預設採用 16-bit 讀寫。
     * 若手冊規範該區塊必須以 32-bit 進行操作，請將底下的 uint16_t 改為 uint32_t，
     * 並將 i2c_read_reg16/i2c_write_reg16 替換為 i2c_read_reg32/i2c_write_reg32。
     */

    if (cmd_val == 1) {
        /* ========================================================
         * 狀況 A：收到 1 -> 開啟鏡像 (R/B 設為 1, G 清零)
         * ======================================================== */
        
        // 處理 Red Panel
        if (gClientR) {
            ret = i2c_read_reg16(gClientR, 0x20020e, &valR);
            if (ret == 0) {
                valR |= 0x0001; /* Bit 0 置 1 */
                i2c_write_reg16(gClientR, 0x20020e, valR);
            }
        }

        // 處理 Green Panel
        if (gClientG) {
            ret = i2c_read_reg16(gClientG, 0x20020e, &valG);
            if (ret == 0) {
                valG &= ~0x0001; /* Bit 0 清零 */
                i2c_write_reg16(gClientG, 0x20020e, valG);
            }
        }

        // 處理 Blue Panel
        if (gClientB) {
            ret = i2c_read_reg16(gClientB, 0x20020e, &valB);
            if (ret == 0) {
                valB |= 0x0001; /* Bit 0 置 1 */
                i2c_write_reg16(gClientB, 0x20020e, valB);
            }
        }
        dev_info(dev, "Mirror Mode Enabled: R/B set bit0, G cleared bit0\n");

    } else if (cmd_val == 0) {
        /* ========================================================
         * 狀況 B：收到 0 -> 關閉鏡像，恢復正常視角 (R/B 清零, G 設為 1)
         * ======================================================== */
        
        // 處理 Red Panel
        if (gClientR) {
            ret = i2c_read_reg16(gClientR, 0x20020e, &valR);
            if (ret == 0) {
                valR &= ~0x0001; /* Bit 0 清零 */
                i2c_write_reg16(gClientR, 0x20020e, valR);
            }
        }

        // 處理 Green Panel
        if (gClientG) {
            ret = i2c_read_reg16(gClientG, 0x20020e, &valG);
            if (ret == 0) {
                valG |= 0x0001; /* Bit 0 置 1 */
                i2c_write_reg16(gClientG, 0x20020e, valG);
            }
        }

        // 處理 Blue Panel
        if (gClientB) {
            ret = i2c_read_reg16(gClientB, 0x20020e, &valB);
            if (ret == 0) {
                valB &= ~0x0001; /* Bit 0 清零 */
                i2c_write_reg16(gClientB, 0x20020e, valB);
            }
        }
        dev_info(dev, "Mirror Mode Disabled: R/B cleared bit0, G set bit0\n");
    } else {
        dev_warn(dev, "Unsupported mirror value: %u. Use 0 or 1.\n", cmd_val);
        return -EINVAL;
    }

    return count;
}


static ssize_t store_mirror_dep(struct device *dev, struct my_dev_attr *itemattr,
                            const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4040_MAX_ARGS];
    int argc = 0;
    int ret;
    uint32_t temp;
#if 1
    return count;
#else
    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4040_MAX_ARGS) {
            dev_warn(dev, "Too many arguments\n");
            goto out;
        }
        argv[argc++] = token;
    }

    // 
    if (argc != 2) {
        dev_warn(dev, "Expected 2 arguments, got %d\n", argc);
        goto out;
    }

    // 
    if (!strcmp(argv[0], "r") || !strcmp(argv[0], "R")) {
    }
    else if (!strcmp(argv[0], "g") || !strcmp(argv[0], "G")) {
    }
    else if (!strcmp(argv[0], "b") || !strcmp(argv[0], "B")) {
    }
    else {
        dev_warn(dev, "Panel incorrect: %s\n", argv[0]);
        goto out;
    }

    // 
    unsigned int en;
    ret = kstrtouint(argv[1], 0, &en);
    if (ret) {
        dev_warn(dev, "Invalid value: %s\n", argv[1]);
        goto out;
    }

    // 
    en &= 0x01;
    dev_info(dev, "Parsed: panel=%s, en=%d", argv[0], en);

    // TODO: I2C
    u32 value = en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientR, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientR, itemattr->addr, temp);

        // VO path setting
        value = en << 30;
        if (!i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
            temp &= ~MASK_MIRROR_VO;
            temp |= (value & MASK_MIRROR_VO);
            i2c_write_reg32(gClientR, REG_FMO_VO, temp);
        }
    }
    value = !en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientG, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientG, itemattr->addr, temp);

        // VO path setting
        value = !en << 30;
        if (!i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
            temp &= ~MASK_MIRROR_VO;
            temp |= (value & MASK_MIRROR_VO);
            i2c_write_reg32(gClientG, REG_FMO_VO, temp);
        }
    }
    value = en << __builtin_ctz(itemattr->bitmask);
    if (!i2c_read_reg32(gClientB, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(gClientB, itemattr->addr, temp);

        // VO path setting
        value = en << 30;
        if (!i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
            temp &= ~MASK_MIRROR_VO;
            temp |= (value & MASK_MIRROR_VO);
            i2c_write_reg32(gClientB, REG_FMO_VO, temp);
        }
    }

out:
    kfree(input);
    return count;
#endif
}

static ssize_t store_offset(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *input_free, *token;
    
    /* 💡 注意：因為 RGB 三組如果都帶上 H 跟 V (例如 R 1 10 20)，
     * 最多會有 12 個 token，原先的 8 個不夠用，這裡局部放大到 16 個 */
    #define OFFSET_MAX_ARGS 16
    char *argv[OFFSET_MAX_ARGS];
    
    int argc = 0;
    int ret = 0;
    struct i2c_client *client_temp;
    uint32_t i = 0;

    /* 複製字串並保留原始指標 (為了最後能正確 kfree) */
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;
    input_free = input; 

    /* 將冒號 ':' 與空白、換行加入分隔符號中 */
    while ((token = strsep(&input, " \t\n:")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= OFFSET_MAX_ARGS) {
            dev_warn(dev, "Too many arguments for offset (max %d)\n", OFFSET_MAX_ARGS);
            break;
        }
        argv[argc++] = token;
    }

    if (argc == 0) {
        dev_warn(dev, "Invalid arguments format for offset.\n");
        goto out;
    }

    /* 💡 智慧解析迴圈：支援 'R 1' (2參數) 或是 'R 1 10 20' (4參數) 的混合批次寫入 */
    while (i < argc) {
        char *color_str = argv[i++];
        char *en_str;
        u32 en = 0, val_h = 0, val_v = 0;
        int has_hv = 0;
	uint32_t offset_val = 0;

        if (i >= argc) {
            dev_warn(dev, "Missing enable value for panel %s\n", color_str);
            break;
        }
        en_str = argv[i++];

        if (kstrtou32(en_str, 10, &en)) {
            dev_warn(dev, "Invalid enable value: %s\n", en_str);
            continue; /* 跳過這組，繼續解析下一個 */
        }

        /* 判斷目標面板 */
        if (!strcasecmp(color_str, "r")) {
            client_temp = gClientR;
        } else if (!strcasecmp(color_str, "g")) {
            client_temp = gClientG;
        } else if (!strcasecmp(color_str, "b")) {
            client_temp = gClientB;
        } else {
            dev_warn(dev, "Unknown panel tag: %s\n", color_str);
            continue;
        }

        if (!client_temp) {
            dev_err(dev, "Client %s is not initialized\n", color_str);
            continue;
        }

        /* 💡 向前探測 (Look-ahead)：
         * 檢查下一個 token 是不是數字 (還是下一個面板的 R/G/B 標籤) */
        if (i < argc && strcasecmp(argv[i], "r") && strcasecmp(argv[i], "g") && strcasecmp(argv[i], "b")) {
            /* 如果不是 R/G/B，代表使用者有給 H 的值 */
            char *h_str = argv[i++];
            
            /* 再檢查下一個是不是 V 的值 */
            if (i < argc && strcasecmp(argv[i], "r") && strcasecmp(argv[i], "g") && strcasecmp(argv[i], "b")) {
                char *v_str = argv[i++];
                
                if (kstrtou32(h_str, 10, &val_h) || kstrtou32(v_str, 10, &val_v)) {
                    dev_warn(dev, "Invalid H or V value for %s panel\n", color_str);
                    continue;
                }
                has_hv = 1; /* 標記這組資料包含完整的 H 與 V */
            } else {
                dev_warn(dev, "Missing V value for %s panel (H was given). Skipping...\n", color_str);
                continue;
            }
        }

        /* 過濾有效位元數 */
        en &= 0x01;
        val_h &= 0x1F;
        val_v &= 0x1F;

        dev_info(dev, "Parsed Offset: panel=%s, en=%u, h=%u, v=%u (has_hv=%d)\n", 
                 color_str, en, val_h, val_v, has_hv);

	if(en == 1){
		if (has_hv == 1){
			offset_val = (val_h & 0x1F) << 8 | (val_v & 0x1F);
			dev_info(dev, "panel: %s, offset_val:0x%08x\n", color_str, offset_val);
			i2c_write_reg32(client_temp, itemattr->addr , offset_val);
		}
	}
    }

out:
    /* 釋放記憶體，避免 Memory Leak */
    kfree(input_free);
    return count;
}


static ssize_t my_attr_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    int count;
    struct my_dev_attr *myattr = container_of(attr, struct my_dev_attr, dev_attr);

    if (!strcmp("luminance", myattr->label)) {
        count = show_luminance(dev, myattr, buf);
    }
    else if (!strcmp("current", myattr->label)) {
        count = show_current(dev, myattr, buf);
    }
    else if (!strcmp("temperature", myattr->label)) {
        count = show_temperature(dev, myattr, buf);
    }
    else if (!strcmp("flip", myattr->label)) {
        count = show_flip(dev, myattr, buf);
    }
    else if (!strcmp("mirror", myattr->label)) {
        count = show_mirror(dev, myattr, buf);
    }
    else if (!strcmp("offset", myattr->label)) {
        count = show_offset(dev, myattr, buf);
    }
    else {
        count = scnprintf(buf, PAGE_SIZE, "Label: %s, Addr: 0x%02x\n",
                            myattr->label, myattr->addr);
    }
    
    return count;
}

static ssize_t my_attr_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{
    struct my_dev_attr *myattr = container_of(attr, struct my_dev_attr, dev_attr);
    dev_info(dev, "Received write to %s (0x%02x): %.*s\n",
             myattr->label, myattr->addr, (int)count, buf);
    
    if (!strcmp("luminance", myattr->label)) {
        store_luminance(dev, myattr, buf, count);
    }
    if (!strcmp("current", myattr->label)) {
        store_current(dev, myattr, buf, count);
    }
    if (!strcmp("flip", myattr->label)) {
        store_flip(dev, myattr, buf, count);
    }
    if (!strcmp("mirror", myattr->label)) {
        store_mirror(dev, myattr, buf, count);
    }
    if (!strcmp("offset", myattr->label)) {
        store_offset(dev, myattr, buf, count);
    }
    
    return count;
}


// --------- 【新增】硬體電源與訊號控制函式 ---------

static void jbd4040_avee_on(struct jbd4040_priv *priv)
{
    if (!priv || !priv->gpiod_avee) return;
    msleep(1000); // 對應 Python time.sleep(1)
    gpiod_set_value_cansleep(priv->gpiod_avee, 1);
    pr_info("jbd4040: AVEE set HIGH (Panel Turned ON)\n");
}

static void jbd4040_avee_off(struct jbd4040_priv *priv)
{
    if (!priv || !priv->gpiod_avee) return;
    msleep(1000); // 對應 Python time.sleep(1)
    gpiod_set_value_cansleep(priv->gpiod_avee, 0);
    pr_info("jbd4040: AVEE set LOW (Panel Turned OFF)\n");
}

static void jbd4040_power_on(struct jbd4040_priv *priv)
{
    if (!priv) return;
    pr_info("jbd4040: Starting power on sequence...\n");

    // 全初始化為 LOW
    gpiod_set_value_cansleep(priv->gpiod_dvdd, 0);
    gpiod_set_value_cansleep(priv->gpiod_vddi, 0);
    gpiod_set_value_cansleep(priv->gpiod_avdd, 0);
    gpiod_set_value_cansleep(priv->gpiod_avee, 0);
    gpiod_set_value_cansleep(priv->gpiod_reset, 1);
    msleep(200);

    // DVDD ON
    gpiod_set_value_cansleep(priv->gpiod_dvdd, 1);
    msleep(200);

    // VDDI ON
    gpiod_set_value_cansleep(priv->gpiod_vddi, 1);

    // AVDD ON
    gpiod_set_value_cansleep(priv->gpiod_avdd, 1);
    msleep(100);

    // RESET Pulse (High -> Low -> High)
    gpiod_set_value_cansleep(priv->gpiod_reset, 0);
    msleep(1000);
    gpiod_set_value_cansleep(priv->gpiod_reset, 1);
    msleep(50);
    gpiod_set_value_cansleep(priv->gpiod_reset, 0);

    pr_info("jbd4040: Power on sequence completed.\n");
}

static void jbd4040_power_off(struct jbd4040_priv *priv)
{
    if (!priv) return;
    pr_info("jbd4040: Starting power off sequence...\n");

    msleep(10);
    gpiod_set_value_cansleep(priv->gpiod_vddi, 0);
    gpiod_set_value_cansleep(priv->gpiod_avdd, 0);

    msleep(10);
    gpiod_set_value_cansleep(priv->gpiod_dvdd, 0);

    pr_info("jbd4040: Power off sequence completed.\n");
}

/*
 * 將指定的 Gamma Table 寫入面板並啟用
 */
static int jbd4040_update_gamma(struct i2c_client *client, const uint16_t *gamma_data)
{
    int i, ret;
    uint32_t current_addr;

    pr_info("jbd4040: Updating Gamma Table (256 points)...\n");

    /* 1. 把控制權切換到廣播位址 (或者指定的色彩面板位址) */
    client->addr = JBD4040_I2C_ADDR_ALL; // 若要分開寫入 RGB，可在此切換 0x59, 0x5A, 0x5B

    /* 2. 寫入 256 階 Gamma 數據 */
    for (i = 0; i < 256; i++) {
        /* 💡 假設暫存器是 16-bit (2 bytes) 遞增的。如果你的手冊是 32-bit (4 bytes) 遞增，請改成 i * 4 */
        current_addr = JBD4040_GAMMA_TABLE_0_ADDR + (i * 2);

        /* 呼叫我們之前實作好的 16-bit 寫入函式 */
        ret = i2c_write_reg16(client, current_addr, gamma_data[i]);
        if (ret < 0) {
            dev_err(&client->dev, "Failed to write gamma point %d at 0x%06X\n", i, current_addr);
            return ret;
        }
    }

    /* 3. 寫入 Gamma Enable 暫存器 */
    /* 假設啟用是寫入 1 (或者根據手冊定義的值) */
    ret = i2c_write_reg16(client, JBD4040_GAMMA_EN_REG, 0x0001); 
    if (ret < 0) {
        dev_err(&client->dev, "Failed to enable Gamma!\n");
        return ret;
    }

    pr_info("jbd4040: Gamma updated successfully.\n");
    return 0;
}


static int jbd4040_init_registers(struct i2c_client *client)
{
    uint8_t orig_addr = client->addr; /* 備份原本 DTS 的 I2C 位址 (0x59) */
    uint16_t chip_id0 = 0;
    uint16_t chip_id1 = 0;
    int ret;

    pr_info("Venom jbd4040: Executing real initialization sequence from Python...\n");

    /* ===========================================================
     * 1. 廣播位址 (0x58) 初始化序列 (self.all_i2c_device)
     * =========================================================== */
    client->addr = JBD4040_I2C_ADDR_ALL;

    pr_info("client->addr : 0x%x\n", client->addr);
    /* --- Interrupt Mask Registers --- */
    i2c_write_reg32(client, 0x201044, 0x0000ffff);
    i2c_write_reg32(client, 0x20104c, 0x0000ffff);
    i2c_write_reg32(client, 0x201054, 0x0000ffff);
    i2c_write_reg32(client, 0x20105c, 0x0000ffff);
    i2c_write_reg32(client, 0x201064, 0x0000ffff);
    i2c_write_reg32(client, 0x20106c, 0x0000ffff);
    i2c_write_reg16(client, 0x200b0a, 0xffff);

    /* --- MIPI Initialization --- */
    i2c_write_reg32(client, 0x201004, 0x00000001);
    i2c_write_reg32(client, 0x20100c, 0x00000000);
    i2c_write_reg32(client, 0x201028, 0x0000000f);
    i2c_write_reg32(client, 0x20102c, 0x00000004);
    i2c_write_reg32(client, 0x202000, 0x0000007d);
    i2c_write_reg32(client, 0x2021e0, 0x00000008);

    pr_info("Venom jdb4040: tx delay timing to 0x4a2"); //tx delay timing 680 for old type
    //i2c_write_reg32(client, 0x201038, 0x000004a0); //tx delay time
    //i2c_write_reg32(client, 0x201038, 0x000008a0);//only green
    //i2c_write_reg32(client, 0x201038, 0x000006a0);//no red
    //i2c_write_reg32(client, 0x201038, 0x000005a0);//got red, but offset
    //i2c_write_reg32(client, 0x201038, 0x00000620);//got red, but offset
    //i2c_write_reg32(client, 0x201038, 0x00000080);//got red, but offset
    i2c_write_reg32(client, 0x201038, 0x000004a2);//got red, but offset
    i2c_write_reg32(client, 0x202128, 0x0000000f);
    i2c_write_reg32(client, 0x2021f4, 0x00000027);

    /* --- Panel Initialization --- */
    i2c_write_reg16(client, 0x200100, 0x0022);
    //i2c_write_reg16(client, 0x200a00, 0x000C);
    i2c_write_reg16(client, 0x200a00, 0x0008); //60HZ, 10bit scan 
    i2c_write_reg16(client, 0x200a02, 0x0001);
    i2c_write_reg16(client, 0x200a04, 0x0002);
    i2c_write_reg16(client, 0x200a14, 0x1388);
    i2c_write_reg16(client, 0x200a1c, 0x0000);
    i2c_write_reg16(client, 0x200a1e, 0x017b);
    i2c_write_reg16(client, 0x200a20, 0x0000);
    i2c_write_reg16(client, 0x200a22, 0x01f3);
    i2c_write_reg16(client, 0x200a24, 0x0a06);
    i2c_write_reg16(client, 0x200b06, 0x0000);

    /* --- Algorithm Initialization --- */
    i2c_write_reg16(client, 0x200d30, 0x0002);
    i2c_write_reg16(client, 0x200d34, 0x80a1);
    i2c_write_reg16(client, 0x200204, 0x03ff);

    /* --- Video mode --- */
    i2c_write_reg16(client, 0x200b06, 0x0001);

    /* --- Frame Sync --- */
    i2c_write_reg16(client, 0x200a04, 0x000f);

    msleep(20); /* 確保廣播設定完成再進入通道各別處理 */


    jbd4040_update_gamma(client, gamma_2_2_table);

    /* ===========================================================
     * 2. 針對個別面板 (紅 0x59 / 綠 0x5A / 藍 0x5B) 的獨立暫存器設定
     * =========================================================== */
    
    /* --- 紅光面板 (0x59) --- */
    client->addr = JBD4040_I2C_ADDR_RED;
    i2c_write_reg16(client, 0x20020e, 0x0000);

    /* --- 綠光面板 (0x5A) --- */
    client->addr = JBD4040_I2C_ADDR_GREEN;
    i2c_write_reg16(client, 0x20020e, 0x0001);

    /* --- 藍光面板 (0x5B) --- */
    client->addr = JBD4040_I2C_ADDR_BLUE;
    i2c_write_reg16(client, 0x20020e, 0x0000);

    msleep(10);

    /* ===========================================================
     * 3. 切回原本主面板位址 (0x58)，讀取驗證 CHIP_ID
     * =========================================================== */
    client->addr = orig_addr;

    printk("Venom Test\n");

    /* 讀取 CHIP_ID0 與 CHIP_ID1 */
    ret = i2c_read_reg16(client, REG_CHIP_ID0, &chip_id0);
    if (ret < 0) {
        pr_err("jbd4040: Failed to read CHIP_ID0\n");
        return ret;
    }

    printk("Venom CHIP_ID0: 0x%08X\n", chip_id0);
    ret = i2c_read_reg16(client, REG_CHIP_ID1, &chip_id1);
    if (ret < 0) {
        pr_err("jbd4040: Failed to read CHIP_ID1\n");
        return ret;
    }
    printk("Venom CHIP_ID1: 0x%08X\n", chip_id1);

    pr_info("=========================================\n");
    pr_info("jbd4040 Panel Configured Perfectly!\n");
    pr_info("jbd4040 CHIP_ID0: 0x%08X\n", chip_id0);
    pr_info("jbd4040 CHIP_ID1: 0x%08X\n", chip_id1);
    pr_info("=========================================\n");
    return 0;
}


// --------- I2C Driver Probe/Remove ---------
static int jbd4040_probe(struct i2c_client *client)
{
    int ret;
    struct device_node *regmap_node, *child;
    u32 addr;
    u32 bitmask;
    const char *label;

    struct device *dev = &client->dev; /* 方便代碼閱讀 */

    printk("Venom Enter %s\n",__func__);

    /* --- 【新增】分配私有結構體空間 --- */
    g_priv = devm_kzalloc(dev, sizeof(struct jbd4040_priv), GFP_KERNEL);
    if (!g_priv)
        return -ENOMEM;

    /* --- 【新增】從 DTS 自動抓取並請求 5 根 GPIO 資源 --- */
    /* devm_gpiod_get 會自動參考 DTS 中名為 "dvdd-gpios" 的屬性 */
    g_priv->gpiod_dvdd = devm_gpiod_get_optional(dev, "dvdd", GPIOD_OUT_LOW);
    if (IS_ERR(g_priv->gpiod_dvdd)) return PTR_ERR(g_priv->gpiod_dvdd);

    g_priv->gpiod_vddi = devm_gpiod_get_optional(dev, "vddi", GPIOD_OUT_LOW);
    if (IS_ERR(g_priv->gpiod_vddi)) return PTR_ERR(g_priv->gpiod_vddi);

    g_priv->gpiod_avdd = devm_gpiod_get_optional(dev, "avdd", GPIOD_OUT_LOW);
    if (IS_ERR(g_priv->gpiod_avdd)) return PTR_ERR(g_priv->gpiod_avdd);

    g_priv->gpiod_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(g_priv->gpiod_reset)) return PTR_ERR(g_priv->gpiod_reset);

    g_priv->gpiod_avee = devm_gpiod_get_optional(dev, "avee", GPIOD_OUT_LOW);
    if (IS_ERR(g_priv->gpiod_avee)) return PTR_ERR(g_priv->gpiod_avee);

    /* --- 【新增】依照開機順序執行硬體初始化 --- */
    jbd4040_power_on(g_priv);

    // 原有的暫存器初始化與暫存器寫入...
    // 提示：你可以在這行之後緊接著呼叫：

    msleep(200);
    ret = jbd4040_init_registers(client);
    if(ret < 0){
    	dev_err(&client->dev, "jbd4040 hardware register init failed!\n");
    }

    msleep(200);
    jbd4040_avee_on(g_priv);

    memcpy(&clientR, client, sizeof(clientR));
    memcpy(&clientG, client, sizeof(clientG));
    memcpy(&clientB, client, sizeof(clientB));

    gClientR = &clientR;
    gClientG = &clientG;
    gClientB = &clientB;

    gClientR->addr = 0x59;
    gClientG->addr = 0x5a;
    gClientB->addr = 0x5b;

    // Allocate char device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk("alloc chrdev region failed\n");
        return ret;
    }

    // Register char device
    cdev_init(&jbd4040_cdev, &fops);
    ret = cdev_add(&jbd4040_cdev, dev_num, 1);
    if (ret < 0) {
	printk("cdev add failed\n");
        goto unregister_chrdev;
    }

    // Create device class
    jbd4040_class = class_create(CLASS_NAME);
    if (IS_ERR(jbd4040_class)) {
        ret = PTR_ERR(jbd4040_class);
	printk("class create failed\n");
        goto del_cdev;
    }

    // Create device node in /dev
    device_create(jbd4040_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Create sysfs attributes
    dev_info(&client->dev, "Venom jbd4040 ready to create sysfs ");
    // old method. cannot remove	    
    device_create_file(&client->dev, &dev_attr_reg);
    device_create_file(&client->dev, &dev_attr_val);

    // new method
    //pr_info("=========================================\n");

    /* 【強制手動建立群組】 */
    //if (sysfs_create_group(&client->dev.kobj, &jbd4040_attr_group)) {
    //    dev_err(&client->dev, "Failed to create sysfs group\n");
    //}


    mutex_init(&jbd4040_mutex);

    regmap_node = of_get_child_by_name(client->dev.of_node, "regmap");
    if (regmap_node) {
        for_each_child_of_node(regmap_node, child) {
            if (of_property_read_u32(child, "reg", &addr))
                continue;
            if (of_property_read_u32(child, "bitmask", &bitmask))
                continue;
            if (of_property_read_string(child, "label", &label))
                continue;

            dev_info(&client->dev, "Registering %s @ 0x%02x\n", label, addr);

            struct my_dev_attr *newattr = devm_kzalloc(&client->dev,
                                                sizeof(*newattr), GFP_KERNEL);
            if (!newattr)
                return -ENOMEM;

            newattr->addr = addr;
            newattr->bitmask = bitmask;
            newattr->dev = &client->dev;
            strscpy(newattr->label, label, sizeof(newattr->label));

            newattr->dev_attr.attr.name = newattr->label;
            if (!strcmp("temperature", newattr->label)) {
                newattr->dev_attr.attr.mode = 0444;
            }
            else {
                newattr->dev_attr.attr.mode = 0664;
            }
            newattr->dev_attr.show = my_attr_show;
            newattr->dev_attr.store = my_attr_store;

            int ret = device_create_file(&client->dev, &newattr->dev_attr);
            if (ret) {
                dev_warn(&client->dev, "Failed to create sysfs attr %s\n", label);
                devm_kfree(&client->dev, newattr);
            }
        }
    }
    else {
        dev_info(&client->dev, "No regmap to be registered\n");
    }

    dev_info(&client->dev, "JB-Display 4040 I2C driver probed\n");
    
    return 0;

del_cdev:
    cdev_del(&jbd4040_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void jbd4040_remove(struct i2c_client *client)
{
    struct device_node *regmap_node, *child;
    u32 addr, bitmask;
    const char *label;   
       	
    dev_info(&client->dev, "JB-Display 4040 I2C driver ready to remove\n");
    jbd4040_avee_off(g_priv);
    jbd4040_power_off(g_priv);

    device_remove_file(&client->dev, &dev_attr_reg);
    device_remove_file(&client->dev, &dev_attr_val);
    /* ========================================================
     * 2. 最小幅度修改：在 remove 中再次遍歷 DTS，動態刪除檔案
     * ======================================================== */
    regmap_node = of_get_child_by_name(client->dev.of_node, "regmap");
    if (regmap_node) {
        for_each_child_of_node(regmap_node, child) {
            if (of_property_read_u32(child, "reg", &addr))
                continue;
            if (of_property_read_u32(child, "bitmask", &bitmask))
                continue;
            if (of_property_read_string(child, "label", &label))
                continue;

            /* * 這裡我們不需要完整的 my_dev_attr，
             * 我們只需要一個臨時的 device_attribute 來告訴核心「我要刪除哪個檔名的檔案」。
             */
            struct device_attribute tmp_attr;
            sysfs_attr_init(&tmp_attr.attr);
            tmp_attr.attr.name = label; /* 把 DTS 讀出來的 label 當作檔名傳進去 */

            dev_info(&client->dev, "Force removing dynamic sysfs attr: %s\n", label);

            /* 核心主要是認 .attr.name 這個字串來刪除檔案 */
            device_remove_file(&client->dev, &tmp_attr);
        }
        of_node_put(regmap_node);
    }

    //dev_info("Venom jbd4040 remvoe sysfs group");
    //sysfs_remove_group(&client->dev.kobj, &jbd4040_attr_group);

    device_destroy(jbd4040_class, dev_num);
    class_destroy(jbd4040_class);
    cdev_del(&jbd4040_cdev);
    unregister_chrdev_region(dev_num, 1);
    dev_info(&client->dev, "JB-Display 4040 I2C driver remove ok\n");
}

// --------- Device Match Tables ---------
static const struct i2c_device_id jbd4040_id[] = {
    { "jbd4040i2cdev", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, jbd4040_id);

static const struct of_device_id jbd4040_of_match[] = {
    { .compatible = "gis,jbd4040_i2c" },
    {},
};
MODULE_DEVICE_TABLE(of, jbd4040_of_match);

// --------- I2C Driver Structure ---------
static struct i2c_driver jbd4040_driver = {
    .driver = {
        .name = "jbd4040_i2c_driver",
        .of_match_table = jbd4040_of_match,
    },
    .probe = jbd4040_probe,
    .remove = jbd4040_remove,
    .id_table = jbd4040_id,
};

module_i2c_driver(jbd4040_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Chang");
MODULE_DESCRIPTION("JB-Display 4040 I2C driver with ioctl and sysfs");
