#ifndef VRI_D3D11_SWAPCHAIN_H
#define VRI_D3D11_SWAPCHAIN_H

#include "vri_d3d11_common.h"

typedef struct {
    IDXGISwapChain4 *p_swapchain;
    IDXGIFactory2   *p_factory2;
    void            *p_waitable_object;
    uint32_t         flags;
    void            *p_hwnd;
    uint64_t         present_id;
    VriTexture       texture;
} VriD3D11Swapchain;

void      d3d11_register_swapchain_functions(VriDeviceDispatchTable *table);
VriResult d3d11_swapchain_present(VriSwapchain swapchain, uint32_t image_index);

#endif
