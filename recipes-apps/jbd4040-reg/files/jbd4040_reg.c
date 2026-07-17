#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_NODE "/dev/jbd4040"

/* 必須與你 Kernel Driver 定義的結構完全一致 */
struct i2c_ioctl_data {
    uint32_t reg;
    uint32_t val;
};

#define IOCTL_READ_REG32    _IOR('J', 0xF1, struct i2c_ioctl_data)
#define IOCTL_READ_REG16    _IOR('J', 0xF2, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG32   _IOW('J', 0xF3, struct i2c_ioctl_data)
#define IOCTL_WRITE_REG16   _IOW('J', 0xF4, struct i2c_ioctl_data)

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s r32 <reg_addr>\n", prog);
    fprintf(stderr, "  %s r16 <reg_addr>\n", prog);
    fprintf(stderr, "  %s w32 <reg_addr> <val>\n", prog);
    fprintf(stderr, "  %s w16 <reg_addr> <val>\n\n", prog);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s r32 0x0302ae38\n", prog);
    fprintf(stderr, "  %s w16 0x0302ae38 0x1fff\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return -1;
    }

    const char *mode = argv[1];
    struct i2c_ioctl_data data;
    memset(&data, 0, sizeof(data));

    /* 參數2: 暫存器地址 (基底設為0，代表自動判斷 0x 或純數字) */
    data.reg = (uint32_t)strtoul(argv[2], NULL, 0);

    int is_write = (strcmp(mode, "w32") == 0 || strcmp(mode, "w16") == 0);
    int is_16bit = (strcmp(mode, "r16") == 0 || strcmp(mode, "w16") == 0);

    if (is_write) {
        if (argc < 4) {
            fprintf(stderr, "Error: Write mode requires a <value> argument.\n\n");
            print_usage(argv[0]);
            return -1;
        }
        /* 參數3: 欲寫入的值 */
        data.val = (uint32_t)strtoul(argv[3], NULL, 0);
    }

    /* 決定 IOCTL 指令 */
    unsigned long cmd = 0;
    if      (strcmp(mode, "r32") == 0) cmd = IOCTL_READ_REG32;
    else if (strcmp(mode, "r16") == 0) cmd = IOCTL_READ_REG16;
    else if (strcmp(mode, "w32") == 0) cmd = IOCTL_WRITE_REG32;
    else if (strcmp(mode, "w16") == 0) cmd = IOCTL_WRITE_REG16;
    else {
        fprintf(stderr, "Error: Unknown operation mode '%s'\n\n", mode);
        print_usage(argv[0]);
        return -1;
    }

    int fd = open(DEVICE_NODE, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", DEVICE_NODE, strerror(errno));
        return -1;
    }

    if (ioctl(fd, cmd, &data) < 0) {
        fprintf(stderr, "ioctl(%s, reg:0x%08X) failed: %s\n", mode, data.reg, strerror(errno));
        close(fd);
        return -1;
    }

    /* 輸出結果 (讀取模式時只印出 Value，方便被 Shell script 接住) */
    if (!is_write) {
        if (is_16bit)
            printf("0x%04X\n", (uint16_t)data.val);
        else
            printf("0x%08X\n", data.val);
    } else {
        printf("Write %s [0x%08X] = 0x%X : SUCCESS\n", 
               is_16bit ? "16bit" : "32bit", data.reg, data.val);
    }

    close(fd);
    return 0;
}
