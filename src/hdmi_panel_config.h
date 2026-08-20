/*******************************************************************************
 * file hdmi_panel_config.h
 * Central configuration for display timings, clock rates, and VRAM layout.
 *
 * Single point of definition for the 1920x720 60 Hz display subsystem:
 *   - Defines active geometry (HACT/VACT) and total blanking intervals.
 *   - Provides timing definitions for the IT6263 bridge and CyGfx driver.
 *   - Specifies pixel clock rates and VRAM allocation parameters.
 *
 * Target: Waveshare 12.3" (1920x720) via FPD-Link on KIT-T2G-C-2D-6M-LITE.
 ******************************************************************************/
#ifndef HDMI_PANEL_CONFIG_H_
#define HDMI_PANEL_CONFIG_H_

/*
 * Resolution & Geometry
 * Active area and totals
 * Active Area (HACT, VACT): 1920×720 visible screen resolution 
 * Total Frame (HTOTAL, VTOTAL): Total raster scan size including non-visible blanking periods (2180×756)
 * Blanking Intervals:
 *		Horizontal blanking =2180-1920=260
 *		Vertical blanking =756-720=36
 * HDMI_PANEL_REFRESH_HZ: The target refresh rate (60 complete frames drawn per second)
 */
#define HDMI_PANEL_HACT          1920u
#define HDMI_PANEL_VACT          720u
#define HDMI_PANEL_HTOTAL        2180u
#define HDMI_PANEL_VTOTAL        756u
#define HDMI_PANEL_REFRESH_HZ    60u
#define HDMI_PANEL_H_BLANK       (HDMI_PANEL_HTOTAL - HDMI_PANEL_HACT)
#define HDMI_PANEL_V_BLANK       (HDMI_PANEL_VTOTAL - HDMI_PANEL_VACT)

/*
 * The Blanking Splits (IT6263 vs. CyGfx)
 * IT6263 HDMI Transmitter Timing Split
 * The IT6263 HDMI bridge requires standard discrete video timing parameters:
 * Horizontal Split: 120" (Front Porch)"+32" (HSYNC Pulse)"+108" (Back Porch)"
 * Vertical Split (36" lines" ): 16" (Front Porch)"+10" (VSYNC Pulse)"+10" (Back Porch)"
 * HDMI_PANEL_SYNC_POL = 0u: Specifies active-low polarity for both HSYNC and VSYNC
 */
#define HDMI_PANEL_H_FRONT       120u
#define HDMI_PANEL_H_SYNC        32u
#define HDMI_PANEL_H_BACK        108u
#define HDMI_PANEL_V_FRONT       16u
#define HDMI_PANEL_V_SYNC        10u
#define HDMI_PANEL_V_BACK        10u
#define HDMI_PANEL_SYNC_POL      0u

/*
 * CyGfx Middleware Timing Parameters
 * HSYNC / VSYNC: Pulse width
 * HSBP / VSBP: Combined Sync Pulse + Back Porch duration
 * Infineon’s internal CyGfx display driver API does not accept Front Porch directly. 
 * Instead, it takes the Sync width and the combined Sync + Back Porch (SBP) value
 * "HSBP"="HSYNC"+"HBACK"=192
 * "VSBP"="VSYNC"+"VBACK"=26
 */
#define HDMI_PANEL_CYGFX_HSYNC   44u
#define HDMI_PANEL_CYGFX_HSBP    192u    /* hsync + horizontal back porch */
#define HDMI_PANEL_CYGFX_VSYNC   5u
#define HDMI_PANEL_CYGFX_VSBP    26u     /* vsync + vertical back porch   */

/*
 * Pixel Clock Configuration
 * 	_HZ: The calculated theoretical clock frequency. Used by PLL setup functions in cy_gfx_env.c
 *	_MHZ: The floating-point target frequency supplied to CyGfx_DispOpenDisplay()
 */
#define HDMI_PANEL_PIXEL_CLOCK_HZ   ((unsigned long)HDMI_PANEL_HTOTAL * \
                                     (unsigned long)HDMI_PANEL_VTOTAL * \
                                     (unsigned long)HDMI_PANEL_REFRESH_HZ)

#define HDMI_PANEL_PIXEL_CLOCK_MHZ  98.899680f

/*
 * VRAM Memory Limits and Buffer Formats
 * HDMI_PANEL_FB_BPP = 2u: 2 bytes per pixel (RGB565). 
 * HDMI_VRAM_SIZE_BYTES: Physical size of dedicated Video RAM 
 * Auxiliary Buffers: 2D blit engine command buffers pattern caches
 */
#define HDMI_PANEL_FB_BPP        2u
#define HDMI_VRAM_SIZE_BYTES     0x400000u

#define HDMI_VRAM_INSTR_BUF      (64u * 1024u)
#define HDMI_VRAM_PATTERN_BUF    (64u * 64u * 4u)

#endif /* HDMI_PANEL_CONFIG_H_ */
