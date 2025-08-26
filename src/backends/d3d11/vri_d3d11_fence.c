#include "vri_d3d11_fence.h"

#include "vri/vri.h"
#include "vri_d3d11_device.h"

#define FENCE_OBJECT_SIZE (sizeof(struct VriFence_T) + sizeof(VriD3D11Fence))

static VriResult d3d11_fence_create(VriDevice device, uint64_t initial_value, VriFence *p_fence);
static void      d3d11_fence_destroy(VriDevice device, VriFence fence);
static uint64_t  d3d11_fence_get_value(VriDevice device, VriFence fence);

void d3d11_register_fence_functions(VriDeviceDispatchTable *table) {
    table->pfn_fence_create = d3d11_fence_create;
    table->pfn_fence_destroy = d3d11_fence_destroy;
    table->pfn_fence_get_value = d3d11_fence_get_value;
    table->pfn_fences_wait = d3d11_fences_wait;
}

static VriResult d3d11_fence_create(VriDevice device, uint64_t initial_value, VriFence *p_fence) {
    VriDebugCallback dbg = device->debug_callback;

    // Allocate fence
    *p_fence = vri_object_allocate(device, &device->allocation_callback, FENCE_OBJECT_SIZE, VRI_OBJECT_FENCE);
    if (!*p_fence) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to allocate fence");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    (*p_fence)->p_backend_data = (VriD3D11Fence *)(*p_fence + 1);

    VriD3D11Fence *d3d11_fence = (*p_fence)->p_backend_data;
    d3d11_fence->event = NULL;
    d3d11_fence->p_fence = NULL;

    VriD3D11Device *d3d11_device = device->p_backend_data;

    HRESULT hr = d3d11_device->p_device->lpVtbl->CreateFence(d3d11_device->p_device, initial_value, D3D11_FENCE_FLAG_NONE, COM_IID_PPV_ARGS(ID3D11Fence, &d3d11_fence->p_fence));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create D3D11 fence");
        device->allocation_callback.pfn_free(*p_fence, FENCE_OBJECT_SIZE, 8);
        *p_fence = NULL;
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    d3d11_fence->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!d3d11_fence->event) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create fence event");
        COM_RELEASE(d3d11_fence->p_fence);
        device->allocation_callback.pfn_free(*p_fence, FENCE_OBJECT_SIZE, 8);
        *p_fence = NULL;
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    return VRI_SUCCESS;
}

static void d3d11_fence_destroy(VriDevice device, VriFence fence) {
    if (fence) {
        VriD3D11Fence *d3d11_fence = fence->p_backend_data;
        if (d3d11_fence->event) {
            CloseHandle(d3d11_fence->event);
        }
        COM_SAFE_RELEASE(d3d11_fence->p_fence);

        // Free fence struct
        device->allocation_callback.pfn_free(fence, FENCE_OBJECT_SIZE, 8);
    }
}

static uint64_t d3d11_fence_get_value(VriDevice device, VriFence fence) {
    (void)device;
    VriD3D11Fence *f = fence->p_backend_data;
    return f->p_fence->lpVtbl->GetCompletedValue(f->p_fence);
}

VriResult d3d11_fences_wait(VriDevice device, const VriFence *p_fences, const uint64_t *p_values, uint32_t fence_count, VriBool wait_all, uint64_t timeout_ns) {
    (void)device;

    if (fence_count == 0) {
        return VRI_SUCCESS;
    }

    HANDLE *events_to_wait = device->allocation_callback.pfn_allocate(fence_count * sizeof(HANDLE), 8);
    if (!events_to_wait) {
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    uint32_t event_count = 0;
    for (uint32_t i = 0; i < fence_count; ++i) {
        VriD3D11Fence *fence = p_fences[i]->p_backend_data;
        ID3D11Fence   *d3d11_fence = fence->p_fence;

        if (d3d11_fence->lpVtbl->GetCompletedValue(d3d11_fence) >= p_values[i]) {
            if (!wait_all) {
                device->allocation_callback.pfn_free(events_to_wait, fence_count * sizeof(HANDLE), 8);
                return VRI_SUCCESS;
            }

            continue;
        }

        HRESULT hr = d3d11_fence->lpVtbl->SetEventOnCompletion(d3d11_fence, p_values[i], fence->event);
        if (FAILED(hr)) {
            device->allocation_callback.pfn_free(events_to_wait, fence_count * sizeof(HANDLE), 8);
            return VRI_ERROR_SYSTEM_FAILURE;
        }
        events_to_wait[event_count++] = fence->event;
    }

    VriResult res = VRI_SUCCESS;
    if (event_count > 0) {
        DWORD timeout_ms = (timeout_ns == UINT64_MAX) ? INFINITE : (DWORD)(timeout_ns / 1000000ULL);
        DWORD wait_result = WaitForMultipleObjectsEx(event_count, events_to_wait, (BOOL)wait_all, timeout_ms, FALSE);

        if (wait_result >= WAIT_OBJECT_0 && wait_result < (WAIT_OBJECT_0 + event_count)) {
            res = VRI_SUCCESS;
        } else if (wait_result == WAIT_TIMEOUT) {
            res = VRI_TIMEOUT;
        } else {
            res = VRI_ERROR_SYSTEM_FAILURE;
        }
    }
    device->allocation_callback.pfn_free(events_to_wait, fence_count * sizeof(HANDLE), 8);
    return res;
}

VriResult d3d11_fence_signal(VriFence fence, uint64_t value) {
    VriD3D11Fence *d3d11_fence = fence->p_backend_data;
    if (d3d11_fence->p_fence) {
        ID3D11DeviceContext4 *ctx = ((VriD3D11Device *)fence->base.p_device->p_backend_data)->p_immediate_context;
        HRESULT               hr = ctx->lpVtbl->Signal(ctx, d3d11_fence->p_fence, value);
        return SUCCEEDED(hr) ? VRI_SUCCESS : VRI_ERROR_SYSTEM_FAILURE;
    }

    return VRI_ERROR_SYSTEM_FAILURE;
}
