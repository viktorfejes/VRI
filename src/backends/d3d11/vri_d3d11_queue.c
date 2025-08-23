#include "vri_d3d11_queue.h"

#include "vri_d3d11_command_buffer.h"
#include "vri_d3d11_device.h"

#define QUEUE_STRUCT_SIZE (sizeof(struct VriQueue_T))

static VriResult d3d11_queue_submit(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count, VriFence fence);

VriResult d3d11_queue_create(VriDevice device, const VriAllocationCallback *allocation_callback, VriQueue *p_queue) {
    // Attempt to allocate the full internal struct
    size_t queue_size = QUEUE_STRUCT_SIZE;
    *p_queue = vri_object_allocate(device, allocation_callback, queue_size, VRI_OBJECT_QUEUE);
    if (!*p_queue) {
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    // NOTE: For D3D11 Queues there won't be any backends as we are storing the parent device
    // in the base field so we can just reach back to that for the immediate context which we'll need.

    // TODO: Fill up dispatch table!
    (*p_queue)->dispatch.pfn_queue_submit = d3d11_queue_submit;

    return VRI_SUCCESS;
}

void d3d11_queue_destroy(VriDevice device, VriQueue queue) {
    if (queue) {
        device->allocation_callback.pfn_free(queue, QUEUE_STRUCT_SIZE, 8);
    }
}

static VriResult d3d11_queue_submit(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count, VriFence fence) {
    (void)fence;
    // For the d3d11 backend we need to fetch the immediate context from
    // the parent device as the VriQueue type is just an empty wrapper
    VriD3D11Device       *pd = (VriD3D11Device *)(queue->base.p_device->p_backend_data);
    ID3D11DeviceContext4 *immediate_ctx = pd->p_immediate_context;

    for (uint32_t i = 0; i < submit_count; ++i) {
        const VriQueueSubmitDesc *submit = &p_submits[i];

        for (uint32_t j = 0; j < submit->command_buffer_count; ++j) {
            VriD3D11CommandBuffer *cb = submit->p_command_buffers[j]->p_backend_data;
            ID3D11CommandList     *cmd_list = cb->p_command_list;
            immediate_ctx->lpVtbl->ExecuteCommandList(immediate_ctx, cmd_list, FALSE);

            // if (cb->usage & VRI_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) {
            //     vri_command_buffer_reset(submit->p_command_buffers[j]);
            // }
        }
    }

    return VRI_SUCCESS;
}
