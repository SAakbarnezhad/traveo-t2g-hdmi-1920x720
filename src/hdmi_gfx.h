/*******************************************************************************
 * file hdmi_gfx.h
 * Internal interface connecting the HDMI wrapper to the CyGfx driver.
 *
 * The HDMI implementation is split across two files to resolve a fundamental
 * type and macro collision between Infineon's BSP layer and its graphics
 * middleware:
 *   - Board Support Layer (BSP):     cy_gfx_env.h, cy_fpdlink.h
 *   - Graphics Middleware Layer:     ut_display.h, cygfx_display_api.h
 *
 * These layers define conflicting enums for FPD-Link channels (cy_en_fpdlink_id_t
 * vs. CYGFX_DISP_FPDLINK_ID_E), display panel types, and LVDS formatting macros,
 * causing redefinition errors when both sets of headers are included in the
 * same unit.
 *
 * Separation of concerns:
 *   - hdmi_display.c: Hardware control (power rails, GPIOs, I2C, PLL, IT6263).
 *   - hdmi_gfx.c:     Graphics stack (VRAM buffers, surfaces, windows, timings).
 *   - hdmi_gfx.h:     Internal link between both files using shared CyGfx types
 ******************************************************************************/
#ifndef HDMI_GFX_H_
#define HDMI_GFX_H_

#include "cygfx_driver_api.h"
#include "hdmi_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called by hdmi_display_init() and configures and opens the physical display controller channel 
 * (Display 1 / FPD-Link) in the CyGfx driver using our 1920×720 60 Hz timing properties
 */
CYGFX_ERROR hdmi_gfx_open_display(void);

/* Called by hdmi_display_init() allocates memory block in VRAM 
 * and creates the CYGFX_SURFACE_OBJECT_S descriptor
 */
CYGFX_ERROR hdmi_gfx_alloc_framebuffer(void);

/* Called by hdmi_display_create_window() creates the layer 0 display window, sets its dimensions 
 * and coordinates ((0,0)), and binds the VRAM surface to Fetch Engine 0
 */
CYGFX_ERROR hdmi_gfx_create_window(void);

/* 	Fills the hdmi_fb_info_t struct */
void hdmi_gfx_get_framebuffer(hdmi_fb_info_t* out);

/* Push window and display configs to hardware during vertical blanking*/
void hdmi_gfx_present(void);

#ifdef __cplusplus
}
#endif

#endif /* HDMI_GFX_H_ */
