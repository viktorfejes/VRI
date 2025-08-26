#include "vri_d3d11_swapchain.h"

#include "vri_d3d11_device.h"
#include "vri_d3d11_texture.h"

#define SWAPCHAIN_STRUCT_SIZE (sizeof(struct VriSwapchain_T) + sizeof(VriD3D11Swapchain))

static VriResult swapchain_create(VriDevice device, const VriSwapchainDesc *p_desc, VriSwapchain *p_swapchain);
static void      swapchain_destroy(VriDevice device, VriSwapchain swapchain);
static VriResult swapchain_acquire_next_image(VriDevice device, VriSwapchain swapchain, VriFence fence, uint32_t *p_image_index);

void d3d11_register_swapchain_functions(VriDeviceDispatchTable *table) {
    table->pfn_swapchain_create = swapchain_create;
    table->pfn_swapchain_destroy = swapchain_destroy;
    table->pfn_swapchain_acquire_next_image = swapchain_acquire_next_image;
}

static VriResult swapchain_create(VriDevice device, const VriSwapchainDesc *p_desc, VriSwapchain *p_swapchain) {
    VriDebugCallback dbg = device->debug_callback;

    HWND hwnd = (HWND)p_desc->p_window_desc->p_hwnd;
    if (!hwnd) {
        return VRI_ERROR_INVALID_API_USAGE;
    }

    // TODO: Encapsulate into a getter on the device?
    VriD3D11Device *d3d11_device = device->p_backend_data;
    IDXGIAdapter   *adapter = d3d11_device->p_adapter;

    IDXGIFactory2   *factory2 = NULL;
    IDXGISwapChain1 *swapchain1 = NULL;
    IDXGISwapChain4 *swapchain4 = NULL;
    ID3D11Resource  *native_texture = NULL;
    VriResult        result = VRI_SUCCESS;

    // Get the factory from the adapter
    HRESULT hr = adapter->lpVtbl->GetParent(adapter, COM_IID_PPV_ARGS(IDXGIFactory2, &factory2));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to get DXGI Factory2");
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    DXGI_FORMAT format = vri_to_dxgi_format(p_desc->format)->typed;

    DXGI_SWAP_CHAIN_DESC1 desc = {
        .Width = p_desc->width,   // Zero here means it defaults to window width
        .Height = p_desc->height, // ... and to window height.
        .Format = format,
        .BufferCount = p_desc->texture_count, // I'm so confused by this BufferCount and whether it includes front buffer or not...
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .SampleDesc.Count = 1,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Scaling = DXGI_SCALING_NONE,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };

    // Create the base swapchain
    hr = factory2->lpVtbl->CreateSwapChainForHwnd(factory2, (IUnknown *)d3d11_device->p_device, hwnd, &desc, NULL, NULL, &swapchain1);
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create base swapchain");
        result = VRI_ERROR_SYSTEM_FAILURE;
        goto error;
    }

    // Make sure fullscreen is controlled by us
    hr = factory2->lpVtbl->MakeWindowAssociation(factory2, hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Couldn't make window association");
        result = VRI_ERROR_UNSUPPORTED;
        goto error;
    }

    // Upgrade to Swapchain4
    hr = swapchain1->lpVtbl->QueryInterface(swapchain1, COM_IID_PPV_ARGS(IDXGISwapChain4, &swapchain4));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to upgrade to IDXGISwapChain4. System may be too old.");
        result = VRI_ERROR_UNSUPPORTED;
        goto error;
    }

    // Color space
    DXGI_COLOR_SPACE_TYPE color_space = *vri_color_space_to_dxgi(p_desc->color_space);
    uint32_t              color_space_support = 0;

    hr = swapchain4->lpVtbl->CheckColorSpaceSupport(swapchain4, color_space, &color_space_support);
    if (SUCCEEDED(hr) && (color_space_support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
        hr = swapchain4->lpVtbl->SetColorSpace1(swapchain4, color_space);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_WARNING, "Couldn't set color space for swapchain");
        }
    }

    HANDLE  waitable_object = NULL;
    uint8_t frames_in_flight = p_desc->frames_in_flight;

    if (p_desc->flags & VRI_SWAPCHAIN_FLAG_BIT_WAITABLE) {
        hr = swapchain4->lpVtbl->SetMaximumFrameLatency(swapchain4, frames_in_flight);
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to set maximum frame latency");
            result = VRI_ERROR_UNSUPPORTED;
            goto error;
        }
        // Store waitable object handle after setting max latency
        waitable_object = swapchain4->lpVtbl->GetFrameLatencyWaitableObject(swapchain4);
    } else {
        if (frames_in_flight == 0) frames_in_flight = 2;

        IDXGIDevice1  *dxgi_device1 = NULL;
        ID3D11Device5 *d = d3d11_device->p_device;
        hr = d->lpVtbl->QueryInterface(d, COM_IID_PPV_ARGS(IDXGIDevice1, &dxgi_device1));
        if (SUCCEEDED(hr)) {
            dxgi_device1->lpVtbl->SetMaximumFrameLatency(dxgi_device1, frames_in_flight);
        }

        // Release the queried IDXGIDevice1
        dxgi_device1->lpVtbl->Release(dxgi_device1);
    }

    // We are allocating the swapchain here so we can allocate the texture together with it
    *p_swapchain = vri_object_allocate(device, &device->allocation_callback, SWAPCHAIN_STRUCT_SIZE, VRI_OBJECT_SWAPCHAIN);
    if (!*p_swapchain) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Allocation for swapchain struct failed.");
        result = VRI_ERROR_OUT_OF_MEMORY;
        goto error;
    }

    VriD3D11Swapchain *internal = (VriD3D11Swapchain *)((*p_swapchain) + 1);
    (*p_swapchain)->p_backend_data = internal;

    {
        // Get the texture for the swapchain
        hr = swapchain4->lpVtbl->GetBuffer(swapchain4, 0, COM_IID_PPV_ARGS(ID3D11Resource, &native_texture));
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to get buffer from swapchain");
            device->allocation_callback.pfn_free((*p_swapchain), SWAPCHAIN_STRUCT_SIZE, 8);
            result = VRI_ERROR_SYSTEM_FAILURE;
            goto error;
        }

        if (d3d11_texture_create_from_resource(device, &native_texture, &internal->texture) != VRI_SUCCESS) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Couldn't create textures from swapchain's backbuffer");
            result = VRI_ERROR_SYSTEM_FAILURE;
            goto error;
        }
    }

    // Save out the created pointer
    internal->p_hwnd = hwnd;
    internal->p_swapchain = swapchain4;
    internal->p_factory2 = factory2;
    internal->p_waitable_object = waitable_object;
    internal->present_id = 0;
    internal->flags = p_desc->flags;

    // Release temporaries
    COM_SAFE_RELEASE(swapchain1);

    return VRI_SUCCESS;

error:
    COM_SAFE_RELEASE(native_texture);
    COM_SAFE_RELEASE(swapchain4);
    COM_SAFE_RELEASE(swapchain1);
    COM_SAFE_RELEASE(factory2);

    return result;
}

static void swapchain_destroy(VriDevice device, VriSwapchain swapchain) {
    if (swapchain) {
        VriD3D11Swapchain *internal = (VriD3D11Swapchain *)swapchain->p_backend_data;

        if (internal) {
            d3d11_texture_destroy(device, internal->texture);

            COM_SAFE_RELEASE(internal->p_swapchain);
            COM_SAFE_RELEASE(internal->p_factory2);

            internal->p_waitable_object = NULL;
            internal->p_hwnd = NULL;
        }

        device->allocation_callback.pfn_free(swapchain, SWAPCHAIN_STRUCT_SIZE, 8);
    }
}

static VriResult swapchain_acquire_next_image(VriDevice device, VriSwapchain swapchain, VriFence fence, uint32_t *p_image_index) {
    (void)device;
    (void)swapchain;
    (void)fence;

    // Always 0 for D3D11 as we only have a single image that we are allowed to touch
    *p_image_index = 0;
    return VRI_SUCCESS;
}

VriResult d3d11_swapchain_present(VriSwapchain swapchain, uint32_t image_index) {
    VriD3D11Swapchain *internal = swapchain->p_backend_data;

    if (image_index >= 1) {
        return VRI_ERROR_INVALID_API_USAGE;
    }

    uint32_t sync_interval = !!(internal->flags & VRI_SWAPCHAIN_FLAG_BIT_VSYNC);
    uint32_t present_flags = ((!sync_interval) & !!(internal->flags & VRI_SWAPCHAIN_FLAG_BIT_ALLOW_TEARING)) * DXGI_PRESENT_ALLOW_TEARING;

    HRESULT hr = internal->p_swapchain->lpVtbl->Present(internal->p_swapchain, sync_interval, present_flags);

    if (SUCCEEDED(hr)) {
        internal->present_id++;
        return VRI_SUCCESS;
    }

    switch (hr) {
        case DXGI_ERROR_DEVICE_REMOVED:
        case DXGI_ERROR_DEVICE_RESET:
            return VRI_ERROR_DEVICE_REMOVED;
        case DXGI_STATUS_OCCLUDED:
            // This is not necessarily a failure, but the app might want to pause rendering
            return VRI_SUBOPTIMAL;
        default:
            return VRI_ERROR_SYSTEM_FAILURE;
    }
}
