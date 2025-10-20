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


#define DEVICE_NAME "jbd4020"
#define CLASS_NAME "gis"

#define IOCTL_READ_REG32    _IOR('M', 0x01, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG32   _IOW('M', 0x02, struct i2c_ioctl_data)

#define IOCTL_LUMIN_GET     _IOR('R', 0x01, struct jbd4020_luminance)
#define IOCTL_CURRENT_GET   _IOR('R', 0x02, struct jbd4020_current)
#define IOCTL_TEMP_READ     _IOR('R', 0x03, struct jbd4020_temp)
#define IOCTL_FLIP_GET      _IOR('R', 0x04, struct jbd4020_flip)
#define IOCTL_MIRROR_GET    _IOR('R', 0x05, struct jbd4020_mirror)
#define IOCTL_OFFSET_GET    _IOR('R', 0x06, struct jbd4020_offset)

#define IOCTL_LUMIN_SET     _IOW('W', 0x01, struct jbd4020_luminance)
#define IOCTL_CURRENT_SET   _IOW('W', 0x02, struct jbd4020_current)
#define IOCTL_FLIP_SET      _IOW('W', 0x04, struct jbd4020_flip)
#define IOCTL_MIRROR_SET    _IOW('W', 0x05, struct jbd4020_mirror)
#define IOCTL_OFFSET_SET    _IOW('W', 0x06, struct jbd4020_offset)

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


// Structure for IOCTL data transfer
struct i2c_ioctl_data {
    uint32_t reg;
    uint32_t val;
};

struct jbd4020_luminance {
    u16 r;
    u16 g;
    u16 b;
    u8 status;
};

struct jbd4020_current {
    u8 r;
    u8 g;
    u8 b;
    u8 status;
};

struct jbd4020_temp {
    u32 r;
    u32 g;
    u32 b;
    u8 status;
};

struct jbd4020_flip {
    u8 en;
    u8 status;
};

struct jbd4020_mirror {
    u8 en;
    u8 status;
};

struct jbd4020_offset {
    u8 en;
    u16 rh;
    u16 rv;
    u16 gh;
    u16 gv;
    u16 bh;
    u16 bv;
    u8 status;
};

static struct i2c_client clientG;
static struct i2c_client clientB;

static struct i2c_client *gClientR;
static struct i2c_client *gClientG;
static struct i2c_client *gClientB;
static dev_t dev_num;
static struct cdev jbd4020_cdev;
static struct class *jbd4020_class;
static DEFINE_MUTEX(jbd4020_mutex);


// --------- I2C 32-bit Register Read/Write ---------
static int i2c_read_reg32(struct i2c_client *client, uint32_t reg, uint32_t *val)
{
    uint8_t buf[4];
    struct i2c_msg msgs[2];

    // Prepare register address (32-bit)
    buf[0] = (reg >> 24) & 0xFF;
    buf[1] = (reg >> 16) & 0xFF;
    buf[2] = (reg >> 8) & 0xFF;
    buf[3] = reg & 0xFF;

    msgs[0].addr = client->addr;
    msgs[0].flags = 0;
    msgs[0].len = 4;
    msgs[0].buf = buf;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 4;
    msgs[1].buf = buf;

    if (i2c_transfer(client->adapter, msgs, 2) != 2)
        return -EIO;

    *val = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
    return 0;
}

static int i2c_write_reg32(struct i2c_client *client, uint32_t reg, uint32_t val)
{
    uint8_t buf[8];

    // Prepare register and value (both 32-bit)
    buf[0] = (reg >> 24) & 0xFF;
    buf[1] = (reg >> 16) & 0xFF;
    buf[2] = (reg >> 8) & 0xFF;
    buf[3] = reg & 0xFF;
    buf[4] = val & 0xFF;
    buf[5] = (val >> 8) & 0xFF;
    buf[6] = (val >> 16) & 0xFF;
    buf[7] = (val >> 24) & 0xFF;

    return (i2c_master_send(client, buf, 8) == 8) ? 0 : -EIO;
}

// --------- Character Device IOCTL ---------
static long jbd4020_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    mutex_lock(&jbd4020_mutex);

    switch (cmd) {
        case IOCTL_READ_REG32: 
            {
                struct i2c_ioctl_data data;

                if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_read_reg32(gClientR, data.reg, &data.val)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_WRITE_REG32:
            {
                struct i2c_ioctl_data data;

                if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_write_reg32(gClientR, data.reg, data.val)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_LUMIN_GET:
            {
                struct jbd4020_luminance lumin;
                uint32_t temp;

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
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_LUMIN_SET:
            {
                struct jbd4020_luminance lumin;
                uint32_t temp;

                lumin.status = 0;
                if (copy_from_user(&lumin, (void __user *)arg, sizeof(lumin))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.r & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientR, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_read_reg32(gClientG, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.g & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientG, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_read_reg32(gClientB, REG_BRIGHTNESS, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_BRIGHTNESS;
                temp |= lumin.b & MASK_BRIGHTNESS;
                if (i2c_write_reg32(gClientB, REG_BRIGHTNESS, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &lumin, sizeof(lumin))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_CURRENT_GET:
            {
                struct jbd4020_current curr;
                uint32_t temp;

                curr.status = 0;
                if (i2c_read_reg32(gClientR, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                curr.r = temp & MASK_CURRENT;

                if (i2c_read_reg32(gClientG, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                curr.g = temp & MASK_CURRENT;

                if (i2c_read_reg32(gClientB, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                curr.b = temp & MASK_CURRENT;

                if (copy_to_user((void __user*)arg, &curr, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_CURRENT_SET:
            {
                struct jbd4020_current curr;
                uint32_t temp;

                curr.status = 0;
                if (copy_from_user(&curr, (void __user *)arg, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.r & MASK_CURRENT;
                if (i2c_write_reg32(gClientR, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_read_reg32(gClientG, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.g & MASK_CURRENT;
                if (i2c_write_reg32(gClientG, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (i2c_read_reg32(gClientB, REG_CURRENT, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_CURRENT;
                temp |= curr.b & MASK_CURRENT;
                if (i2c_write_reg32(gClientB, REG_CURRENT, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &curr, sizeof(curr))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_TEMP_READ:
            {
                struct jbd4020_temp stemp;
                uint32_t temp;

                stemp.status = 0;
                if (i2c_read_reg32(gClientR, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                stemp.r = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (i2c_read_reg32(gClientG, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                stemp.g = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (i2c_read_reg32(gClientB, REG_TEMPERATURE, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                stemp.b = (((temp & MASK_TEMPERATURE) - 118) * 165000) / 815 - 40000;

                if (copy_to_user((void __user *)arg, &stemp, sizeof(stemp))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_FLIP_GET:
            {
                struct jbd4020_flip flip;
                uint32_t temp;

                flip.status = 0;
                if (i2c_read_reg32(gClientR, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                flip.en = (temp & MASK_FLIP_SRAM) == MASK_FLIP_SRAM;

                if (copy_to_user((void __user*)arg, &flip, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_FLIP_SET:
            {
                struct jbd4020_flip flip;
                uint32_t temp;

                if (copy_from_user(&flip, (void __user *)arg, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
                
                flip.status = 0;
                // SRAM
                if (i2c_read_reg32(gClientR, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientR, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientG, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientG, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientG, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientB, REG_FLIP_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_SRAM;
                if (flip.en) {
                    temp |= MASK_FLIP_SRAM;
                }
                if (i2c_write_reg32(gClientB, REG_FLIP_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_FLIP_VO;
                if (flip.en) {
                    temp |= MASK_FLIP_VO;
                }
                if (i2c_write_reg32(gClientB, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &flip, sizeof(flip))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_MIRROR_GET:
            {
                struct jbd4020_mirror mirror;
                uint32_t temp;

                mirror.status = 0;
                if (i2c_read_reg32(gClientR, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                mirror.en = (temp & MASK_MIRROR_SRAM) == MASK_MIRROR_SRAM;

                if (copy_to_user((void __user*)arg, &mirror, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_MIRROR_SET:
            {
                struct jbd4020_mirror mirror;
                uint32_t temp;

                if (copy_from_user(&mirror, (void __user *)arg, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
                
                // SRAM
                if (i2c_read_reg32(gClientR, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientR, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientR, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientG, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (0 == mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientG, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (0 == mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientG, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                // SRAM
                if (i2c_read_reg32(gClientB, REG_MIRROR_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_SRAM;
                if (mirror.en) {
                    temp |= MASK_MIRROR_SRAM;
                }
                if (i2c_write_reg32(gClientB, REG_MIRROR_SRAM, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                // Video path
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                temp &= ~MASK_MIRROR_VO;
                if (mirror.en) {
                    temp |= MASK_MIRROR_VO;
                }
                if (i2c_write_reg32(gClientB, REG_FMO_VO, temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }

                mirror.status = 0;

                if (copy_to_user((void __user*)arg, &mirror, sizeof(mirror))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_OFFSET_GET:
            {
                struct jbd4020_offset offset;
                uint32_t temp;

                offset.status = 0;
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                offset.en = (temp & MASK_OFFSET_VO) == MASK_OFFSET_VO;
                offset.rh = temp & MASK_OFFSET_H_VO;
                offset.rv = (temp & MASK_OFFSET_V_VO) >> 16;
                
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                offset.gh = temp & MASK_OFFSET_H_VO;
                offset.gv = (temp & MASK_OFFSET_V_VO) >> 16;
                
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
                }
                offset.bh = temp & MASK_OFFSET_H_VO;
                offset.bv = (temp & MASK_OFFSET_V_VO) >> 16;

                if (copy_to_user((void __user*)arg, &offset, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        case IOCTL_OFFSET_SET:
            {
                struct jbd4020_offset offset;
                uint32_t temp;

                if (copy_from_user(&offset, (void __user *)arg, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientR, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientG, REG_OFFSET_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientG, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientB, REG_OFFSET_SRAM, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }
                
                if (i2c_read_reg32(gClientB, REG_FMO_VO, &temp)) {
                    ret = -EIO;
                    goto jbd4020_ioctl_exit;
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
                    goto jbd4020_ioctl_exit;
                }

                if (copy_to_user((void __user*)arg, &offset, sizeof(offset))) {
                    ret = -EFAULT;
                    goto jbd4020_ioctl_exit;
                }
            }
            break;

        default:
            ret = -EINVAL;
            goto jbd4020_ioctl_exit;
    }
jbd4020_ioctl_exit:
    mutex_unlock(&jbd4020_mutex);
    return ret;
}

static int jbd4020_open(struct inode *inode, struct file *file) {
    return 0;
}

static int jbd4020_release(struct inode *inode, struct file *file) {
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = jbd4020_ioctl,
    .open = jbd4020_open,
    .release = jbd4020_release,
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


#define DEF_I2C_JBD4020_MAX_ARGS        8

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
    uint32_t count = 0;
    uint32_t tempR = 0, tempG = 0, tempB = 0;

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
}

static ssize_t show_flip(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t val = 0;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &val)) {
        val &= itemattr->bitmask;
        count = scnprintf(buf, PAGE_SIZE, "Flip is %s\n", val ? "Enabled" : "Disabled");
    }

    return count;
}

static ssize_t show_mirror(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t val = 0;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &val)) {
        val &= itemattr->bitmask;
        count = scnprintf(buf, PAGE_SIZE, "Mirror is %s\n", val ? "Enabled" : "Disabled");
    }

    return count;
}

static ssize_t show_offset(struct device *dev, struct my_dev_attr *itemattr, char *buf)
{
    uint32_t count = 0;
    uint32_t offsetR = 0, offsetG = 0, offsetB = 0;
    //uint32_t rh = 0, rv = 0, gh = 0, gv = 0, bh = 0, bv = 0;

    if (0 == i2c_read_reg32(gClientR, itemattr->addr, &offsetR) &&
        0 == i2c_read_reg32(gClientG, itemattr->addr, &offsetG) &&
        0 == i2c_read_reg32(gClientB, itemattr->addr, &offsetB)) {
        count = scnprintf(buf, PAGE_SIZE, "R(%s) H:%u, V:%u\nG(%s) H:%u, V:%u\nB(%s) H:%u, V:%u\n",
                offsetR & 0x01 ? "enabled" : "disabled", (offsetR >> 4) & 0x1F, (offsetR >> 12) & 0x1F,
                offsetG & 0x01 ? "enabled" : "disabled", (offsetG >> 4) & 0x1F, (offsetG >> 12) & 0x1F,
                offsetB & 0x01 ? "enabled" : "disabled", (offsetB >> 4) & 0x1F, (offsetB >> 12) & 0x1F);
    }

    return count;
}

// Functions for handling "echo"
static ssize_t store_luminance(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4020_MAX_ARGS];
    int argc = 0;
    int ret;
    struct i2c_client *client;
    uint32_t temp;

    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4020_MAX_ARGS) {
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
        client = gClientR;
    }
    else if (!strcmp(argv[0], "g") || !strcmp(argv[0], "G")) {
        client = gClientG;
    }
    else if (!strcmp(argv[0], "b") || !strcmp(argv[0], "B")) {
        client = gClientB;
    }
    else {
        dev_warn(dev, "Panel incorrect: %s\n", argv[0]);
        goto out;
    }

    // 
    unsigned int value;
    ret = kstrtouint(argv[1], 0, &value);
    if (ret) {
        dev_warn(dev, "Invalid value: %s\n", argv[1]);
        goto out;
    }

    // 
    dev_info(dev, "Parsed: panel=%s, value=%d", argv[0], value);
    if (8191 < value) {
        dev_info(dev, "Value greater than limitation, will be truncated");
    }

    // TODO: I2C
    if (!i2c_read_reg32(client, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(client, itemattr->addr, temp);
    }

out:
    kfree(input);
    return count;
}

static ssize_t store_current(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4020_MAX_ARGS];
    int argc = 0;
    int ret;
    struct i2c_client *client;
    uint32_t temp;

    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4020_MAX_ARGS) {
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
        client = gClientR;
    }
    else if (!strcmp(argv[0], "g") || !strcmp(argv[0], "G")) {
        client = gClientG;
    }
    else if (!strcmp(argv[0], "b") || !strcmp(argv[0], "B")) {
        client = gClientB;
    }
    else {
        dev_warn(dev, "Panel incorrect: %s\n", argv[0]);
        goto out;
    }

    // 
    unsigned int value;
    ret = kstrtouint(argv[1], 0, &value);
    if (ret) {
        dev_warn(dev, "Invalid value: %s\n", argv[1]);
        goto out;
    }

    // 
    dev_info(dev, "Parsed: panel=%s, value=%d\n", argv[0], value);
    if (255 < value) {
        dev_info(dev, "Value greater than limitation, will be truncated");
    }

    // TODO: I2C
    if (!i2c_read_reg32(client, itemattr->addr, &temp)) {
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(client, itemattr->addr, temp);
    }

out:
    kfree(input);
    return count;
}

static ssize_t store_flip(struct device *dev, struct my_dev_attr *itemattr,
                            const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4020_MAX_ARGS];
    int argc = 0;
    int ret;
    uint32_t temp;

    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4020_MAX_ARGS) {
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
}

static ssize_t store_mirror(struct device *dev, struct my_dev_attr *itemattr,
                            const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4020_MAX_ARGS];
    int argc = 0;
    int ret;
    uint32_t temp;

    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4020_MAX_ARGS) {
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
}

static ssize_t store_offset(struct device *dev, struct my_dev_attr *itemattr, const char *buf, size_t count)
{
    char *input, *token;
    char *argv[DEF_I2C_JBD4020_MAX_ARGS];
    int argc = 0;
    int ret;
    struct i2c_client *client;
    uint32_t temp;

    // 
    input = kstrndup(buf, count, GFP_KERNEL);
    if (!input)
        return -ENOMEM;

    // 
    while ((token = strsep(&input, " \t\n")) != NULL) {
        if (*token == '\0')
            continue;
        if (argc >= DEF_I2C_JBD4020_MAX_ARGS) {
            dev_warn(dev, "Too many arguments\n");
            goto out;
        }
        argv[argc++] = token;
    }

    // 
    if (argc != 2 && argc != 4) {
        dev_warn(dev, "Expected 2 or 4 arguments, got %d\n", argc);
        goto out;
    }

    // 
    if (!strcmp(argv[0], "r") || !strcmp(argv[0], "R")) {
        client = gClientR;
    }
    else if (!strcmp(argv[0], "g") || !strcmp(argv[0], "G")) {
        client = gClientG;
    }
    else if (!strcmp(argv[0], "b") || !strcmp(argv[0], "B")) {
        client = gClientB;
    }
    else {
        dev_warn(dev, "Panel incorrect: %s\n", argv[0]);
        goto out;
    }

    // 
    u32 en;
    ret = kstrtouint(argv[1], 0, &en);
    if (ret) {
        dev_warn(dev, "Invalid value: %s\n", argv[1]);
        goto out;
    }

    // 
    u32 val_h = 0;
    if (4 == argc) {
        ret = kstrtouint(argv[2], 0, &val_h);
        if (ret) {
            dev_warn(dev, "Invalid value: %s\n", argv[2]);
            goto out;
        }
    }

    // 
    u32 val_v = 0;
    if (4 == argc) {
        ret = kstrtouint(argv[3], 0, &val_v);
        if (ret) {
            dev_warn(dev, "Invalid value: %s\n", argv[3]);
            goto out;
        }
    }

    // 
    en &= 0x01;
    val_h &= 0x1F;
    val_v &= 0x1F;
    dev_info(dev, "Parsed: panel=%s, en=%u, h=%u, v=%u", argv[0], en, val_h, val_v);

    // TODO: I2C
    u32 value;
    if (!i2c_read_reg32(client, itemattr->addr, &temp)) {
        if (0 == en || 2 == argc) {
            val_v = (temp >> 12) & 0x1F;
            val_h = (temp >>  4) & 0x1F;
        }
        value = (val_v << 12) | (val_h << 4) | en;
        temp &= ~itemattr->bitmask;
        temp |= (value & itemattr->bitmask);
        i2c_write_reg32(client, itemattr->addr, temp);

        // VO path setting
        value = (en << 29) | (val_v << 16) | val_h;
        if (!i2c_read_reg32(client, REG_FMO_VO, &temp)) {
            temp &= ~0x2FFF0FFF;
            temp |= (value & 0x2FFF0FFF);
            i2c_write_reg32(client, REG_FMO_VO, temp);
        }
    }

out:
    kfree(input);
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

// --------- I2C Driver Probe/Remove ---------
static int jbd4020_probe(struct i2c_client *client)
{
    int ret;
    struct device_node *regmap_node, *child;
    u32 addr;
    u32 bitmask;
    const char *label;

    memcpy(&clientG, client, sizeof(clientG));
    memcpy(&clientB, client, sizeof(clientB));

    gClientR = client;
    gClientG = &clientG;
    gClientB = &clientB;

    gClientG->addr = 0x5a;
    gClientB->addr = 0x5b;

    // Allocate char device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        return ret;
    }

    // Register char device
    cdev_init(&jbd4020_cdev, &fops);
    ret = cdev_add(&jbd4020_cdev, dev_num, 1);
    if (ret < 0) {
        goto unregister_chrdev;
    }

    // Create device class
    jbd4020_class = class_create(CLASS_NAME);
    if (IS_ERR(jbd4020_class)) {
        ret = PTR_ERR(jbd4020_class);
        goto del_cdev;
    }

    // Create device node in /dev
    device_create(jbd4020_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Create sysfs attributes
    device_create_file(&client->dev, &dev_attr_reg);
    device_create_file(&client->dev, &dev_attr_val);

    mutex_init(&jbd4020_mutex);

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

    dev_info(&client->dev, "JB-Display 4020 I2C driver probed\n");
    
    return 0;

del_cdev:
    cdev_del(&jbd4020_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void jbd4020_remove(struct i2c_client *client)
{
    device_remove_file(&client->dev, &dev_attr_reg);
    device_remove_file(&client->dev, &dev_attr_val);
    device_destroy(jbd4020_class, dev_num);
    class_destroy(jbd4020_class);
    cdev_del(&jbd4020_cdev);
    unregister_chrdev_region(dev_num, 1);
}

// --------- Device Match Tables ---------
static const struct i2c_device_id jbd4020_id[] = {
    { "jbd4020i2cdev", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, jbd4020_id);

static const struct of_device_id jbd4020_of_match[] = {
    { .compatible = "gis,jbd4020_i2c" },
    {},
};
MODULE_DEVICE_TABLE(of, jbd4020_of_match);

// --------- I2C Driver Structure ---------
static struct i2c_driver jbd4020_driver = {
    .driver = {
        .name = "jbd4020_i2c_driver",
        .of_match_table = jbd4020_of_match,
    },
    .probe = jbd4020_probe,
    .remove = jbd4020_remove,
    .id_table = jbd4020_id,
};

module_i2c_driver(jbd4020_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Calvin Chiang");
MODULE_DESCRIPTION("JB-Display 4020 I2C driver with ioctl and sysfs");
