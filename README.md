# TRAVEO T2G — native HDMI output at 1920x720 for the CyGfx graphics stack

Infineon's graphics code examples for the TRAVEO&trade; T2G cluster kit send their
framebuffer to a PC through the Virtual Display Tool over USB. For our team project we
needed the same examples on a real panel, so I replaced that path with a native HDMI
bring-up on the kit itself:

```
CM7 / CyGfx blit engine
        |
   VRAM framebuffer (1920x720, RGB565)
        |
   VIDEOSS display controller 1
        |
   FPD-Link (single link, VESA, 8-bit)
        |
   IT6263 LVDS-to-HDMI bridge  <-- I2C configuration
        |
   Waveshare 12.3" 1920x720 @ 60 Hz
```

After this, the Infineon "Surface Blit" tutorial draws on the physical panel with no
change to its drawing steps.

## Hardware and tools

| | |
|---|---|
| MCU board | KIT_T2G_C-2D-6M_LITE (CYT4DNJBZ, Cortex-M7) |
| Panel | Waveshare 12.3", 1920x720, 60 Hz, HDMI |
| Bridge | ITE IT6263, LVDS to HDMI, configured over I2C |
| Toolchain | ModusToolbox 3.8, GCC Arm 14.2, J-Link 8.74 |
| Middleware | CyGfx graphics driver (tviic2d-gfx-mw), retarget-io |

## What is in this repository

Only the files I wrote or modified. Infineon's BSP, the CyGfx middleware and the
`graphics_support/` low-level drivers are covered by the Cypress/Infineon EULA and are
not redistributed here — see [How to build](#how-to-build).

| File | Role |
|---|---|
| `src/hdmi_panel_config.h` | Every timing number for the panel in one place: geometry, both blanking conventions, pixel clock, VRAM budget |
| `src/hdmi_display.h/.c` | Hardware side: VIDEOSS power-up, GPIO and I2C to the IT6263, FPD-Link enable, PLL, hot-plug service |
| `src/hdmi_gfx.h/.c` | CyGfx side: open display controller 1 with our timing, allocate the VRAM framebuffer, create the layer-0 window, commit frames |
| `src/gfx.c`, `src/gfx.h` | Infineon's Surface Blit tutorial, redirected to the HDMI framebuffer. Drawing steps unchanged |
| `src/main.c` | Console init, `hdmi_display_init()`, `initGfx()`, then a loop that services hot-plug detect |

Call order, which is not optional:

```c
hdmi_display_init();          /* VIDEOSS, I2C, IT6263 awake, framebuffer allocated */
initGfx();                    /* allocates the blit instruction buffer, then calls
                                 hdmi_display_create_window() and draws */
for (;;) { hdmi_service(); }  /* HPD + IT6263 keepalive */
```

## Three things worth explaining

**Why the wrapper is split across two files.** Infineon's BSP headers
(`cy_gfx_env.h`, `cy_fpdlink.h`) and the graphics middleware headers
(`ut_display.h`, `cygfx_display_api.h`) declare conflicting enums for FPD-Link
channels and panel types, plus clashing LVDS macros. Including both in one
translation unit does not compile. So `hdmi_display.c` owns the BSP side,
`hdmi_gfx.c` owns the middleware side, and `hdmi_gfx.h` is the narrow bridge
between them using only CyGfx types.

**Why the timings appear twice.** The IT6263 wants classic front porch / sync /
back porch. The CyGfx display API does not take front porch at all — it takes the
sync width and the combined sync + back porch (HSBP/VSBP). Both are derived in
`hdmi_panel_config.h` from the same totals (2180 x 756 at 60 Hz, so a 98.9 MHz
pixel clock), so there is one place to edit for a different panel.

**Why the window is created late.** The layer-0 window can only be bound after the
blit engine's instruction buffer exists, and the IT6263 only locks once a live LVDS
stream is present. `hdmi_display_create_window()` is therefore called from inside
`initGfx()`, not from `main()`, and it blocks until the bridge reports lock.

## How to build

1. Create the *Graphics Empty Template* code example for `KIT_T2G_C-2D-6M_LITE` in
   ModusToolbox 3.8 and build it once, so the BSP and the `tviic2d-gfx-mw` middleware
   are downloaded.
2. Copy `graphics_support/` from Infineon's FPD-Link HDMI basic example into the
   project. That directory holds `cy_base_hdmi.*`, `cy_fpdlink.*`, `cy_gfx_env.*`,
   `cy_i2c_hdmi.*`, `cy_lvds_hdmi.*`, `cy_it6263_config.h` and `cy_videoss.h`.
3. Copy the files from `src/` here into the project root, overwriting `main.c` and
   `gfx.c`.
4. Build and program over J-Link. The debug UART prints the init sequence; the panel
   shows the tutorial's blue background, the bitmap copies and the blended text.

## Status

Works: 1920x720 at 60 Hz over HDMI, RGB565 single framebuffer, hot-plug handling,
the full Surface Blit tutorial rendering on the panel.

Not done here: double buffering, LVGL on top of this layer (that is the next stage of
the team project), and dual-link FPD for resolutions above this one.

## Context

Built as part of a Master's team project at Hochschule Darmstadt (Embedded and
Microelectronics), where this HDMI layer is the display foundation for a
hardware-accelerated LVGL v9 port on TRAVEO T2G.
