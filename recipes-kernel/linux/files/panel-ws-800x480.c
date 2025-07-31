// SPDX-License-Identifier: GPL-2.0
/*
* Copyright © 2025 GiS Ltd
* Based on panel-raspberrypi-touchscreen 
*/

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/pm.h>

#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_connector.h>
#include <drm/drm_drv.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_crtc_helper.h>

#define WS_DSI_DRIVER_NAME "ws-dsi"


struct vpanel_desc {
    /**
     * @modes: Pointer to array of fixed modes appropriate for this panel.
     *
     * If only one mode then this can just be the address of the mode.
     * NOTE: cannot be used with "timings" and also if this is specified
     * then you cannot override the mode in the device tree.
     */
    const struct drm_display_mode *modes;

    /** @num_modes: Number of elements in modes array. */
    unsigned int num_modes;

    /**
     * @timings: Pointer to array of display timings
     *
     * NOTE: cannot be used with "modes" and also these will be used to
     * validate a device tree override if one is present.
     */
    const struct display_timing *timings;

    /** @num_timings: Number of elements in timings array. */
    unsigned int num_timings;

    /** @bpc: Bits per color. */
    unsigned int bpc;

    /** @size: Structure containing the physical size of this panel. */
    struct {
        /**
         * @size.width: Width (in mm) of the active display area.
         */
        unsigned int width;

        /**
         * @size.height: Height (in mm) of the active display area.
         */
        unsigned int height;
    } size;

    /** @delay: Structure containing various delay values for this panel. */
    struct {
        /**
         * @delay.prepare: Time for the panel to become ready.
         *
         * The time (in milliseconds) that it takes for the panel to
         * become ready and start receiving video data
         */
        unsigned int prepare;

        /**
         * @delay.enable: Time for the panel to display a valid frame.
         *
         * The time (in milliseconds) that it takes for the panel to
         * display the first valid frame after starting to receive
         * video data.
         */
        unsigned int enable;

        /**
         * @delay.disable: Time for the panel to turn the display off.
         *
         * The time (in milliseconds) that it takes for the panel to
         * turn the display off (no content is visible).
         */
        unsigned int disable;

        /**
         * @delay.unprepare: Time to power down completely.
         *
         * The time (in milliseconds) that it takes for the panel
         * to power itself down completely.
         *
         * This time is used to prevent a future "prepare" from
         * starting until at least this many milliseconds has passed.
         * If at prepare time less time has passed since unprepare
         * finished, the driver waits for the remaining time.
         */
        unsigned int unprepare;
    } delay;

    /** @bus_format: See MEDIA_BUS_FMT_... defines. */
    u32 bus_format;

    /** @bus_flags: See DRM_BUS_FLAG_... defines. */
    u32 bus_flags;

    /** @connector_type: LVDS, eDP, DSI, DPI, etc. */
    int connector_type;
};



struct vpanel_desc_dsi {
    struct vpanel_desc desc;

    unsigned long flags;
    enum mipi_dsi_pixel_format format;
    unsigned int lanes;
};

static const struct drm_display_mode ws_800x480_mode = {
    .clock = 33000, // kHz 假設值
    .hdisplay = 800,
    .hsync_start = 800 + 40,
    .hsync_end = 800 + 40 + 128,
    .htotal = 800 + 40 + 128 + 88,
    .vdisplay = 480,
    .vsync_start = 480 + 10,
    .vsync_end = 480 + 10 + 2,
    .vtotal = 480 + 10 + 2 + 33,
};

static const struct vpanel_desc_dsi ws_800x480_desc_dsi = {
    .desc = {
        .modes = &ws_800x480_mode,
        .num_modes = 1,
        .bpc = 8,
        .size = {
            .width = 192,
            .height = 168,  
        },
        .connector_type = DRM_MODE_CONNECTOR_DSI,
    },
    .flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
    .format = MIPI_DSI_FMT_RGB888,
    .lanes = 2,
};


static const struct of_device_id vws_panel_of_match[] = {
    {
        .compatible = "ws,ws_800x480",
        .data = &ws_800x480_desc_dsi,
    },
};


static int panel_vws_dsi_probe(struct mipi_dsi_device *dsi)
{
    printk("panel vws dsi probe");
    const struct panel_desc_dsi *desc;
    int err = -1;

    desc = of_device_get_match_data(&dsi->dev);
    if (!desc){
        return -ENODEV;
    }
    printk("Venom got desc vvvv!");
    
#if 0
    err = panel_simple_probe(&dsi->dev, &desc->desc);
    if (err < 0)
        return err;

    dsi->mode_flags = desc->flags;
    dsi->format = desc->format;
    dsi->lanes = desc->lanes;

    err = mipi_dsi_attach(dsi);
    if (err) {
        struct panel_simple *panel = mipi_dsi_get_drvdata(dsi);

        drm_panel_remove(&panel->base);
    }
#endif
    return err;
}

static void panel_vws_dsi_remove(struct mipi_dsi_device *dsi)
{
#if 0
    int err;
    err = mipi_dsi_detach(dsi);
    if (err < 0)
        dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", err);

    panel_simple_remove(&dsi->dev);
#endif
}

static void panel_vws_dsi_shutdown(struct mipi_dsi_device *dsi)
{
#if 0
    panel_simple_shutdown(&dsi->dev);
#endif
}


static struct mipi_dsi_driver panel_vws_dsi_driver = {
    .driver = {
        .name = "ws_800x480",
        .of_match_table = vws_panel_of_match,
    },
    .probe = panel_vws_dsi_probe,
    .remove = panel_vws_dsi_remove,
    .shutdown = panel_vws_dsi_shutdown,
};



static int __init panel_vws_init(void)
{
    int err;
    printk("panel_vws_init!\n");
    if (IS_ENABLED(CONFIG_DRM_MIPI_DSI)) {
        err = mipi_dsi_driver_register(&panel_vws_dsi_driver);
        if (err < 0)
            return err;
    }

    return 0;
}
module_init(panel_vws_init);

static void __exit panel_vws_exit(void)
{
    printk("panel_vws_exit!\n");
    if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
        mipi_dsi_driver_unregister(&panel_vws_dsi_driver);

}
module_exit(panel_vws_exit);

MODULE_AUTHOR("Jason Chang <jason.chang@gis.com.tw>");
MODULE_DESCRIPTION("Waveshare 5.5 800x480 DSI panel driver");
MODULE_LICENSE("GPL");
                      
