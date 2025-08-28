#include "vri_d3d11_command_buffer.h"

#include "vri_d3d11_device.h"
#include "vri_d3d11_pipeline.h"

#define COMMAND_BUFFER_OBJECT_SIZE (sizeof(struct VriCommandBuffer_T) + sizeof(VriD3D11CommandBuffer))

static VriResult d3d11_command_buffers_allocate(VriDevice device, const VriCommandBufferAllocateDesc *p_desc, VriCommandBuffer *p_command_buffers);
static void      d3d11_command_buffers_free(VriDevice device, VriCommandPool command_pool, uint32_t command_buffer_count, const VriCommandBuffer *p_command_buffers);
static VriResult d3d11_command_buffer_begin(VriCommandBuffer command_buffer, const VriCommandBufferBeginDesc *p_desc);
static VriResult d3d11_command_buffer_end(VriCommandBuffer command_buffer);
static VriResult d3d11_command_buffer_reset(VriCommandBuffer command_buffer);

void d3d11_register_command_buffer_functions(VriDeviceDispatchTable *table) {
    table->pfn_command_buffers_allocate = d3d11_command_buffers_allocate;
    table->pfn_command_buffers_free = d3d11_command_buffers_free;
}

static VriResult d3d11_command_buffers_allocate(VriDevice device, const VriCommandBufferAllocateDesc *p_desc, VriCommandBuffer *p_command_buffers) {
    VriDebugCallback dbg = device->debug_callback;

    for (uint32_t i = 0; i < p_desc->command_buffer_count; ++i) {
        VriCommandBuffer cmd = vri_object_allocate(device, &device->allocation_callback, COMMAND_BUFFER_OBJECT_SIZE, VRI_OBJECT_COMMAND_BUFFER);
        if (!cmd) return VRI_ERROR_OUT_OF_MEMORY;
        cmd->p_backend_data = (VriD3D11CommandBuffer *)(cmd + 1);

        VriD3D11CommandBuffer *impl = (VriD3D11CommandBuffer *)cmd->p_backend_data;
        ID3D11Device5         *d3d11_device = ((VriD3D11Device *)device->p_backend_data)->p_device;

        ID3D11DeviceContext *base_context = NULL;
        HRESULT              hr = d3d11_device->lpVtbl->CreateDeferredContext(d3d11_device, 0, &base_context);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed CreateDeferredContext.");
            return VRI_ERROR_SYSTEM_FAILURE;
        }

        hr = base_context->lpVtbl->QueryInterface(base_context, COM_IID_PPV_ARGS(ID3D11DeviceContext4, &impl->p_deferred_context));
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to upgrade deferred context to ID3D11DeviceContext4.");
            COM_RELEASE(base_context);
            return VRI_ERROR_UNSUPPORTED;
        }

        // Fill up the dispatch table
        // TODO: Move to a separate function
        cmd->dispatch.pfn_command_buffer_begin = d3d11_command_buffer_begin;
        cmd->dispatch.pfn_command_buffer_end = d3d11_command_buffer_end;
        cmd->dispatch.pfn_command_buffer_reset = d3d11_command_buffer_reset;

        // Fill from pipeline
        d3d11_register_pipeline_functions_with_command_buffer(&cmd->dispatch);

        cmd->state = VRI_COMMAND_BUFFER_STATE_INITIAL;
        impl->p_command_list = NULL;
        p_command_buffers[i] = cmd;

        COM_RELEASE(base_context);
    }

    return VRI_SUCCESS;
}

static void d3d11_command_buffers_free(VriDevice device, VriCommandPool command_pool, uint32_t command_buffer_count, const VriCommandBuffer *p_command_buffers) {
    (void)command_pool;
    for (uint32_t i = 0; i < command_buffer_count; i++) {
        VriD3D11CommandBuffer *impl = (VriD3D11CommandBuffer *)p_command_buffers[i]->p_backend_data;

        COM_SAFE_RELEASE(impl->p_deferred_context);
        COM_SAFE_RELEASE(impl->p_command_list);

        device->allocation_callback.pfn_free(p_command_buffers[i], COMMAND_BUFFER_OBJECT_SIZE, 8);
    }
}

static VriResult d3d11_command_buffer_begin(VriCommandBuffer command_buffer, const VriCommandBufferBeginDesc *p_desc) {
    (void)p_desc;

    command_buffer->pipeline = NULL;

    if (command_buffer->state != VRI_COMMAND_BUFFER_STATE_INITIAL &&
        command_buffer->state != VRI_COMMAND_BUFFER_STATE_EXECUTABLE) {
        return VRI_ERROR_INVALID_API_USAGE;
    }

    command_buffer->state = VRI_COMMAND_BUFFER_STATE_RECORDING;

    return VRI_SUCCESS;
}

static VriResult d3d11_command_buffer_end(VriCommandBuffer command_buffer) {
    if (command_buffer->state != VRI_COMMAND_BUFFER_STATE_RECORDING)
        return VRI_ERROR_INVALID_API_USAGE;

    VriD3D11CommandBuffer *cb = command_buffer->p_backend_data;

    HRESULT hr = cb->p_deferred_context->lpVtbl->FinishCommandList(cb->p_deferred_context, FALSE, &cb->p_command_list);
    if (FAILED(hr)) return VRI_ERROR_SYSTEM_FAILURE;

    command_buffer->state = VRI_COMMAND_BUFFER_STATE_EXECUTABLE;
    return VRI_SUCCESS;
}

static VriResult d3d11_command_buffer_reset(VriCommandBuffer command_buffer) {
    VriD3D11CommandBuffer *cb = command_buffer->p_backend_data;

    COM_SAFE_RELEASE(cb->p_command_list);
    command_buffer->state = VRI_COMMAND_BUFFER_STATE_INITIAL;

    return VRI_SUCCESS;
}
