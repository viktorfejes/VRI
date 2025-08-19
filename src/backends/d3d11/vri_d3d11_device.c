#include "vri_d3d11_common.h"

#include "vri_d3d11_command_buffer.h"
#include "vri_d3d11_command_pool.h"
#include "vri_d3d11_device.h"
#include "vri_d3d11_fence.h"
#include "vri_d3d11_swapchain.h"
#include "vri_d3d11_texture.h"

static size_t get_device_size(void);
static void   d3d11_register_device_functions(VriDeviceDispatchTable *table);

VriResult d3d11_device_create(const VriDeviceDesc *p_desc, VriDevice *p_device) {
    // Convinience assignment for the debug messages
    VriDebugCallback dbg = p_desc->debug_callback;

    // Attempt to allocate the full internal struct
    size_t device_size = get_device_size();
    *p_device = vri_object_allocate(*p_device, &p_desc->allocation_callback, device_size, VRI_OBJECT_DEVICE);
    if (!*p_device) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Allocation for device struct failed.");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    // Dispatch table is right after the device
    VriDeviceDispatchTable *disp = (VriDeviceDispatchTable *)(*p_device + 1);
    (*p_device)->p_dispatch = disp;

    // Backend data is after the dispatch table
    VriD3D11Device *internal_state = (VriD3D11Device *)(disp + 1);
    (*p_device)->p_backend_data = internal_state;

    // Identify the adapter in the API specific way
    // TODO: Add fallback when we just want an adapter so luid == 0
    // In that case we can use NULL for the adapter and D3D_DRIVER_TYPE_HARDWARE
    {
        VriAdapterDesc adapter_desc = p_desc->adapter_desc;
        IDXGIFactory4 *dxgi_factory = NULL;
        HRESULT        hr = CreateDXGIFactory2(p_desc->enable_api_validation ? D3D11_CREATE_DEVICE_DEBUG : 0, COM_IID_PPV_ARGS(IDXGIFactory4, &dxgi_factory));
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Failed to create DXGIFactory2 for adapter identification");
            return VRI_ERROR_SYSTEM_FAILURE;
        }

        // LUID luid = *(LUID *)&adapter_desc.luid;
        LUID luid = {
            .LowPart = (DWORD)(adapter_desc.luid & 0xFFFFFFFF),
            .HighPart = (LONG)(adapter_desc.luid >> 32),
        };
        hr = dxgi_factory->lpVtbl->EnumAdapterByLuid(dxgi_factory, luid, COM_IID_PPV_ARGS(IDXGIAdapter, &internal_state->p_adapter));
        if (FAILED(hr)) {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Couldn't get IDXGIAdapter. The provided LUID may be invalid.");
            dxgi_factory->lpVtbl->Release(dxgi_factory);
            return VRI_ERROR_INVALID_API_USAGE;
        }

        dxgi_factory->lpVtbl->Release(dxgi_factory);
    }

    UINT              create_device_flags = p_desc->enable_api_validation ? D3D11_CREATE_DEVICE_DEBUG : 0;
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    ID3D11Device        *base_device = NULL;
    ID3D11DeviceContext *base_context = NULL;
    D3D_FEATURE_LEVEL    achieved_level = {0};

    HRESULT hr = D3D11CreateDevice(
        internal_state->p_adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        create_device_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &base_device,
        &achieved_level,
        &base_context);

    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Failed to create D3D11 device. Check driver compatibility and installation.");
        p_desc->allocation_callback.pfn_free(*p_device, device_size, 8);
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    // Upgrade to D3D11.4 - Device5
    ID3D11Device5 *device5 = NULL;
    hr = base_device->lpVtbl->QueryInterface(base_device, COM_IID_PPV_ARGS(ID3D11Device5, &device5));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11Device5. The system's D3D11 version is too old.");
        p_desc->allocation_callback.pfn_free(*p_device, device_size, 8);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return VRI_ERROR_UNSUPPORTED;
    }

    // Upgrade Immediate Device Context, as well.
    ID3D11DeviceContext4 *context4 = NULL;
    hr = base_context->lpVtbl->QueryInterface(base_context, COM_IID_PPV_ARGS(ID3D11DeviceContext4, &context4));
    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11DeviceContext4. The system's D3D11 version is too old.");
        p_desc->allocation_callback.pfn_free(*p_device, device_size, 8);
        device5->lpVtbl->Release(device5);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return VRI_ERROR_UNSUPPORTED;
    }

    if (p_desc->enable_api_validation) {
        // Set up enhanced debug layer
        ID3D11InfoQueue *info_queue = NULL;
        hr = device5->lpVtbl->QueryInterface(device5, COM_IID_PPV_ARGS(ID3D11InfoQueue, &info_queue));

        if (SUCCEEDED(hr) && info_queue) {
            // No breaking on errors, only logging
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_WARNING, FALSE);

            // Enable message storage so we can retrieve them
            info_queue->lpVtbl->SetMuteDebugOutput(info_queue, FALSE);

            // Set storage limit
            info_queue->lpVtbl->SetMessageCountLimit(info_queue, 1024);

            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_INFO, "D3D11 debug layer enabled for logging");
            info_queue->lpVtbl->Release(info_queue);
        } else {
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to get ID3D11InfoQueue for debug layer setup.");
        }
    }

    // Fill out the known fields
    (*p_device)->backend = VRI_BACKEND_D3D11;
    internal_state->p_device = device5;
    internal_state->p_immediate_context = context4;

    // Fill up the dispatch table
    d3d11_register_device_functions((*p_device)->p_dispatch);
    d3d11_register_command_pool_functions((*p_device)->p_dispatch);
    d3d11_register_command_buffer_functions((*p_device)->p_dispatch);
    d3d11_register_texture_functions((*p_device)->p_dispatch);
    d3d11_register_fence_functions((*p_device)->p_dispatch);
    d3d11_register_swapchain_functions((*p_device)->p_dispatch);

    // Release remaining not needed resources
    base_device->lpVtbl->Release(base_device);
    base_context->lpVtbl->Release(base_context);

    return VRI_SUCCESS;
}

static void d3d11_device_destroy(VriDevice device) {
    if (device) {
        VriD3D11Device *internal_state = (VriD3D11Device *)device->p_backend_data;

        if (internal_state) {
            // Release the GPU resources
            COM_SAFE_RELEASE(internal_state->p_adapter);
            COM_SAFE_RELEASE(internal_state->p_immediate_context);
            COM_SAFE_RELEASE(internal_state->p_device);
        }

        // Free the ENTIRE allocated block (device + internal_state)
        size_t device_size = get_device_size();
        device->allocation_callback.pfn_free(device, device_size, 8);
    }
}

static size_t get_device_size(void) {
    return sizeof(struct VriDevice_T) +    // Base device size
           sizeof(VriD3D11Device) +        // Internal backend size
           sizeof(VriDeviceDispatchTable); // Size of the dispatch table (vtable)
}

static void d3d11_register_device_functions(VriDeviceDispatchTable *table) {
    table->pfn_device_destroy = d3d11_device_destroy;
}
