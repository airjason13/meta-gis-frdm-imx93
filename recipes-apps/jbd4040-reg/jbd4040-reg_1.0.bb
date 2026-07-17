SUMMARY = "JBD4040 I2C Register Read/Write Utility"
DESCRIPTION = "Userspace CLI tool for reading and writing JBD4040 registers via ioctl"
AUTHOR = "Your Name <your.email@example.com>"
SECTION = "console/tools"

# 如果此程式為專屬授權或內部工具，可設為 CLOSED；若是開源請修改為 MIT 或 GPLv2 等
LICENSE = "CLOSED"
# 如果使用開源授權（例如 MIT），需要指定 LIC_FILES_CHKSUM，例如：
# LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://jbd4040_reg.c \
    file://Makefile \
"

# 由於從 file:// 讀取的檔案會直接放進 WORKDIR，將編譯來源目錄 S 指向該處
S = "${WORKDIR}"

# 傳遞 Yocto 的交叉編譯工具鏈參數給 Makefile
EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS} ${LDFLAGS}'"

do_compile() {
    oe_runmake
}

do_install() {
    # 建立目標檔案系統的 /usr/bin 目錄
    install -d ${D}${bindir}
    
    # 將編譯完成的執行檔安裝到 /usr/bin，並設定執行權限 (0755)
    install -m 0755 ${S}/jbd4040_reg ${D}${bindir}/
}
