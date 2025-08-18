#ifndef VRI_D3D11_DEVICE_H
#define VRI_D3D11_DEVICE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11Device5        *p_device;
    ID3D11DeviceContext4 *p_immediate_context;
    IDXGIAdapter         *p_adapter;
} VriD3D11Device;

#endif
