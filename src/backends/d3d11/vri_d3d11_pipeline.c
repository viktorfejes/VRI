#include "vri_d3d11_pipeline.h"

#include "vri_d3d11_command_buffer.h"
#include "vri_d3d11_device.h"

#define PIPELINE_OBJECT_SIZE (sizeof(struct VriPipeline_T) + sizeof(VriD3D11Pipeline))

static VriResult d3d11_pipeline_layout_create(VriDevice device, const VriPipelineLayoutDesc *p_desc, VriPipelineLayout *p_pipeline_layout);
static VriResult d3d11_pipeline_create_graphics(VriDevice device, const VriGraphicsPipelineDesc *p_desc, VriPipeline *p_pipeline);
static VriResult d3d11_pipeline_create_compute(VriDevice device, const VriComputePipelineDesc *p_desc, VriPipeline *p_pipeline);
static void      d3d11_pipeline_bind(VriCommandBuffer command_buffer, VriPipeline pipeline);

void d3d11_register_pipeline_functions_with_device(VriDeviceDispatchTable *table) {
    table->pfn_pipeline_layout_create = d3d11_pipeline_layout_create;
    table->pfn_pipeline_create_graphics = d3d11_pipeline_create_graphics;
    table->pfn_pipeline_create_compute = d3d11_pipeline_create_compute;
}

void d3d11_register_pipeline_functions_with_command_buffer(VriCommandBufferDispatchTable *table) {
    table->pfn_cmd_bind_pipeline = d3d11_pipeline_bind;
}

static VriResult d3d11_pipeline_layout_create(VriDevice device, const VriPipelineLayoutDesc *p_desc, VriPipelineLayout *p_pipeline_layout) {
    (void)device;
    (void)p_desc;
    (void)p_pipeline_layout;

    return VRI_SUCCESS;
}

static VriResult d3d11_pipeline_create_graphics(VriDevice device, const VriGraphicsPipelineDesc *p_desc, VriPipeline *p_pipeline) {
    VriDebugCallback dbg = device->debug_callback;

    // Allocate pipeline internals
    *p_pipeline = vri_object_allocate(device, &device->allocation_callback, PIPELINE_OBJECT_SIZE, VRI_OBJECT_PIPELINE);
    if (!*p_pipeline) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to allocate pipeline object");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    (*p_pipeline)->p_backend_data = (VriD3D11Pipeline *)(*p_pipeline + 1);
    VriD3D11Pipeline *d3d11_pipeline = (*p_pipeline)->p_backend_data;

    HRESULT        hr = E_FAIL;
    ID3D11Device5 *d3d11_device = ((VriD3D11Device *)device->p_backend_data)->p_device;

    // Shaders
    {
        for (uint32_t i = 0; i < p_desc->shader_count; ++i) {
            const VriShaderModuleDesc *shader_desc = (p_desc->p_shaders + i);

            switch (shader_desc->stage) {
                case VRI_SHADER_STAGE_FLAG_BIT_VERTEX: {
                    hr = d3d11_device->lpVtbl->CreateVertexShader(
                        d3d11_device,
                        shader_desc->p_bytecode,
                        shader_desc->size,
                        NULL,
                        &d3d11_pipeline->p_vertex_shader);
                } break;

                case VRI_SHADER_STAGE_FLAG_BIT_TESSELATION_CONTROL: {
                    hr = d3d11_device->lpVtbl->CreateHullShader(
                        d3d11_device,
                        shader_desc->p_bytecode,
                        shader_desc->size,
                        NULL,
                        &d3d11_pipeline->p_hull_shader);
                } break;

                case VRI_SHADER_STAGE_FLAG_BIT_TESSELATION_EVALUATION: {
                    hr = d3d11_device->lpVtbl->CreateDomainShader(
                        d3d11_device,
                        shader_desc->p_bytecode,
                        shader_desc->size,
                        NULL,
                        &d3d11_pipeline->p_domain_shader);
                } break;

                case VRI_SHADER_STAGE_FLAG_BIT_GEOMETRY: {
                    hr = d3d11_device->lpVtbl->CreateGeometryShader(
                        d3d11_device,
                        shader_desc->p_bytecode,
                        shader_desc->size,
                        NULL,
                        &d3d11_pipeline->p_geometry_shader);
                } break;

                case VRI_SHADER_STAGE_FLAG_BIT_FRAGMENT: {
                    hr = d3d11_device->lpVtbl->CreatePixelShader(
                        d3d11_device,
                        shader_desc->p_bytecode,
                        shader_desc->size,
                        NULL,
                        &d3d11_pipeline->p_pixel_shader);
                } break;

                default:
                    return VRI_ERROR_INVALID_API_USAGE;
            }

            if (FAILED(hr)) {
                dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create shader for shader module");
                return VRI_ERROR_SYSTEM_FAILURE;
            }
        }
    }

    // Input Assembly State
    {
        d3d11_pipeline->topology = vri_topology_to_d3d11_topology(p_desc->p_input_assembly_state->topology);
    }

    // Vertex Input
    {
    }

    // Rasterization State
    {
        const VriRasterizationStateDesc *rdesc = p_desc->p_rasterization_state;

        D3D11_RASTERIZER_DESC rasterizer_desc = {
            .FillMode = rdesc->fill_mode == VRI_FILL_MODE_FILL ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME,
            .CullMode = vri_cull_mode_to_d3d11(rdesc->cull_mode),
            .FrontCounterClockwise = rdesc->front_face == VRI_FRONT_FACE_COUNTER_CLOCKWISE ? TRUE : FALSE,
            .DepthClipEnable = rdesc->depth_clamp_enable,
        };

        hr = d3d11_device->lpVtbl->CreateRasterizerState(d3d11_device, &rasterizer_desc, &d3d11_pipeline->p_rasterizer_state);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create rasterizer state");
            return VRI_ERROR_SYSTEM_FAILURE;
        }
    }

    return VRI_SUCCESS;
}

static VriResult d3d11_pipeline_create_compute(VriDevice device, const VriComputePipelineDesc *p_desc, VriPipeline *p_pipeline) {
    VriDebugCallback dbg = device->debug_callback;

    // Allocate pipeline internals
    *p_pipeline = vri_object_allocate(device, &device->allocation_callback, PIPELINE_OBJECT_SIZE, VRI_OBJECT_PIPELINE);
    if (!*p_pipeline) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to allocate pipeline object");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    (*p_pipeline)->p_backend_data = (VriD3D11Pipeline *)(*p_pipeline + 1);
    VriD3D11Pipeline *d3d11_pipeline = (*p_pipeline)->p_backend_data;
    ID3D11Device5    *d3d11_device = ((VriD3D11Device *)device->p_backend_data)->p_device;

    HRESULT hr = d3d11_device->lpVtbl->CreateComputeShader(
        d3d11_device,
        p_desc->p_shader[0].p_bytecode,
        p_desc->p_shader[0].size,
        NULL,
        &d3d11_pipeline->p_compute_shader);

    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create compute shader for compute pipeline");
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    return VRI_SUCCESS;
}

static void d3d11_pipeline_bind(VriCommandBuffer command_buffer, VriPipeline pipeline) {
    ID3D11DeviceContext4 *deferred_context = ((VriD3D11CommandBuffer *)command_buffer->p_backend_data)->p_deferred_context;

    VriPipeline       current_pipeline = command_buffer->pipeline;
    VriD3D11Pipeline *new_d3d11_pipeline = pipeline->p_backend_data;
    VriD3D11Pipeline *current_d3d11_pipeline = current_pipeline ? (VriD3D11Pipeline *)pipeline->p_backend_data : NULL;

    // Graphics Pipeline
    if (new_d3d11_pipeline->p_compute_shader == NULL) {
        // Shaders
        if (!current_pipeline || new_d3d11_pipeline->p_vertex_shader != current_d3d11_pipeline->p_vertex_shader) {
            deferred_context->lpVtbl->VSSetShader(deferred_context, new_d3d11_pipeline->p_vertex_shader, NULL, 0);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_hull_shader != current_d3d11_pipeline->p_hull_shader) {
            deferred_context->lpVtbl->HSSetShader(deferred_context, new_d3d11_pipeline->p_hull_shader, NULL, 0);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_domain_shader != current_d3d11_pipeline->p_domain_shader) {
            deferred_context->lpVtbl->DSSetShader(deferred_context, new_d3d11_pipeline->p_domain_shader, NULL, 0);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_geometry_shader != current_d3d11_pipeline->p_geometry_shader) {
            deferred_context->lpVtbl->GSSetShader(deferred_context, new_d3d11_pipeline->p_geometry_shader, NULL, 0);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_pixel_shader != current_d3d11_pipeline->p_pixel_shader) {
            deferred_context->lpVtbl->PSSetShader(deferred_context, new_d3d11_pipeline->p_pixel_shader, NULL, 0);
        }

        // States
        if (!current_pipeline || new_d3d11_pipeline->topology != current_d3d11_pipeline->topology) {
            deferred_context->lpVtbl->IASetPrimitiveTopology(deferred_context, new_d3d11_pipeline->topology);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_rasterizer_state != current_d3d11_pipeline->p_rasterizer_state) {
            deferred_context->lpVtbl->RSSetState(deferred_context, new_d3d11_pipeline->p_rasterizer_state);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_blend_state != current_d3d11_pipeline->p_blend_state) {
            deferred_context->lpVtbl->OMSetBlendState(deferred_context, new_d3d11_pipeline->p_blend_state, NULL, 0xFFFFFFFF);
        }
        if (!current_pipeline || new_d3d11_pipeline->p_depth_stencil_state != current_d3d11_pipeline->p_depth_stencil_state) {
            deferred_context->lpVtbl->OMSetDepthStencilState(deferred_context, new_d3d11_pipeline->p_depth_stencil_state, 0);
        }
    }
    // Compute Pipeline
    else {
        if (!current_pipeline || new_d3d11_pipeline->p_compute_shader != current_d3d11_pipeline->p_compute_shader) {
            deferred_context->lpVtbl->CSSetShader(deferred_context, new_d3d11_pipeline->p_compute_shader, NULL, 0);
        }
    }
}
