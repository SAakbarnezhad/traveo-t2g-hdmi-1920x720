# TRAVEO T2G, native HDMI output at 1920x720 for the CyGfx graphics stack

Infineon's graphics code examples for the TRAVEO T2G cluster kit send the framebuffer
to a PC over USB with the Virtual Display Tool. For our team project we needed those
examples running on a real panel, so I replaced the virtual display with a native HDMI
path on the kit itself.

```
CM7 / CyGfx blit engine
        |
   VRAM framebuffer (1920x720, RGB565)
        |
   VIDEOSS display controller 1
        |
   FPD-Link (single link, VESA, 8 bit)
        |
   IT6263 LVDS to HDMI bridge, configured over I2C
        |
   Waveshare 12.3" 1920x720 60 Hz
```

The Infineon "Surface Blit" tutorial then draws on the physical panel with no change to
its drawing steps.

![The Waveshare panel running the Surface Blit tutorial over HDMI: blue background, the
black and white bitmap copy, the colour pattern block and the blended Hello World text](panel.jpeg)

## Hardware and tools

| | |
|---|---|
| MCU board | KIT_T2G_C-2D-6M_LITE (CYT4DNJBZ, Cortex-M7) |
| Panel | Waveshare 12.3", 1920x720, 60 Hz, HDMI |
| Bridge | ITE IT6263, LVDS to HDMI, configured over I2C |
| Toolchain | ModusToolbox 3.8, GCC Arm 14.2, J-Link 8.74 |
| Middleware | CyGfx graphics driver (tviic2d-gfx-mw), retarget-io |

## Files

This repository has only the files I wrote. Infineon's BSP, the CyGfx middleware and
the `graphics_support/` drivers are covered by the Cypress and Infineon license and are
not included here. See the build section below.

| File | Role |
|---|---|
| `src/hdmi_panel_config.h` | All panel numbers in one place: geometry, both blanking conventions, pixel clock, VRAM budget |
| `src/hdmi_display.h/.c` | Hardware side. VIDEOSS power up, GPIO and I2C to the IT6263, FPD-Link enable, PLL, hot plug service |
| `src/hdmi_gfx.h/.c` | CyGfx side. Opens display controller 1 with our timing, allocates the VRAM framebuffer, creates the layer 0 window, commits frames |
| `src/gfx.c`, `src/gfx.h` | Infineon's Surface Blit tutorial, redirected to the HDMI framebuffer. Drawing steps unchanged |
| `src/main.c` | Console init, `hdmi_display_init()`, `initGfx()`, then a loop that services hot plug detect |

The call order is fixed:

```c
hdmi_display_init();          /* VIDEOSS, I2C, IT6263 awake, framebuffer allocated */
initGfx();                    /* allocates the blit instruction buffer, then calls
                                 hdmi_display_create_window() and draws */
for (;;) { hdmi_service(); }  /* HPD and IT6263 keepalive */
```

## Design notes

**The wrapper is split over two files.** Infineon's BSP headers (`cy_gfx_env.h`,
`cy_fpdlink.h`) and the graphics middleware headers (`ut_display.h`,
`cygfx_display_api.h`) declare conflicting enums for FPD-Link channels and panel types,
and conflicting LVDS macros. Including both in one translation unit does not compile.
`hdmi_display.c` uses the BSP side, `hdmi_gfx.c` uses the middleware side, and
`hdmi_gfx.h` connects them using only CyGfx types.

**The timings are defined twice.** The IT6263 needs front porch, sync width and back
porch. The CyGfx display API does not take a front porch. It takes the sync width and
the combined sync plus back porch (HSBP and VSBP). Both sets are derived in
`hdmi_panel_config.h` from the same totals, 2180 x 756 at 60 Hz, giving a 98.9 MHz
pixel clock. To change the panel, only that header has to change.

**The window is created late.** The layer 0 window can only be bound after the blit
engine instruction buffer exists, and the IT6263 only locks when a live LVDS stream is
present. `hdmi_display_create_window()` is therefore called inside `initGfx()` and not
from `main()`, and it waits until the bridge reports lock.

## Build

1. Create the Graphics Empty Template code example for `KIT_T2G_C-2D-6M_LITE` in
   ModusToolbox 3.8 and build it once, so the BSP and the `tviic2d-gfx-mw` middleware
   are downloaded.
2. Copy `graphics_support/` from Infineon's FPD-Link HDMI basic example into the
   project. It contains `cy_base_hdmi.*`, `cy_fpdlink.*`, `cy_gfx_env.*`,
   `cy_i2c_hdmi.*`, `cy_lvds_hdmi.*`, `cy_it6263_config.h` and `cy_videoss.h`.
3. Copy the files from `src/` into the project root, replacing `main.c` and `gfx.c`.
4. Build and program over J-Link. The debug UART prints the init steps and the panel
   shows the tutorial output.

## Status

Working: 1920x720 at 60 Hz over HDMI, RGB565 single framebuffer, hot plug handling, and
the complete Surface Blit tutorial on the panel.

Not implemented here: double buffering, LVGL on top of this layer, which is the next
stage of the team project, and dual link FPD for higher resolutions.

## Context

Master's team project at Hochschule Darmstadt, Embedded and Microelectronics. This HDMI
layer is the display base for a hardware accelerated LVGL v9 port on TRAVEO T2G.
