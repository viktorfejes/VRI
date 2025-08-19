#ifndef VRI_D3D11_COMMAND_BUFFER_H
#define VRI_D3D11_COMMAND_BUFFER_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11DeviceContext4 *p_deferred_context;
    ID3D11CommandList    *p_command_list;
} VriD3D11CommandBuffer;

void d3d11_register_command_buffer_functions(VriDeviceDispatchTable *table);

#endif
