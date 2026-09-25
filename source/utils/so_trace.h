#ifndef ZOMBIE_SHOOTER_SO_TRACE_H
#define ZOMBIE_SHOOTER_SO_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <so_util/so_util.h>

extern so_module so_mod;

static inline uintptr_t so_trace_normalize(uintptr_t address) {
    return address & ~(uintptr_t)1;
}

static inline uintptr_t so_trace_image_end(void) {
    uintptr_t end = so_mod.text_base + so_mod.text_size;

    if (so_mod.plt_base + so_mod.plt_size > end)
        end = so_mod.plt_base + so_mod.plt_size;
    if (so_mod.exidx_base + so_mod.exidx_size > end)
        end = so_mod.exidx_base + so_mod.exidx_size;
    for (int i = 0; i < so_mod.n_data; ++i) {
        uintptr_t data_end = so_mod.data_base[i] + so_mod.data_size[i];
        if (data_end > end)
            end = data_end;
    }

    return end;
}

static inline bool so_trace_offset(uintptr_t address, uintptr_t *offset) {
    uintptr_t normalized = so_trace_normalize(address);
    uintptr_t start = so_mod.load_addr;
    uintptr_t end = so_trace_image_end();

    if (!start || normalized < start || normalized >= end)
        return false;
    if (offset)
        *offset = normalized - start;
    return true;
}

static inline bool so_trace_is_code(uintptr_t address) {
    uintptr_t normalized = so_trace_normalize(address);
    return normalized >= so_mod.text_base &&
           normalized < so_mod.text_base + so_mod.text_size;
}

#endif
