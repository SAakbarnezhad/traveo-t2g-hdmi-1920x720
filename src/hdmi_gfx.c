/*******************************************************************************
 * file hdmi_gfx.c
 * Display driver configuration and VRAM framebuffer management.
 *
 * Implements the CyGfx graphics stack interface:
 *   - Configures display timing and opens the video controller (Display 1 / FPD-Link).
 *   - Allocates the physical RGB565 framebuffer in VRAM and prepares the surface.
 *   - Configures Layer 0 display window properties and binds the render surface.
 *   - Latches window/display configuration changes during vertical blanking.
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>

#include "cy_pdl.h"
#include "cybsp.h"

#include "cygfx_driver_api.h"
#include "cygfx_sysinit.h"
#include "cygfx_window.h"
#include "sm_util.h"
#include "ut_compatibility.h"
#include "ut_disp.h"
#include "ut_disp_panels.h"
#include "ut_memman.h"

#include "hdmi_gfx.h"
#include "hdmi_panel_config.h"

/* Handle to the active display controller (Controller 1 / FPD-Link) */
static CYGFX_DISP             s_disp   = NULL;
/* used to control the base full-screen canvas (Layer 0) configured inside the chip's display*/
static CYGFX_WINDOW           s_window = NULL;
/* The CyGfx surface descriptor (stores geometry, pitch, pixel format and base address) */
static CYGFX_SURFACE_OBJECT_S s_surfFb;
/* The physical pointer to the  framebuffer in VRAM */
static void*                  s_fbBase = NULL;
/* Stores the hardware clock divider computed during PLL initialization*/
static CYGFX_U08              s_fpdClkDiv = 0;

/* Opening the Controller
 * 	- Pulls Timings from hdmi_panel_config.h:
 *		Loads active resolution, totals, pixel clock, sync pulses, and back-porch timing.
 *	- Selects Display Controller 1:
 * 		Sets .outputController = CYGFX_DISP_CONTROLLER_1 
 *	- Calculates PLL Divider:
 *		Calls utDispGetPll() to calculate the divider ratio needed to generate the  pixel clock from the PLL
 *  - Initializes Driver & VRAM Allocator:
 *		CyGfx_SysInitializeDriver starts the internal graphics driver
 *   	utMmanReset() resets the VRAM bump allocator to ensure clean allocations
 *	- Opens Display Engine:
 * 		CyGfx_DispOpenDisplay programs the hardware timing registers and initializes the controller
 *
 */
CYGFX_ERROR hdmi_gfx_open_display(void)
{
    CYGFX_ERROR             ret       = CYGFX_OK;
    CYGFX_SYSINIT_INFO_S    initInfo  = CYGFX_SYS_INIT_INITIALIZER;
    CYGFX_DISP_PROPERTIES_S dispProps = CYGFX_DISP_PROPERTIES_INITIALIZER;
    CYGFX_U08               clockDivider;
    dispProps                  = s_panel_XXX_1920_720;
    dispProps.pTconProps       = NULL;  
    dispProps.countTconProps   = 0;
    dispProps.outputController = CYGFX_DISP_CONTROLLER_1;
    dispProps.timing.pixelClock = HDMI_PANEL_PIXEL_CLOCK_MHZ;
    dispProps.timing.Hact  = HDMI_PANEL_HACT;
    dispProps.timing.Vact  = HDMI_PANEL_VACT;
    dispProps.timing.Htot  = HDMI_PANEL_HTOTAL;
    dispProps.timing.Vtot  = HDMI_PANEL_VTOTAL;
    dispProps.timing.Hsbp  = HDMI_PANEL_CYGFX_HSBP;
    dispProps.timing.Hsync = HDMI_PANEL_CYGFX_HSYNC;
    dispProps.timing.Vsbp  = HDMI_PANEL_CYGFX_VSBP;
    dispProps.timing.Vsync = HDMI_PANEL_CYGFX_VSYNC;

    UTIL_SUCCESS(ret, utDispGetPll(dispProps.timing.pixelClock,
                                   dispProps.displayMode,
                                   &clockDivider, &initInfo.PllDsp1));
    s_fpdClkDiv = clockDivider; 
    UTIL_SUCCESS(ret, CyGfx_SysInitializeDriver(&initInfo));
    UTIL_SUCCESS(ret, utMmanReset());     
    UTIL_SUCCESS(ret, CyGfx_DispOpenDisplay(&dispProps, &s_disp, NULL)); 

    printf("  hdmi: display opened %ux%u @ %u.%02u MHz (PLL %lu Hz, div reg %u)\r\n",
           (unsigned)HDMI_PANEL_HACT, (unsigned)HDMI_PANEL_VACT,
           (unsigned)(HDMI_PANEL_PIXEL_CLOCK_HZ / 1000000UL),
           (unsigned)((HDMI_PANEL_PIXEL_CLOCK_HZ / 10000UL) % 100UL),
           (unsigned long)initInfo.PllDsp1, (unsigned)clockDivider);
    fflush(stdout);
    return ret;
}


/* VRAM Memory Allocation*/
CYGFX_ERROR hdmi_gfx_alloc_framebuffer(void)
{
    CYGFX_ERROR ret      = CYGFX_OK;
    CYGFX_ADDR  physAddr = 0;
    CYGFX_U32   fbSize   = HDMI_FB_WIDTH * HDMI_FB_HEIGHT * HDMI_FB_BPP;

    s_fbBase = utVideoAlloc(fbSize, 32u, &physAddr);
    if (s_fbBase == NULL)
    {
        printf("  hdmi: utVideoAlloc(%lu) FAILED - not enough VRAM\r\n",
               (unsigned long)fbSize);
        return CYGFX_ERR;
    }
    memset(s_fbBase, 0, fbSize);
    UTIL_SUCCESS(ret, CyGfx_SmResetSurfaceObject(&s_surfFb));
    UTIL_SUCCESS(ret, CyGfx_SmAssignBuffer(&s_surfFb,
                                           HDMI_FB_WIDTH, HDMI_FB_HEIGHT,
                                           HDMI_FB_FORMAT, s_fbBase, 0u));

    printf("  hdmi: framebuffer %ux%u RGB565 @ 0x%08x (%lu bytes)\r\n",
           (unsigned)HDMI_FB_WIDTH, (unsigned)HDMI_FB_HEIGHT,
           (unsigned)(uintptr_t)s_fbBase, (unsigned long)fbSize);
    fflush(stdout);
    return ret;
}

/*
 * Creating Layer 0
 */
CYGFX_ERROR hdmi_gfx_create_window(void)
{
    CYGFX_ERROR ret = CYGFX_OK;
    CYGFX_DISP_WINDOW_PROPERTIES_S winProps = CYGFX_DISP_WINDOW_PROPERTIES_INITIALIZER;
	
    if ((HDMI_FB_WIDTH > HDMI_PANEL_HACT) || (HDMI_FB_HEIGHT > HDMI_PANEL_VACT))
    {
        printf("ERROR: Framebuffer exceeds panel size!\r\n");
        return CYGFX_ERR;
    }

    winProps.topLeftX = HDMI_WIN_X; 
    winProps.topLeftY = HDMI_WIN_Y;  
    winProps.width    = HDMI_FB_WIDTH;
    winProps.height   = HDMI_FB_HEIGHT;
    winProps.layerId  = CYGFX_DISP_LAYER_0;  
    winProps.features = 0; 

    UTIL_SUCCESS(ret, CyGfx_DispWinCreate(s_disp, &winProps, &s_window));
    UTIL_SUCCESS(ret, CyGfx_WinSetSurface(s_window,
                                          CYGFX_DISP_BUFF_TARGET_COLOR_BUFF,
                                          &s_surfFb));
    UTIL_SUCCESS(ret, CyGfx_WinCommit(s_window));

    UTIL_SUCCESS(ret, CyGfx_DispCommit(s_disp));
    printf("  hdmi: window + DispCommit OK (pipeline applied)\r\n");
    fflush(stdout);
    return ret;
}

/*
 * Fill the caller's structure with framebuffer details.
 * Returns: base address, dimensions, row pitch, pixel format and surface handle
 */
void hdmi_gfx_get_framebuffer(hdmi_fb_info_t* out)
{
    if (out == NULL) return;
    out->base        = s_fbBase;
    out->width       = HDMI_FB_WIDTH;
    out->height      = HDMI_FB_HEIGHT;
    out->strideBytes = HDMI_FB_WIDTH * HDMI_FB_BPP;
    out->format      = HDMI_FB_FORMAT;
    out->surface     = &s_surfFb;
}

/*
 * Push any pending window and display changes to hardware
 */
void hdmi_gfx_present(void)
{
    if (s_window == NULL) return;
    (void)CyGfx_WinCommit(s_window);
    (void)CyGfx_DispCommit(s_disp);
}
