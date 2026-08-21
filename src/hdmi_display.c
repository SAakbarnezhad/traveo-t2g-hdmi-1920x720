/*******************************************************************************
 * hdmi_display.c
 *
 * HDMI wrapper — hardware side.
 * Copied graphics_support/ from the FPD-Link HDMI basic example (project 00),
 * then wrapped it so gfx_tutorials doesn't need to touch IT6263 registers.
 *
 * hdmi_gfx.c handles CyGfx driver + VRAM window
 ******************************************************************************/

#include <stdio.h>

#include "cy_pdl.h"
#include "cybsp.h"

#include "cy_gfx_env.h"
#include "cy_lvds_hdmi.h"
#include "cy_videoss.h"
#include "platform_irq.h"

#include "hdmi_display.h"
#include "hdmi_gfx.h"


/* Low-Level Configuration Structure configures the IT6263 HDMI transmitter
 * colorBits: Uses 8 bits per color channel (24-bit total color over LVDS). 
 * encodeFormat: Selects VESA bit-packing format rather than JEIDA. 
 * singleOrDual: Drives only FPD-Link 1 
 * videoType: Identifies the incoming video mode  for the HDMI InfoFrame
 *
 * Must match Waveshare panel + what we patched in cy_gfx_env.c
 */
static cy_stc_hdmi_trx_config_t s_hdmiCfg =
{
    .colorBits    = CY_LVDS_LINK_8BIT,
    .encodeFormat = CY_LVDS_FORMAT_VESA,
    .colorFormat  = CY_HDMI_COLOR_RGB444,
    .singleOrDual = CY_LVDS_LINK_SINGLE,
    .videoType    = CY_HDMI_1920x720w60,
};

/* Low-Level Configuration Structure 
 * .bInitDisplay0Ttl = false & .bInitDisplay1Ttl = false: 
 * Keeps parallel TTL display pins disabled, ensuring MCU routing uses the dedicated differential FPD-Link pins.
 *
 * .bInitSwTimer = true: Starts the internal 1 ms software timer used by the IT6263 for delays and timeouts
 *
 * FPD-Link pins yes, TTL/FX3 display pins no
 */

static const cy_gfxenv_stc_cfg_t s_gfxEnvCfg =
{
    .bInitSwTimer     = true,
    .bInitSemihosting = false,
    .pstcInitPortPins = &(cy_gfxenv_stc_init_portpins_t)
    {
      #if (CY_USE_PSVP == 0)
        .bInitDisplay0Ttl = false,
      #else
        .bInitDisplay0Ttl = true,
      #endif
      #if (CY_USE_PSVP == 0) && defined(VIDEOSS0_FPDLINK1)
        .bInitDisplay1Ttl = false,
      #else
        .bInitDisplay1Ttl = true,
      #endif
        .bInitCapture0Ttl       = false,
        .bInitSmif0             = false,
        .bInitSmif1             = false,
        .bInitBacklightDisp0    = false,
        .bInitBacklightDisp1    = false,
        .bInitBacklightFpdLink0 = false,
        .bInitBacklightFpdLink1 = false,
        .bInitButtonGpios       = false,
    },
    .pstcInitExtMem  = NULL,
    .pstcInitButtons = &(cy_gfxenv_stc_init_buttons_t)
    {
        .u8CySwTimerId = (CY_SWTMR_MAX_TIMERS - 1),
        .pfnCallback   = NULL,
    }
};


/* Calls Cy_GfxEnv_EnableHdmiTestImage(), which executes five critical operations: 
 * 1.Configures PLL400M to generate the 98.88 MHz pixel clock on clock path 5. 
 * 2.Sets up TCON1 VESA color bit mapping. 
 * 3.Starts the hardware FPD-Link serializer. 
 * 4.Writes 1920×720 active and 2180×756 total timings to FRAMEGEN1. 
 * 5.Sets FGEN = 1 to start generating HSYNC, VSYNC, and DE pulses
 */
static CYGFX_ERROR applyFpdLinkOutput(void)
{
	if (Cy_GfxEnv_EnableHdmiTestImage(CY_GFXENV_DISP_TYPE_1920_720_60_HDMI_VESA) != CY_GFXENV_SUCCESS)
    {
        printf("  hdmi: EnableHdmiTestImage FAILED\r\n");
        return CYGFX_ERR;
    }
    
    printf("  hdmi: PLL400M + FPD-Link OK\r\n");
    fflush(stdout);
    return CYGFX_OK;
}

/* Called once from main(). Powers up the VIDEOSS peripheral, sets up VideoSS interrupts, 
 * initializes the I2C link to the IT6263 bridge, and allocates framebuffer in VRAM
 */
CYGFX_ERROR hdmi_display_init(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    /* Power to the graphics subsystem */
    Cy_PD_Enable();
	/* Configures MCU port pins and starts the software timer */
    Cy_GfxEnv_Init(&s_gfxEnvCfg);

    /* Provides a 250 ms settling delay for the IT6263 power rails */
    Cy_SysLib_Delay(250u);

    ConfigureVideoSSInterrupt();

    /* Performs hardware reset, verifies IT6263 chip ID over I2C */
    Cy_HDMI_Transceiver_Init(s_hdmiCfg);
	
	/* Initializes the CyGfx driver and applies display properties*/
    ret = hdmi_gfx_open_display();
    if (ret != CYGFX_OK) return ret;
	
	/* Allocates VRAM and creates the hardware surface descriptor*/
    ret = hdmi_gfx_alloc_framebuffer();
    if (ret != CYGFX_OK) return ret;

    return CYGFX_OK;
}

/*
 * Called from gfx.c after the blit engine instruction buffer is allocated. 
 * It maps layer 0 to the framebuffer, starts the PLL pixel clock, enables FPD-Link output, 
 * and blocks until the IT6263 chip locks onto the live LVDS stream
 */
CYGFX_ERROR hdmi_display_create_window(void)
{
    CYGFX_ERROR ret;
    cy_en_hdmi_trx_status_t hdmiRet = CY_HDMI_TRX_ERROR;

	/* Creates the layer 0 window and attaches it to the VRAM framebuffer*/
    ret = hdmi_gfx_create_window();
    if (ret != CYGFX_OK)
    {
        printf("  hdmi: create_window FAILED 0x%08lx\r\n", (unsigned long)ret);
        return ret;
    }
	
    /* Commits the window and display settings so Fetch Layer 0 has a valid surface before scanning begins */
    hdmi_gfx_present();
	
	/* Starts the pixel clock and the FPD-Link LVDS stream */
    ret = applyFpdLinkOutput();
    if (ret != CYGFX_OK) return ret;

    /* IT6263 wants a re-init once video is actually running.
     * Same call as in init()now that LVDS is live, mode detection succeeds */
    Cy_HDMI_Transceiver_Init(s_hdmiCfg);
    printf("  hdmi: waiting for LVDS/HDMI lock (~20 s)...\r\n");
    fflush(stdout);
    /* Polls Cy_HDMI_Transceiver_DeviceLoop() every 500 ms until the IT6263 locks onto the signal 
     * and begins driving HDMI */
    for (uint8_t i = 0; i < 40u; i++)
    {
        hdmiRet = Cy_HDMI_Transceiver_DeviceLoop();
        if (hdmiRet == CY_HDMI_TRX_SUCCESS) break;
        Cy_SysLib_Delay(500u);
    }
    printf("  hdmi: %s\r\n",
           (hdmiRet == CY_HDMI_TRX_SUCCESS) ? "link OK" : "link NOT ready (check cable)");
    fflush(stdout);

    return CYGFX_OK;
}

/* After init: get fb pointer + surface for drawing. */
void hdmi_get_framebuffer(hdmi_fb_info_t* out)
{
    hdmi_gfx_get_framebuffer(out);
}

/* Push finished pixels to the panel (WinCommit + DispCommit). Call after each draw step */
void hdmi_present(void)
{
    hdmi_gfx_present();
}

/* Must be called periodically in the application's background/idle loop. It monitors Hot Plug Detect (HPD)
 * state and services the IT6263 so the monitor connection remains active. 
 */
void hdmi_service(void)
{
    (void)Cy_HDMI_Transceiver_DeviceLoop();
}
