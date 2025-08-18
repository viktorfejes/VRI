#include "vri_d3d11_fence.h"

#include "vri_d3d11_device.h"

static VriResult d3d11_fence_create(VriDevice device, uint64_t initial_value, VriFence *p_fence);
static void      d3d11_fence_destroy(VriDevice device, VriFence fence);
static size_t    get_fence_size(void);

void d3d11_register_fence_functions(VriDeviceDispatchTable *table) {
    table->pfn_fence_create = d3d11_fence_create;
    table->pfn_fence_destroy = d3d11_fence_destroy;
}

static VriResult d3d11_fence_create(VriDevice device, uint64_t initial_value, VriFence *p_fence) {
    VriDebugCallback dbg = device->debug_callback;

    // Allocate fence
    size_t fence_size = get_fence_size();
    *p_fence = vri_object_allocate(device, &device->allocation_callback, fence_size, VRI_OBJECT_FENCE);
    if (!*p_fence) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to allocate fence");
        return VRI_FAILURE;
    }

    VriD3D11Fence *d3d11_fence = (VriD3D11Fence *)(*p_fence + 1);
    (*p_fence)->p_backend_data = d3d11_fence;

    VriD3D11Device *d3d11_device = device->p_backend_data;

    if (initial_value == VRI_SWAPCHAIN_SEMAPHORE) {
        d3d11_fence->p_fence = NULL;
        d3d11_fence->event = NULL;
        d3d11_fence->value = VRI_SWAPCHAIN_SEMAPHORE;
        return VRI_SUCCESS;
    }

    HRESULT hr = d3d11_device->p_device->lpVtbl->CreateFence(d3d11_device->p_device, 0, D3D11_FENCE_FLAG_NONE, COM_IID_PPV_ARGS(ID3D11Fence, &d3d11_fence->p_fence));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create D3D11 fence");
        device->allocation_callback.pfn_free(*p_fence, fence_size, 8);
        *p_fence = NULL;
        return VRI_FAILURE;
    }

    d3d11_fence->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!d3d11_fence->event) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create fence event");
        COM_RELEASE(d3d11_fence->p_fence);
        device->allocation_callback.pfn_free(*p_fence, fence_size, 8);
        *p_fence = NULL;
        return VRI_FAILURE;
    }

    d3d11_fence->value = initial_value;

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
        size_t fence_size = get_fence_size();
        device->allocation_callback.pfn_free(fence, fence_size, 8);
    }
}

static size_t get_fence_size(void) {
    return sizeof(struct VriFence_T) + // Base fence size
           sizeof(VriD3D11Fence);      // Internal backend size
}
