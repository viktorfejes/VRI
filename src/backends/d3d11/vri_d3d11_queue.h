#ifndef VRI_D3D11_QUEUE_H
#define VRI_D3D11_QUEUE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11DeviceContext4 *p_immediate_context;
} VriD3D11Queue;

#endif
