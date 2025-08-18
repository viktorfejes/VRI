#include <vri/vri.h>

#include <platform/input.h>
#include <platform/platform.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct window {
    VriSwapchain       swapchain;
    platform_window_t *platform;
} window_t;

static VriDevice device;
static VriFence  image_available_fence;
// static VriCommandPool cmd_pool;

static platform_state_t *platform_state;
static input_state_t     input_state;
static window_t          window;

static void vri_logger(VriMessageSeverity severity, const char *p_msg) {
    static const char *severity_lut[] = {
        "INFO",
        "WARNING",
        "ERROR",
        "FATAL",
    };
    printf("[%s] %s\n", severity_lut[severity], p_msg);
}

int main(void) {
    VriDebugCallback vri_callback = {
        .pfn_message_callback = vri_logger,
    };

    VriAdapterDesc adapter_descs[2];
    uint32_t       adapter_desc_count = VRI_ARRAY_SIZE(adapter_descs);
    if (vri_adapters_enumerate(adapter_descs, &adapter_desc_count) != VRI_SUCCESS) {
        printf("Couldn't enumerate adapters\n");
        return 1;
    }

    VriDeviceDesc device_desc = {
        .backend = VRI_BACKEND_D3D11,
        .adapter_desc = adapter_descs[0],
        .enable_api_validation = true,
        .debug_callback = vri_callback,
    };
    if (vri_device_create(&device_desc, &device) != VRI_SUCCESS) {
        printf("Couldn't create D3D11 device\n");
        return 1;
    }

    // Verify what was created
    {
        printf("Device created\n");
        printf("   luid: %lld\n", adapter_descs[0].luid);
        printf("   device_id: %d\n", adapter_descs[0].device_id);
        printf("   vendor: %s\n", adapter_descs[0].vendor == VRI_GPU_VENDOR_NVIDIA ? "Nvidia" : adapter_descs[0].vendor == VRI_GPU_VENDOR_AMD ? "AMD"
                                                                                            : adapter_descs[0].vendor == VRI_GPU_VENDOR_INTEL ? "Intel"
                                                                                                                                              : "Unknown");
        printf("   vram: %llu MB\n", adapter_descs[0].vram / (1024 * 1024));
    }

    if (!input_initialize(&input_state)) {
        printf("Couldn't initialize input state\n");
        return 1;
    }

    size_t platform_mem = 0;
    platform_initialize(&platform_mem, 0);
    platform_state = malloc(platform_mem);
    if (!platform_initialize(&platform_mem, platform_state)) {
        printf("Couldn't initialize platform\n");
        return 1;
    }

    platform_window_desc_t w_desc = {
        .x = 0,
        .y = 0,
        .width = 1024,
        .height = 720,
    };
    platform_create_window(platform_state, &window.platform, &w_desc);

    VriWindowDesc window_desc = {
        .p_hwnd = platform_window_get_native_handle(window.platform),
    };

    VriSwapchainDesc swapchain_desc = {
        .p_window_desc = &window_desc,
        .width = platform_window_get_resolution(window.platform).x,
        .height = platform_window_get_resolution(window.platform).y,
        .format = VRI_FORMAT_R8G8B8A8_UNORM,
        .color_space = VRI_COLORSPACE_SRGB_NONLINEAR,
        .flags = VRI_SWAPCHAIN_FLAG_BIT_VSYNC,
        .texture_count = 2,
        .frames_in_flight = 3,
    };
    vri_swapchain_create(device, &swapchain_desc, &window.swapchain);

    if (vri_fence_create(device, 1, &image_available_fence) != VRI_SUCCESS) {
        printf("Couldn't create image available fence");
        return 1;
    }

    while (!platform_window_should_close(window.platform)) {
        platform_process_messages();

        uint32_t image_index = 0;
        vri_swapchain_acquire_next_image(device, window.swapchain, image_available_fence, &image_index);
        vri_swapchain_present(device, window.swapchain, 0);
    }

    // Clean-up
    vri_fence_destroy(device, image_available_fence);
    vri_swapchain_destroy(device, window.swapchain);
    vri_device_destroy(device);
    vri_report_live_objects();

    return 0;
}
