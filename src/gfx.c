/*******************************************************************************
 * gfx.c
 * Surface Blit tutorial (Infineon gfx_tutorials) — drawing steps unchanged.
 *
 * What we changed for HDMI:
 *   - no local 800x480 display init (hdmi module owns 1920x720 fb)
 *   - draw into hdmi_get_framebuffer(), show with hdmi_present()
 *   - create_window happens inside initBlitContext 
 ******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cygfx_driver_api.h"
#include "cygfx_sysinit.h"
#include "cygfx_window.h"
#include "sm_util.h"
#include "ut_compatibility.h"
#include "ut_memman.h"
#include <string.h>
#include <stdio.h>

#include "gfx.h"
#include "bitmaps.h"
#include "hdmi_display.h"

#define INSTR_BUF_SIZE  (64u * 1024u)
#define PATTERN_W       (64u)
#define PATTERN_H       (64u)
#define RGB565_BLUE     (0x001Fu)

static CYGFX_SURFACE_OBJECT_S* s_pStore = NULL;
static CYGFX_U32               s_dispW  = 0;
static CYGFX_U32               s_dispH  = 0;
static CYGFX_SURFACE_OBJECT_S  s_surfSrc;

static CYGFX_BE_CONTEXT_OBJECT_S s_ctxObj;
static CYGFX_BE_CONTEXT          s_ctx = &s_ctxObj;

static void *s_pInstrBuf   = NULL;
static void *s_pPatternBuf = NULL;

static CYGFX_ERROR  step_fillBlue(void);
static CYGFX_ERROR  step_copyBwImage(void);
static CYGFX_ERROR  step_copyColorPattern(void);
static CYGFX_ERROR  step_blendHelloWorld(void);
static CYGFX_ERROR  step_display(void);
static void         createColorPattern(CYGFX_U32 addr, CYGFX_U32 w, CYGFX_U32 h);

static CYGFX_ERROR  initBlitContext(void);
static void         cpu_fillBlue(hdmi_fb_info_t* fb);

CYGFX_ERROR initGfx(void)
{
    CYGFX_ERROR    ret = CYGFX_OK;
    hdmi_fb_info_t fb;

    /* Everything about the display comes from here. 
     * Change the panel, or shrink the window to a centred region in wrapper
     */
    hdmi_get_framebuffer(&fb);
    s_pStore = fb.surface;
    s_dispW  = fb.width;
    s_dispH  = fb.height;

    UTIL_SUCCESS(ret, initBlitContext());

    cpu_fillBlue(&fb);
    hdmi_present();
    printf("CPU blue fill presented (panel should be blue now)\r\n");
    fflush(stdout);

    UTIL_SUCCESS(ret, step_fillBlue());
    UTIL_SUCCESS(ret, step_copyBwImage());
    UTIL_SUCCESS(ret, step_copyColorPattern());
    UTIL_SUCCESS(ret, step_blendHelloWorld());
    UTIL_SUCCESS(ret, step_display());
    return ret;
}

void deInitGfx(void)
{
    if (s_pInstrBuf)   { utVideoFree(s_pInstrBuf);   s_pInstrBuf   = NULL; }
    if (s_pPatternBuf) { utVideoFree(s_pPatternBuf); s_pPatternBuf = NULL; }
}

static void cpu_fillBlue(hdmi_fb_info_t* fb)
{
    uint16_t* px = (uint16_t*)fb->base;
    uint32_t  n  = fb->width * fb->height;
    uint32_t  i;
    for (i = 0; i < n; i++)
    {
        px[i] = RGB565_BLUE;
    }
}

static CYGFX_ERROR initBlitContext(void)
{
    CYGFX_ERROR ret      = CYGFX_OK;
    CYGFX_ADDR  physAddr = 0;

    s_pInstrBuf = utVideoAlloc(INSTR_BUF_SIZE, 32u, &physAddr);
    if (s_pInstrBuf == NULL) {
        printf("ERROR: utVideoAlloc instruction buffer failed\r\n");
        return CYGFX_ERR;
    }
    s_pPatternBuf = utVideoAlloc(PATTERN_W * PATTERN_H * 4u, 32u, &physAddr);
    if (s_pPatternBuf == NULL) {
        printf("ERROR: utVideoAlloc pattern buffer failed\r\n");
        return CYGFX_ERR;
    }

    /*
     * Order that finally worked on hardware:
     *   1) blit instruction buffer
     *   2) hdmi_display_create_window()  — FPD-Link + HDMI lock happen here
     *   3) blit context bound to HDMI framebuffer
     * Other orders → black screen or BeFinish hang.
     *
     * (1) before (2) because BeSetTaskInstructionBuffer has to be set on the
     * task before any context is reset against it.
     * (2) before (3) because binding a STORE surface into a display pipeline
     * that hasn't been committed yet is what makes BeFinish() never return.
     * Full reasoning is in the header comment of hdmi_display_create_window().
     *
     */
    UTIL_SUCCESS(ret, CyGfx_BeSetTaskInstructionBuffer(
        CYGFX_BE_TASK_MEM_PRIO_1, s_pInstrBuf, INSTR_BUF_SIZE));

    UTIL_SUCCESS(ret, hdmi_display_create_window());

    UTIL_SUCCESS(ret, CyGfx_BeResetContext(s_ctx));
    UTIL_SUCCESS(ret, CyGfx_BeSetAttribute(s_ctx,
                                           CYGFX_BE_CTX_ATTR_ZERO_POINT,
                                           CYGFX_BE_ATTR_ZERO_TOP_LEFT));
    UTIL_SUCCESS(ret, CyGfx_BeBindSurface(s_ctx,
                                          CYGFX_BE_TARGET_STORE,
                                          s_pStore));
    printf("Blit context ready (target %ux%u)\r\n",
           (unsigned)s_dispW, (unsigned)s_dispH);
    fflush(stdout);
    return ret;
}

static CYGFX_ERROR step_fillBlue(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    UTIL_SUCCESS(ret, CyGfx_BeSetAttribute(s_ctx,
                                            CYGFX_BE_CTX_ATTR_COLOR,
                                            CYGFX_SM_COLOR_TO_RGBA(0, 0, 255, 255)));

    UTIL_SUCCESS(ret, CyGfx_BeFill(s_ctx, 0, 0, s_dispW, s_dispH));

    printf("Step 1: fill blue queued\r\n");
    return ret;
}

static CYGFX_ERROR step_copyBwImage(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    UTIL_SUCCESS(ret, CyGfx_SmResetSurfaceObject(&s_surfSrc));
    UTIL_SUCCESS(ret, CyGfx_SmAssignBuffer(
        &s_surfSrc,
        32u, 32u,
        CYGFX_SM_FORMAT_RGB1,
        NULL,
        0u));

    UTIL_SUCCESS(ret, CyGfx_SmSetAttribute(&s_surfSrc,
                                            CYGFX_SM_ATTR_VIRT_ADDRESS,
                                            (CYGFX_U32)s_bwImage));

    UTIL_SUCCESS(ret, CyGfx_BeBindSurface(s_ctx,
                                           CYGFX_BE_TARGET_SRC,
                                           &s_surfSrc));

    UTIL_SUCCESS(ret, CyGfx_BeBlt(s_ctx, 20.0f, 20.0f));

    printf("Step 2: B/W image blit queued at (20,20)\r\n");
    return ret;
}

static void createColorPattern(CYGFX_U32 addr, CYGFX_U32 w, CYGFX_U32 h)
{
    for (CYGFX_U32 x = 0; x < w; x++) {
        for (CYGFX_U32 y = 0; y < h; y++) {
            CYGFX_U32 red   = 255u - (2u * x);
            CYGFX_U32 green = y * 4u;
            CYGFX_U32 blue  = 0u;
            CYGFX_U32 alpha = 255u;
            *((CYGFX_U32 *)(addr + (4u * ((y * w) + x)))) =
                (red << 24u) | (green << 16u) | (blue << 8u) | alpha;
        }
    }
}

static CYGFX_ERROR step_copyColorPattern(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    createColorPattern((CYGFX_U32)(uintptr_t)s_pPatternBuf, PATTERN_W, PATTERN_H);

    CyGfx_SmResetSurfaceObject(&s_surfSrc);
    UTIL_SUCCESS(ret, CyGfx_SmAssignBuffer(
        &s_surfSrc,
        PATTERN_W, PATTERN_H,
        CYGFX_SM_FORMAT_R8G8B8A8,
        s_pPatternBuf,
        0u));

    UTIL_SUCCESS(ret, CyGfx_BeBindSurface(s_ctx,
                                           CYGFX_BE_TARGET_SRC,
                                           &s_surfSrc));

    UTIL_SUCCESS(ret, CyGfx_BeBlt(s_ctx, 35.0f, 45.0f));

    printf("Step 3: color pattern blit queued at (35,45)\r\n");
    return ret;
}

static CYGFX_ERROR step_blendHelloWorld(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    UTIL_SUCCESS(ret, CyGfx_SmResetSurfaceObject(&s_surfSrc));
    UTIL_SUCCESS(ret, CyGfx_SmAssignBuffer(
        &s_surfSrc,
        176u, 32u,
        CYGFX_SM_FORMAT_A8,
        NULL,
        0u));

    UTIL_SUCCESS(ret, CyGfx_SmSetAttribute(&s_surfSrc,
                                            CYGFX_SM_ATTR_VIRT_ADDRESS,
                                            (CYGFX_U32)s_helloWorld));

    UTIL_SUCCESS(ret, CyGfx_BeSetSurfAttribute(s_ctx,
                                                CYGFX_BE_TARGET_SRC,
                                                CYGFX_BE_SURF_ATTR_COLOR,
                                                CYGFX_SM_COLOR_TO_RGBA(255, 0, 0, 255)));

    /* STORE | DST is the important bit here, and it's easy to miss.
     *   STORE = where results get written
     *   DST   = the background that gets READ for blending
     * Binding the framebuffer as both means "read what's already there, blend
     * the red text over it using the A8 coverage, write back in place".
     * Drop the | DST and you get red text sitting on a black box. */
    UTIL_SUCCESS(ret, CyGfx_BeBindSurface(s_ctx,
                                            CYGFX_BE_TARGET_STORE | CYGFX_BE_TARGET_DST,
                                            s_pStore));

    UTIL_SUCCESS(ret, CyGfx_BeBlt(s_ctx, 50.0f, 70.0f));

    printf("Step 4: Hello World blend queued at (50,70)\r\n");
    return ret;
}

static CYGFX_ERROR step_display(void)
{
    CYGFX_ERROR ret = CYGFX_OK;

    /* Steps 1-4 only QUEUED instructions into the 64 KB buffer in VRAM; nothing
     * has been drawn yet. BeFinish kicks the engine and blocks until the queue
     * drains, which it does by sleeping on the GFX2D completion interrupt.
     *
     * If you ever see the boot log stop on the line below, it's not a display
     * problem — it's the interrupt setup in platform_irq.c. */
    printf("Step 5: waiting for Blit Engine (BeFinish)...\r\n");
    fflush(stdout);
    UTIL_SUCCESS(ret, CyGfx_BeFinish(s_ctx));
    hdmi_present();

    return ret;
}
