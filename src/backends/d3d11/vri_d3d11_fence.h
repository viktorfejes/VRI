#ifndef VRI_D3D11_FENCE_H
#define VRI_D3D11_FENCE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11Fence *p_fence;
    HANDLE       event;
} VriD3D11Fence;

void      d3d11_register_fence_functions(VriDeviceDispatchTable *table);
VriResult d3d11_fences_wait(VriDevice device, const VriFence *p_fences, const uint64_t *p_values, uint32_t fence_count, VriBool wait_all, uint64_t timeout_ns);
VriResult d3d11_fence_signal(VriFence fence, uint64_t value);

#endif
