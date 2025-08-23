#ifndef VRI_D3D11_QUEUE_H
#define VRI_D3D11_QUEUE_H

#include "vri_d3d11_common.h"

VriResult d3d11_queue_create(VriDevice device, const VriAllocationCallback *allocation_callback, VriQueue *p_queue);
void      d3d11_queue_destroy(VriDevice device, VriQueue queue);

#endif
