#include "vri_d3d11_pipeline.h"

#include "vri/vri.h"
#include "vri_d3d11_command_buffer.h"
#include "vri_d3d11_common.h"
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
    VriResult      err = VRI_SUCCESS;
    ID3D11Device5 *d3d11_device = ((VriD3D11Device *)device->p_backend_data)->p_device;

    const VriShaderModuleDesc *vertex_shader = NULL;

    // Shaders
    {
        for (uint32_t i = 0; i < p_desc->shader_count; ++i) {
            const VriShaderModuleDesc *shader_desc = (p_desc->p_shaders + i);

            switch (shader_desc->stage) {
                case VRI_SHADER_STAGE_FLAG_BIT_VERTEX: {
                    vertex_shader = shader_desc;
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
                    device->allocation_callback.pfn_free(*p_pipeline, PIPELINE_OBJECT_SIZE, 8);
                    return VRI_ERROR_INVALID_API_USAGE;
            }

            if (FAILED(hr)) {
                dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create shader for shader module");
                err = VRI_ERROR_SYSTEM_FAILURE;
                goto error;
            }
        }
    }

    // Input Assembly State
    {
        d3d11_pipeline->topology = vri_topology_to_d3d11_topology(p_desc->p_input_assembly_state->topology);
    }

    // Vertex Input
    {
        if (p_desc->p_vertex_input) {
            // Make sure we have vertex shader when we have vertex input
            if (!vertex_shader) {
                err = VRI_ERROR_INVALID_API_USAGE;
                goto error;
            }

            const VriVertexInputDesc *vdesc = p_desc->p_vertex_input;

            size_t                    elems_allocate_size = sizeof(D3D11_INPUT_ELEMENT_DESC) * vdesc->attribute_count;
            D3D11_INPUT_ELEMENT_DESC *elems = device->allocation_callback.pfn_allocate(elems_allocate_size, 8);
            if (!elems) {
                err = VRI_ERROR_OUT_OF_MEMORY;
                goto error;
            }

            for (uint32_t i = 0; i < vdesc->attribute_count; ++i) {
                const VriVertexAttributeDesc *attr = &vdesc->p_attributes[i];
                VriVertexInputRate            input_rate = vdesc->p_bindings[attr->binding].input_rate;

                elems[i].SemanticName = attr->d3d.semantic_name; // NOTE: semantic_name might need to live as long as we need the input
                elems[i].SemanticIndex = attr->d3d.semantic_index;
                elems[i].Format = vri_to_dxgi_format(attr->format)->typed;
                elems[i].InputSlot = attr->binding;
                elems[i].AlignedByteOffset = attr->offset;
                elems[i].InputSlotClass = input_rate == VRI_VERTEX_INPUT_RATE_VERTEX ? D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA;
                elems[i].InstanceDataStepRate = input_rate == VRI_VERTEX_INPUT_RATE_VERTEX ? 0 : 1;
            }

            hr = d3d11_device->lpVtbl->CreateInputLayout(
                d3d11_device,
                elems,
                vdesc->attribute_count,
                vertex_shader->p_bytecode,
                vertex_shader->size,
                &d3d11_pipeline->p_input_layout);

            device->allocation_callback.pfn_free(elems, elems_allocate_size, 8);

            if (FAILED(hr)) {
                dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create input layout");
                err = VRI_ERROR_SYSTEM_FAILURE;
                goto error;
            }
        }
    }

    // Rasterization State
    {
        const VriRasterizationStateDesc *rdesc = p_desc->p_rasterization_state;

        D3D11_RASTERIZER_DESC rasterizer_desc = {
            .FillMode = rdesc->fill_mode == VRI_FILL_MODE_FILL ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME,
            .CullMode = vri_cull_mode_to_d3d11(rdesc->cull_mode),
            .FrontCounterClockwise = rdesc->front_face == VRI_FRONT_FACE_COUNTER_CLOCKWISE ? TRUE : FALSE,
            .DepthClipEnable = rdesc->depth_clamp_enable ? FALSE : TRUE,
            .MultisampleEnable = p_desc->p_multisample_state->sample_count > 1 ? TRUE : FALSE,
        };

        hr = d3d11_device->lpVtbl->CreateRasterizerState(d3d11_device, &rasterizer_desc, &d3d11_pipeline->p_rasterizer_state);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create rasterizer state");
            err = VRI_ERROR_SYSTEM_FAILURE;
            goto error;
        }
    }

    // Blend State (OM)
    {
        // If user provided a color blend state, use it; otherwise create a default opaque write-through state.
        D3D11_BLEND_DESC blend_desc = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
        };

        if (p_desc->p_color_blend_state) {
            const VriColorBlendStateDesc *cbs = p_desc->p_color_blend_state;
            blend_desc.IndependentBlendEnable = cbs->independent_blend_enable ? TRUE : FALSE;
            blend_desc.AlphaToCoverageEnable = cbs->alpha_to_coverage_enable ? TRUE : FALSE;

            uint32_t rt_count = cbs->render_target_count ? cbs->render_target_count : 1;
            rt_count = rt_count > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT ? D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT : rt_count;

            for (uint32_t i = 0; i < rt_count; ++i) {
                const VriColorBlendAttachmentDesc *att = &cbs->render_targets[i];

                D3D11_RENDER_TARGET_BLEND_DESC *rt = &blend_desc.RenderTarget[i];
                rt->BlendEnable = att->blend_enable ? TRUE : FALSE;
                rt->SrcBlend = vri_blend_factor_to_d3d11(att->src_color_blend_factor);
                rt->DestBlend = vri_blend_factor_to_d3d11(att->dst_color_blend_factor);
                rt->BlendOp = vri_blend_op_to_d3d11(att->color_blend_op);
                rt->SrcBlendAlpha = vri_blend_factor_to_d3d11(att->src_alpha_blend_factor);
                rt->DestBlendAlpha = vri_blend_factor_to_d3d11(att->dst_alpha_blend_factor);
                rt->BlendOpAlpha = vri_blend_op_to_d3d11(att->alpha_blend_op);
                rt->RenderTargetWriteMask = att->color_write_mask;
            }

            d3d11_pipeline->render_target_count = rt_count;
        } else {
            // default: no blending, write all channels
            blend_desc.RenderTarget[0].BlendEnable = FALSE;
            blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            d3d11_pipeline->render_target_count = 1;
        }

        hr = d3d11_device->lpVtbl->CreateBlendState(d3d11_device, &blend_desc, &d3d11_pipeline->p_blend_state);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create blend state");
            goto error;
        }

        // store a default sample mask (D3D11 uses sample mask on OMSetBlendState)
        d3d11_pipeline->sample_mask = 0xFFFFFFFF;
    }

    // Depth-Stencil State
    {
        if (p_desc->p_depth_stencil_state) {
            const VriDepthStencilStateDesc *dsdesc = p_desc->p_depth_stencil_state;

            D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {
                .DepthEnable = dsdesc->depth_test_enable ? TRUE : FALSE,
                .DepthWriteMask = dsdesc->depth_write_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO,
                .DepthFunc = vri_compare_op_to_d3d11(dsdesc->depth_compare_op),

                .StencilEnable = dsdesc->stencil_test_enable ? TRUE : FALSE,
                .StencilReadMask = dsdesc->stencil_read_mask,
                .StencilWriteMask = dsdesc->stencil_write_mask,

                .FrontFace.StencilFailOp = vri_stencil_op_to_d3d11(dsdesc->front.fail_op),
                .FrontFace.StencilDepthFailOp = vri_stencil_op_to_d3d11(dsdesc->front.depth_fail_op),
                .FrontFace.StencilPassOp = vri_stencil_op_to_d3d11(dsdesc->front.pass_op),
                .FrontFace.StencilFunc = vri_compare_op_to_d3d11(dsdesc->front.compare_op),

                .BackFace.StencilFailOp = vri_stencil_op_to_d3d11(dsdesc->back.fail_op),
                .BackFace.StencilDepthFailOp = vri_stencil_op_to_d3d11(dsdesc->back.depth_fail_op),
                .BackFace.StencilPassOp = vri_stencil_op_to_d3d11(dsdesc->back.pass_op),
                .BackFace.StencilFunc = vri_compare_op_to_d3d11(dsdesc->back.compare_op),
            };

            hr = d3d11_device->lpVtbl->CreateDepthStencilState(d3d11_device, &depth_stencil_desc, &d3d11_pipeline->p_depth_stencil_state);
            if (FAILED(hr)) {
                dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create depth stencil state");
                err = VRI_ERROR_SYSTEM_FAILURE;
                goto error;
            }

            d3d11_pipeline->stencil_ref = dsdesc->stencil_reference ? dsdesc->stencil_reference : 0;
        } else {
            // If no depth/stencil state supplied, create a default disabled depth/stencil state
            D3D11_DEPTH_STENCIL_DESC dsd = {0};
            dsd.DepthEnable = FALSE;
            dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
            dsd.StencilEnable = FALSE;

            hr = d3d11_device->lpVtbl->CreateDepthStencilState(d3d11_device, &dsd, &d3d11_pipeline->p_depth_stencil_state);
            if (FAILED(hr)) {
                dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create default depth/stencil state");
                goto error;
            }
            d3d11_pipeline->stencil_ref = 0;
        }
    }

    // Multisample
    {
        if (p_desc->p_multisample_state) {
            // D3D11 doesn't have a dedicated multisample state object like Vulkan.
            // Sampling behavior is mostly determined at texture/RT creation. We still store sample_count.
            d3d11_pipeline->sample_count = (UINT)p_desc->p_multisample_state->sample_count;
            d3d11_pipeline->sample_shading_enable = p_desc->p_multisample_state->sample_shading_enable;
            // alpha-to-coverage handled by blend_desc.AlphaToCoverageEnable above
        } else {
            d3d11_pipeline->sample_count = 1;
            d3d11_pipeline->sample_shading_enable = 0;
        }
    }

    return VRI_SUCCESS;

error:
    // Release com objects in case they have been created
    COM_SAFE_RELEASE(d3d11_pipeline->p_vertex_shader);
    COM_SAFE_RELEASE(d3d11_pipeline->p_input_layout);
    COM_SAFE_RELEASE(d3d11_pipeline->p_hull_shader);
    COM_SAFE_RELEASE(d3d11_pipeline->p_domain_shader);
    COM_SAFE_RELEASE(d3d11_pipeline->p_geometry_shader);
    COM_SAFE_RELEASE(d3d11_pipeline->p_pixel_shader);
    COM_SAFE_RELEASE(d3d11_pipeline->p_rasterizer_state);
    COM_SAFE_RELEASE(d3d11_pipeline->p_depth_stencil_state);
    COM_SAFE_RELEASE(d3d11_pipeline->p_blend_state);

    // Free pipeline object
    device->allocation_callback.pfn_free(*p_pipeline, PIPELINE_OBJECT_SIZE, 8);
    *p_pipeline = NULL;

    return err;
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
    if (!pipeline) return;

    ID3D11DeviceContext4 *deferred_context = ((VriD3D11CommandBuffer *)command_buffer->p_backend_data)->p_deferred_context;
    VriPipeline           current_pipeline = command_buffer->pipeline;
    VriD3D11Pipeline     *new_d3d11_pipeline = pipeline->p_backend_data;
    VriD3D11Pipeline     *current_d3d11_pipeline = current_pipeline ? (VriD3D11Pipeline *)pipeline->p_backend_data : NULL;

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
