#ifndef VRI_D3D11_FENCE_H
#define VRI_D3D11_FENCE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11Fence *p_fence;
    HANDLE       event;
} VriD3D11Fence;

void d3d11_register_fence_functions(VriDeviceDispatchTable *table);

#endif
