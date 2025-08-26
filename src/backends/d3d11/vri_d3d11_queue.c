#include "vri_d3d11_queue.h"

#include "vri_d3d11_command_buffer.h"
#include "vri_d3d11_device.h"
#include "vri_d3d11_fence.h"
#include "vri_d3d11_swapchain.h"

#define QUEUE_STRUCT_SIZE (sizeof(struct VriQueue_T))

static VriResult d3d11_queue_submit(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count, VriFence fence);
static VriResult d3d11_queue_present(VriQueue queue, const VriQueuePresentDesc *p_present_desc);

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
    (*p_queue)->dispatch.pfn_queue_present = d3d11_queue_present;

    return VRI_SUCCESS;
}

void d3d11_queue_destroy(VriDevice device, VriQueue queue) {
    if (queue) {
        device->allocation_callback.pfn_free(queue, QUEUE_STRUCT_SIZE, 8);
    }
}

static VriResult d3d11_queue_submit(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count, VriFence fence) {
    // For the d3d11 backend we need to fetch the immediate context from
    // the parent device as the VriQueue type is just an empty wrapper
    VriD3D11Device       *pd = (VriD3D11Device *)(queue->base.p_device->p_backend_data);
    ID3D11DeviceContext4 *immediate_ctx = pd->p_immediate_context;

    for (uint32_t i = 0; i < submit_count; ++i) {
        const VriQueueSubmitDesc *submit = &p_submits[i];

        // --- PHASE 1: WAIT ---
        for (uint32_t j = 0; j < submit->fence_wait_count; ++j) {
            VriD3D11Fence *wait_fence = submit->p_fences_wait[j].fence->p_backend_data;
            uint64_t       wait_value = submit->p_fences_wait[j].value;
            immediate_ctx->lpVtbl->Wait(immediate_ctx, wait_fence->p_fence, wait_value);
        }

        // --- PHASE 2: EXECUTE ---
        for (uint32_t j = 0; j < submit->command_buffer_count; ++j) {
            VriD3D11CommandBuffer *cb = submit->p_command_buffers[j]->p_backend_data;
            if (cb->p_command_list) {
                immediate_ctx->lpVtbl->ExecuteCommandList(immediate_ctx, cb->p_command_list, FALSE);

                // if (cb->usage & VRI_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) {
                //     vri_command_buffer_reset(submit->p_command_buffers[j]);
                // }
            }
        }

        // --- PHASE 3: SIGNAL ---
        for (uint32_t j = 0; j < submit->fence_signal_count; ++j) {
            VriD3D11Fence *signal_fence = submit->p_fences_signal[j].fence->p_backend_data;
            uint64_t       signal_value = submit->p_fences_signal[j].value;
            immediate_ctx->lpVtbl->Signal(immediate_ctx, signal_fence->p_fence, signal_value);
        }
    }

    // --- FINAL SIGNAL (Optional) ---
    if (fence != NULL) {
        VriD3D11Fence *final_fence = fence->p_backend_data;
        uint64_t       final_value = 1;
        immediate_ctx->lpVtbl->Signal(immediate_ctx, final_fence->p_fence, final_value);
    }

    return VRI_SUCCESS;
}

static VriResult d3d11_queue_present(VriQueue queue, const VriQueuePresentDesc *p_present_desc) {
    // Wait for timeline fences before presenting
    VriDevice device = queue->base.p_device;
    VriResult wait_result = d3d11_fences_wait(
        device,
        p_present_desc->p_wait_fences,
        p_present_desc->p_wait_values,
        p_present_desc->wait_fence_count,
        true,
        UINT64_MAX);

    if (wait_result != VRI_SUCCESS) {
        return wait_result;
    }

    VriResult overall_result = VRI_SUCCESS;

    // Present each swapchain
    for (uint32_t i = 0; i < p_present_desc->swapchain_count; ++i) {
        VriSwapchain swapchain = p_present_desc->p_swapchains[i];
        uint32_t     image_index = p_present_desc->p_image_indices ? p_present_desc->p_image_indices[i] : 0;

        VriResult present_result = d3d11_swapchain_present(swapchain, image_index);

        // Store per-swapchain result if requested
        if (p_present_desc->p_results) {
            p_present_desc->p_results[i] = present_result;
        }

        // Update overall results with error priority
        if (present_result != VRI_SUCCESS && overall_result == VRI_SUCCESS) {
            overall_result = present_result;
        }
    }

    // Signal timeline fences after present
    uint32_t signal_count = VRI_MIN(p_present_desc->signal_fence_count, p_present_desc->swapchain_count);
    for (uint32_t i = 0; i < signal_count; ++i) {
        VriFence fence = p_present_desc->p_signal_fences[i];
        uint64_t signal_value = p_present_desc->p_signal_values[i];

        VriResult signal_result = d3d11_fence_signal(fence, signal_value);
        if (signal_result != VRI_SUCCESS && overall_result == VRI_SUCCESS) {
            overall_result = signal_result;
        }
    }

    return overall_result;
}
