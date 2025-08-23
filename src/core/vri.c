#include "vri/vri.h"
#include "vri_internal.h"

#include <string.h>

#if VRI_ENABLE_D3D11_SUPPORT
#    include <d3d11_4.h>
#    include <dxgidebug.h>
#endif

#if VRI_ENABLE_VK_SUPPORT
#    include <vulkan.h>
#endif

#define ADAPTER_MAX_COUNT 32

#define IID_PPV_ARGS_C(type, pp_type) \
    &IID_##type, (void **)(pp_type)

#define TYPE_SHIFT  60
#define VRAM_SHIFT  4
#define VENDOR_MASK 0xFull
#define VRAM_MASK   0x0FFFFFFFFFFFFFF0ull

// Forward declaration of backend functions so they don't have to be included
extern VriResult none_device_create(
    const VriDeviceDesc *p_desc,
    VriDevice           *p_device);

extern VriResult d3d11_device_create(
    const VriDeviceDesc *p_desc,
    VriDevice           *p_device);

extern VriResult vk_device_create(
    const VriDeviceDesc *p_desc,
    VriDevice           *p_device);

// Rest of the forward declarations
static VriGpuVendor get_vendor_from_id(uint32_t vendor_id);
static int          sort_adapters(const void *a, const void *b);
static void         setup_callbacks(VriDeviceDesc *p_desc);
static void         finish_device_creation(VriDeviceDesc *p_desc, VriDevice *p_device);
static void        *default_allocator_allocate(size_t size, size_t alignment);
static void         default_allocator_free(void *p_memory, size_t size, size_t alignment);
static void         default_message_callback(VriMessageSeverity severity, const char *p_message);

void vri_object_base_init(VriDevice device, VriObjectBase *base, VriObjectType type) {
    base->type = type;
    base->p_device = device;
}

void *vri_object_allocate(VriDevice device, const VriAllocationCallback *alloc, size_t size, VriObjectType type) {
    void *ptr = alloc->pfn_allocate(size, 8);
    if (ptr == NULL) return NULL;

    memset(ptr, 0, size);

    vri_object_base_init(device, (VriObjectBase *)ptr, type);
    return ptr;
}

#if (VRI_ENABLE_D3D11_SUPPORT || VRI_ENABLE_D3D12_SUPPORT)
static VriResult d3d_enum_adapters(VriAdapterProps *p_descs, uint32_t *p_desc_count) {
    IDXGIFactory4 *dxgi_factory = NULL;
    HRESULT        hr = CreateDXGIFactory2(0, IID_PPV_ARGS_C(IDXGIFactory4, &dxgi_factory));
    if (FAILED(hr)) {
        return VRI_ERROR_UNSUPPORTED;
    }

    uint32_t       adapter_count = 0;
    IDXGIAdapter1 *adapters[ADAPTER_MAX_COUNT];

    for (uint32_t i = 0; i < ADAPTER_MAX_COUNT; ++i) {
        IDXGIAdapter1 *adapter = NULL;
        hr = dxgi_factory->lpVtbl->EnumAdapters1(dxgi_factory, i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = {0};
        if (adapter->lpVtbl->GetDesc1(adapter, &desc) == S_OK) {
            if (desc.Flags == DXGI_ADAPTER_FLAG_NONE) {
                adapters[adapter_count++] = adapter;
            } else {
                adapter->lpVtbl->Release(adapter);
            }
        } else {
            adapter->lpVtbl->Release(adapter);
        }
    }

    if (!adapter_count) {
        dxgi_factory->lpVtbl->Release(dxgi_factory);
        return VRI_ERROR_SYSTEM_FAILURE;
    }

    VriAdapterProps queried_adapter_descs[ADAPTER_MAX_COUNT];
    uint32_t        validated_adapter_count = 0;

    for (uint32_t i = 0; i < adapter_count; ++i) {
        DXGI_ADAPTER_DESC desc = {0};
        adapters[i]->lpVtbl->GetDesc(adapters[i], &desc);

        VriAdapterProps *adapter_desc = &queried_adapter_descs[validated_adapter_count];
        adapter_desc->luid = *(uint64_t *)&desc.AdapterLuid;
        adapter_desc->device_id = desc.DeviceId;
        adapter_desc->vendor = get_vendor_from_id(desc.VendorId);
        adapter_desc->vram = desc.DedicatedVideoMemory;
        adapter_desc->shared_system_memory = desc.SharedSystemMemory;

        // Since queue count cannot be queried in D3D, we'll use some defaults
        adapter_desc->queue_count[VRI_QUEUE_TYPE_GRAPHICS] = 1;
        adapter_desc->queue_count[VRI_QUEUE_TYPE_COMPUTE] = 1;
        adapter_desc->queue_count[VRI_QUEUE_TYPE_TRANSFER] = 1;
        adapter_desc->queue_count[VRI_QUEUE_TYPE_PRESENT] = 1;

        // Get the GPU type by creating a device, at the same time test IF we can create a device
        D3D_FEATURE_LEVEL fl[2] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        ID3D11Device     *device = NULL;

        hr = D3D11CreateDevice((IDXGIAdapter *)adapters[i], D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, fl, 2, D3D11_SDK_VERSION, &device, NULL, NULL);

        if (FAILED(hr)) {
            // Cannot create device on the adapter, so "bin" it
            adapters[i]->lpVtbl->Release(adapters[i]);
            adapters[i] = NULL;
            continue;
        }

        D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2 = {0};
        hr = device->lpVtbl->CheckFeatureSupport(device, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2));
        if (SUCCEEDED(hr)) {
            adapter_desc->type = o2.UnifiedMemoryArchitecture ? VRI_GPU_TYPE_INTEGRATED : VRI_GPU_TYPE_DISCRETE;
        }

        // We don't need the device at this point so just release it
        device->lpVtbl->Release(device);

        validated_adapter_count++;

        // Release the adapter as we are done with it (so we don't have to do this in another loop)
        adapters[i]->lpVtbl->Release(adapters[i]);
    }

    // Clean up factory
    dxgi_factory->lpVtbl->Release(dxgi_factory);

    if (validated_adapter_count == 0) {
        return VRI_ERROR_UNSUPPORTED;
    }

    // Sort adapters based on some scoring
    qsort(queried_adapter_descs, validated_adapter_count, sizeof(queried_adapter_descs[0]), sort_adapters);

    *p_desc_count = VRI_MIN(*p_desc_count, validated_adapter_count);
    for (uint32_t i = 0; i < *p_desc_count; ++i) {
        p_descs[i] = queried_adapter_descs[i];
    }

    return VRI_SUCCESS;
}
#endif

VriResult vri_adapters_enumerate(VriAdapterProps *p_descs, uint32_t *p_desc_count) {
    VriResult result = VRI_ERROR_SYSTEM_FAILURE;

#if VRI_ENABLE_VK_SUPPORT
    // Vulkan return actual capabilities, so let's try this first
    result = vk_enum_adapters(p_desc, p_desc_count);
#endif

#if (VRI_ENABLE_D3D11_SUPPORT || VRI_ENABLE_D3D12_SUPPORT)
    // Only if Vulkan is not available do we use anything other than...
    if (result != VRI_SUCCESS) {
        result = d3d_enum_adapters(p_descs, p_desc_count);
    }
#endif

#if VRI_ENABLE_NONE_SUPPORT && !(VRI_ENABLE_VK_SUPPORT || VRI_ENABLE_D3D11_SUPPORT || VRI_ENABLE_D3D12_SUPPORT)
    if (result != VRI_SUCCESS) {
        if (*p_desc_count > 1) {
            *p_desc_count = 1;
        }

        result = VRI_SUCCESS;
    }
#endif

    return result;
}

void vri_report_live_objects(void) {
#if (VRI_ENABLE_D3D11_SUPPORT || VRI_ENABLE_D3D12_SUPPORT)
    IDXGIDebug1 *debug = NULL;
    HRESULT      hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS_C(IDXGIDebug1, &debug));
    if (SUCCEEDED(hr)) {
        debug->lpVtbl->ReportLiveObjects(debug, DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)((uint32_t)DXGI_DEBUG_RLO_DETAIL | (uint32_t)DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
#endif
}

VriResult vri_device_create(const VriDeviceDesc *p_desc, VriDevice *p_device) {
    VriResult result = VRI_SUCCESS;

    VriDeviceDesc mod_desc = *p_desc;
    setup_callbacks(&mod_desc);

#if VRI_ENABLE_NONE_SUPPORT
    if (mod_desc.gpu_api == VRI_GPU_API_NONE)
        result = none_device_create(&mod_desc, p_device);
#endif

#if VRI_ENABLE_D3D11_SUPPORT
    if (mod_desc.backend == VRI_BACKEND_D3D11)
        result = d3d11_device_create(&mod_desc, p_device);
#endif

#if VRI_ENABLE_D3D12_SUPPORT
    if (mod_desc.backend == VRI_BACKEND_D3D12)
        result = d3d12_device_create(&mod_desc, p_device);
#endif

#if VRI_ENABLE_VK_SUPPORT
    if (mod_desc.backend == VRI_BACKEND_VULKAN)
        result = vk_device_create(&mod_desc, p_device);
#endif

    if (VRI_ERROR(result)) return VRI_ERROR_SYSTEM_FAILURE;

    finish_device_creation(&mod_desc, p_device);

    return VRI_SUCCESS;
}

void vri_device_get_queue(VriDevice device, VriQueueType queue_type, uint32_t queue_index, VriQueue *p_queue) {
    if (device) {
        if (queue_type >= VRI_QUEUE_TYPE_COUNT) return;
        if (queue_index >= device->queue_counts[queue_type]) return;

        *p_queue = device->queues[queue_type][queue_index];
    }
}

// Calling Device table
void vri_device_destroy(VriDevice device) {
    device->dispatch.pfn_device_destroy(device);
}

VriResult vri_command_pool_create(VriDevice device, const VriCommandPoolDesc *p_desc, VriCommandPool *p_command_pool) {
    return device->dispatch.pfn_command_pool_create(device, p_desc, p_command_pool);
}

void vri_command_pool_destroy(VriDevice device, VriCommandPool command_pool) {
    device->dispatch.pfn_command_pool_destroy(device, command_pool);
}

void vri_command_pool_reset(VriDevice device, VriCommandPool command_pool, VriCommandPoolResetFlags flags) {
    device->dispatch.pfn_command_pool_reset(device, command_pool, flags);
}

VriResult vri_command_buffers_allocate(VriDevice device, const VriCommandBufferAllocateDesc *p_desc, VriCommandBuffer *p_command_buffers) {
    return device->dispatch.pfn_command_buffers_allocate(device, p_desc, p_command_buffers);
}

void vri_command_buffers_free(VriDevice device, VriCommandPool command_pool, uint32_t command_buffer_count, const VriCommandBuffer *p_command_buffers) {
    device->dispatch.pfn_command_buffers_free(device, command_pool, command_buffer_count, p_command_buffers);
}

VriResult vri_texture_create(VriDevice device, const VriTextureDesc *p_desc, VriTexture *p_texture) {
    return device->dispatch.pfn_texture_create(device, p_desc, p_texture);
}

void vri_texture_destroy(VriDevice device, VriTexture texture) {
    device->dispatch.pfn_texture_destroy(device, texture);
}

VriResult vri_fence_create(VriDevice device, uint64_t initial_value, VriFence *p_fence) {
    return device->dispatch.pfn_fence_create(device, initial_value, p_fence);
}

void vri_fence_destroy(VriDevice device, VriFence fence) {
    device->dispatch.pfn_fence_destroy(device, fence);
}

uint64_t vri_fence_get_value(VriDevice device, VriFence fence) {
    return device->dispatch.pfn_fence_get_value(device, fence);
}

VriResult vri_fences_wait(VriDevice device, VriFence *p_fences, uint64_t *p_values, uint32_t fence_count, VriBool wait_all, uint64_t timeout_ns) {
    return device->dispatch.pfn_fences_wait(device, p_fences, p_values, fence_count, wait_all, timeout_ns);
}

VriResult vri_swapchain_create(VriDevice device, const VriSwapchainDesc *p_desc, VriSwapchain *p_swapchain) {
    return device->dispatch.pfn_swapchain_create(device, p_desc, p_swapchain);
}

void vri_swapchain_destroy(VriDevice device, VriSwapchain swapchain) {
    device->dispatch.pfn_swapchain_destroy(device, swapchain);
}

VriResult vri_swapchain_acquire_next_image(VriDevice device, VriSwapchain swapchain, VriFence fence, uint32_t *p_image_index) {
    return device->dispatch.pfn_swapchain_acquire_next_image(device, swapchain, fence, p_image_index);
}

VriResult vri_swapchain_present(VriDevice device, VriSwapchain swapchain, VriFence fence) {
    return device->dispatch.pfn_swapchain_present(device, swapchain, fence);
}

// Calling Command Buffer table
VriResult vri_command_buffer_begin(VriCommandBuffer command_buffer, const VriCommandBufferBeginDesc *p_desc) {
    return command_buffer->dispatch.pfn_command_buffer_begin(command_buffer, p_desc);
}

VriResult vri_command_buffer_end(VriCommandBuffer command_buffer) {
    return command_buffer->dispatch.pfn_command_buffer_end(command_buffer);
}

VriResult vri_command_buffer_reset(VriCommandBuffer command_buffer) {
    return command_buffer->dispatch.pfn_command_buffer_reset(command_buffer);
}

VriResult vri_queue_submit(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count, VriFence fence) {
    return queue->dispatch.pfn_queue_submit(queue, p_submits, submit_count, fence);
}

VriResult vri_queue_wait_idle(VriQueue queue) {
    return queue->dispatch.pfn_queue_wait_idle(queue);
}

VriResult vri_queue_present(VriQueue queue, const VriQueuePresentDesc *p_present) {
    return queue->dispatch.pfn_queue_present(queue, p_present);
}

static VriGpuVendor get_vendor_from_id(uint32_t vendor_id) {
    switch (vendor_id) {
        case 0x10DE:
            return VRI_GPU_VENDOR_NVIDIA;
        case 0x1002:
            return VRI_GPU_VENDOR_AMD;
        case 0x8086:
            return VRI_GPU_VENDOR_INTEL;
        default:
            return VRI_GPU_VENDOR_UNKNOWN;
    }
}

static int sort_adapters(const void *a, const void *b) {
    const VriAdapterProps *adapter_a = (const VriAdapterProps *)a;
    const VriAdapterProps *adapter_b = (const VriAdapterProps *)b;

    // Priority order: [type:4][vram:56][vendor:4]
    uint64_t type_a = (adapter_a->type == VRI_GPU_TYPE_DISCRETE) ? 1 : 0;
    uint64_t score_a = ((uint64_t)adapter_a->vendor & VENDOR_MASK) |
                       ((adapter_a->vram << VRAM_SHIFT) & VRAM_MASK) |
                       (type_a << TYPE_SHIFT);

    uint64_t type_b = (adapter_b->type == VRI_GPU_TYPE_DISCRETE) ? 1 : 0;
    uint64_t score_b = ((uint64_t)adapter_b->vendor & VENDOR_MASK) |
                       ((adapter_b->vram << VRAM_SHIFT) & VRAM_MASK) |
                       (type_b << TYPE_SHIFT);

    if (score_a > score_b) return -1;
    if (score_a < score_b) return 1;
    return 0;
}

static void setup_callbacks(VriDeviceDesc *p_desc) {
    if (!p_desc->allocation_callback.pfn_allocate || !p_desc->allocation_callback.pfn_free) {
        p_desc->allocation_callback.pfn_allocate = default_allocator_allocate;
        p_desc->allocation_callback.pfn_free = default_allocator_free;
    }

    if (!p_desc->debug_callback.pfn_message_callback) {
        p_desc->debug_callback.pfn_message_callback = default_message_callback;
    }
}

static void finish_device_creation(VriDeviceDesc *p_desc, VriDevice *p_device) {
    (*p_device)->allocation_callback = p_desc->allocation_callback;
    (*p_device)->debug_callback = p_desc->debug_callback;
    (*p_device)->adapter_props = *p_desc->p_adapter_props;
}

static void *default_allocator_allocate(size_t size, size_t alignment) {
    (void)alignment;
    return malloc(size);
}

static void default_allocator_free(void *p_memory, size_t size, size_t alignment) {
    (void)size;
    (void)alignment;
    free(p_memory);
}

static void default_message_callback(VriMessageSeverity severity, const char *p_message) {
    (void)severity;
    (void)p_message;
    // NO-OP
}
