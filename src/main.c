/*******************************************************************************
 * main.c
 * Surface Blit application for TRAVEO T2G with 1920x720 HDMI output.
 *
 * Replaces legacy FX3 USB virtual display with native HDMI initialization:
 *   - hdmi_display_init() configures VIDEOSS and the IT6263 bridge.
 *   - hdmi_service() runs in the main loop for HPD / link keepalive.
 *
 * Target: KIT-T2G-C-2D-6M-LITE + Waveshare 12.3" (1920x720 @ 60Hz, RGB565)
 ******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "mtb_hal.h"

#include "gfx.h"
#include "hdmi_display.h"

static cy_stc_scb_uart_context_t UART_context;
static mtb_hal_uart_t            UART_hal_obj;

int main(void)
{
    cy_rslt_t   result;
    CYGFX_ERROR gfxErr = CYGFX_OK;
	/* Starts the board clocks, power management units, and low-level system hardware */
    result = cybsp_init();
    CY_ASSERT(result == CY_RSLT_SUCCESS);

	/* Global Interrupt Enable */
    __enable_irq();
	
	/* Debug UART Console Setup */
    result = (cy_rslt_t)Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    Cy_SCB_UART_Enable(UART_HW);
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config, &UART_context, NULL);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    result = cy_retarget_io_init(&UART_hal_obj);
    CY_ASSERT(result == CY_RSLT_SUCCESS);
    printf("\x1b[2J\x1b[;H");
    printf("HDMI Display - blit tutorial 1920x720\r\n");
    printf("==========================================\r\n");
	
	/* Disabling CPU Data Cache */
    SCB_DisableDCache();

    /* power up VIDEOSS, allocate VRAM framebuffer canvas, 
	 * configure the display driver timing, and wake up the IT6263 chip
	 */
    if (hdmi_display_init() != CYGFX_OK)
    {
        printf("ERROR: hdmi_display_init() failed\r\n");
        CY_ASSERT(0);
    }
	
	/* binds the display window, turns on the PLL clock, locks the HDMI output,
	 * renders the blue background, blits the graphics, and commits the frame to the panel
	 */
    gfxErr = initGfx();

    if (gfxErr != CYGFX_OK) {
        printf("ERROR: initGfx() failed: 0x%08x\r\n", (unsigned int)gfxErr);
        CY_ASSERT(0);
    }

    printf("Done. image should be on the HDMI panel.\r\n");

	/**********************************************************************************/
    for (;;) {
		/* It checks the HDMI cable connection (Hot-Plug Detect) and periodically refreshes 
		 * the IT6263 video output state
		 */
        hdmi_service(); 
        Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);  
        Cy_SysLib_Delay(500u);
    }
}
