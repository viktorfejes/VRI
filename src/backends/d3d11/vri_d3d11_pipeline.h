#ifndef VRI_D3D11_PIPELINE_H
#define VRI_D3D11_PIPELINE_H

#include "vri_d3d11_common.h"

typedef struct {
    ID3D11VertexShader   *p_vertex_shader;
    ID3D11HullShader     *p_hull_shader;
    ID3D11DomainShader   *p_domain_shader;
    ID3D11GeometryShader *p_geometry_shader;
    ID3D11PixelShader    *p_pixel_shader;
    ID3D11ComputeShader  *p_compute_shader;
} VriD3D11ShaderModule;

typedef struct {
    ID3D11VertexShader      *p_vertex_shader;
    ID3D11HullShader        *p_hull_shader;
    ID3D11DomainShader      *p_domain_shader;
    ID3D11GeometryShader    *p_geometry_shader;
    ID3D11PixelShader       *p_pixel_shader;
    ID3D11ComputeShader     *p_compute_shader;
    ID3D11InputLayout       *p_input_layout;
    ID3D11RasterizerState   *p_rasterizer_state;
    ID3D11DepthStencilState *p_depth_stencil_state;
    ID3D11BlendState        *p_blend_state;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    D3D11_RASTERIZER_DESC    rasterizer_desc;
    uint32_t                 sample_mask;
    uint32_t                 sample_count;
    uint32_t                 stencil_ref;
    uint8_t                  render_target_count;
    bool                     sample_shading_enable;
} VriD3D11Pipeline;

void d3d11_register_pipeline_functions_with_device(VriDeviceDispatchTable *table);
void d3d11_register_pipeline_functions_with_command_buffer(VriCommandBufferDispatchTable *table);

#endif
