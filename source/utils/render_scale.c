/* MIT. Run #16 recovery A/B: keep the renderer's original Android work-size
 * calculation untouched.  The previous branch installed a hook that capped
 * scale_config::factor() to 0.9 (864x489 on Vita); the physical run crashed
 * immediately after the first software-surface upload.  Disable only that
 * hook so all other current master fixes/optimizations remain testable. */
#include "utils/logger.h"

void render_scale_install(void) {
    l_perf("work_resolution installed=0 reason=run16_recovery_ab original_engine_scale=1");
}
