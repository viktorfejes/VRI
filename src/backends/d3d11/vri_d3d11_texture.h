#ifndef VRI_D3D11_TEXTURE_H
#define VRI_D3D11_TEXTURE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11Resource *p_resource;
} VriD3D11Texture;

void      d3d11_register_texture_functions(VriDeviceDispatchTable *table);
void      d3d11_texture_destroy(VriDevice device, VriTexture p_texture);
VriResult d3d11_texture_create_from_resource(VriDevice device, ID3D11Resource **resource, VriTexture *p_texture);

#endif
