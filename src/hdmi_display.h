/*******************************************************************************
 * file hdmi_display.h
 * Public interface for the HDMI display wrapper.
 *
 * Provides framebuffer management and control functions for the IT6263 bridge.
 *
 * Expected sequence:
 *   1. hdmi_display_init()          - Called once in main() before graphics setup.
 *   2. hdmi_display_create_window() - Called after configuring blit instruction buffer.
 *   3. hdmi_get_framebuffer()       - Retrieves VRAM address and surface properties.
 *   4. hdmi_present()               - Commits window updates after drawing.
 *   5. hdmi_service()               - Called periodically in the main idle loop.
 *
 ******************************************************************************/
#ifndef HDMI_DISPLAY_H_
#define HDMI_DISPLAY_H_

#include "cygfx_driver_api.h"
#include "hdmi_panel_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define the width and height of the drawing canvas in pixels */
#define HDMI_FB_WIDTH     HDMI_PANEL_HACT
#define HDMI_FB_HEIGHT    HDMI_PANEL_VACT
/* Set the frame buffer to 2 bytes per pixel */
#define HDMI_FB_BPP       HDMI_PANEL_FB_BPP
/* Set the color format to RGB565, 16-bit color */
#define HDMI_FB_FORMAT    CYGFX_SM_FORMAT_R5G6B5

/* Where the drawing area sits on the panel. Both zero while the framebuffer is
 * the full panel size; the expression centres it automatically if you make the
 * framebuffer smaller. */
#define HDMI_WIN_X        ((HDMI_PANEL_HACT - HDMI_FB_WIDTH)  / 2u)
#define HDMI_WIN_Y        ((HDMI_PANEL_VACT - HDMI_FB_HEIGHT) / 2u)


/* This structure exposes the allocated framebuffer memory properties to drawing layers
 * base: The physical starting memory address in VRAM where CPU or blitter pixel writes occur 
 * width & height: Active canvas dimensions 
 * strideBytes: The byte length of one horizontal line 
 * format: The color arrangement 
 * surface: Pointer to the to the blit engine STORE target 
 */
typedef struct
{
    void*                    base;
    CYGFX_U32                width;
    CYGFX_U32                height;
    CYGFX_U32                strideBytes;
    CYGFX_SM_FORMAT          format;
    CYGFX_SURFACE_OBJECT_S*  surface;
} hdmi_fb_info_t;

/* Called once from main(). Powers up the VIDEOSS peripheral, sets up VideoSS interrupts, 
 * initializes the I2C link to the IT6263 bridge, and allocates framebuffer in VRAM
 */
CYGFX_ERROR hdmi_display_init(void);

/*
 * Called from gfx.c after the blit engine instruction buffer is allocated. 
 * It maps layer 0 to the framebuffer, starts the PLL pixel clock, enables FPD-Link output, 
 * and blocks until the IT6263 chip locks onto the live LVDS stream
 */
CYGFX_ERROR hdmi_display_create_window(void);

/* After init: get fb pointer + surface for drawing. */
void hdmi_get_framebuffer(hdmi_fb_info_t* out);

/* Push finished pixels to the panel (WinCommit + DispCommit). Call after each draw step */
void hdmi_present(void);

/* Must be called periodically in the application's background/idle loop. It monitors Hot Plug Detect (HPD)
 * state and services the IT6263 so the monitor connection remains active. 
 */
void hdmi_service(void);

#ifdef __cplusplus
}
#endif

#endif /* HDMI_DISPLAY_H_ */
