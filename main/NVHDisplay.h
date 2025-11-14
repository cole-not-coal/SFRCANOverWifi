#ifndef SFR_NVHDISPLAY
#define SFR_NVHDISPLAY

#include "sfrtypes.h"
#include "./NVHDisplay/EVE.h"
#include "./NVHDisplay/EVE_supplemental.h"

#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

/* --------------------------- Function prototypes -------------------------- */
void NVHDisplay_init(void);
void initStaticBackground(void);
void TFT_display(void);

#endif // SFR_NVHDISPLAY