/*******************************************************************************
 * gfx.h
 * Tutorial 01 — Surface Blit Display Basic
 * TRAVEO T2G (CYT4DNJBZ) · KIT-T2G-C-2D-6M-LITE · HDMI Display 1920×720 RGB565 
 *******************************************************************************/
#ifndef GFX_H_
#define GFX_H_

#include "cygfx_driver_api.h"

CYGFX_ERROR initGfx(void);
void        deInitGfx(void);

#endif /* GFX_H_ */
