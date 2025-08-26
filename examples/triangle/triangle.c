#include <vri/vri.h>

#include <platform/input.h>
#include <platform/platform.h>

#include <stdio.h>
#include <stdlib.h>

#define MAX_CONCURRENT_FRAMES 2

typedef struct window {
    VriSwapchain       swapchain;
    platform_window_t *platform;
} window_t;

static VriDevice      device;
static VriCommandPool cmd_pool;

static uint8_t           current_frame = 0;
static platform_state_t *platform_state = NULL;
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

    VriAdapterProps adapter_props[2];
    uint32_t        adapter_props_count = VRI_ARRAY_SIZE(adapter_props);
    if (vri_adapters_enumerate(adapter_props, &adapter_props_count) != VRI_SUCCESS) {
        printf("Couldn't enumerate adapters\n");
        return 1;
    }

    VriQueueDesc qdescs[2] = {
        {.type = VRI_QUEUE_TYPE_GRAPHICS, .count = 1},
        {.type = VRI_QUEUE_TYPE_COMPUTE, .count = 1},
    };

    VriDeviceDesc device_desc = {
        .backend = VRI_BACKEND_D3D11,
        .p_adapter_props = &adapter_props[0],
        .p_queue_descs = qdescs,
        .queue_desc_count = VRI_ARRAY_SIZE(qdescs),
        .enable_api_validation = true,
        .debug_callback = vri_callback,
    };
    VriResult res = 0;
    if ((res = vri_device_create(&device_desc, &device)) != VRI_SUCCESS) {
        printf("Couldn't create D3D11 device (%d)\n", res);
        return 1;
    }

    // Verify what was created
    {
        printf("Device created\n");
        printf("   luid: %lld\n", adapter_props[0].luid);
        printf("   device_id: %d\n", adapter_props[0].device_id);
        printf("   vendor: %s\n", adapter_props[0].vendor == VRI_GPU_VENDOR_NVIDIA ? "Nvidia" : adapter_props[0].vendor == VRI_GPU_VENDOR_AMD ? "AMD"
                                                                                            : adapter_props[0].vendor == VRI_GPU_VENDOR_INTEL ? "Intel"
                                                                                                                                              : "Unknown");
        printf("   vram: %llu MB\n", adapter_props[0].vram / (1024 * 1024));
    }

    VriCommandBuffer cmd_buffers[2];
    VriFence         frame_fence;
    VriFence         image_available_fence;
    VriQueue         graphics_queue;

    vri_device_get_queue(device, VRI_QUEUE_TYPE_GRAPHICS, 0, &graphics_queue);
    if (!graphics_queue) {
        printf("Couldn't get needed queue\n");
        return 1;
    }

    {
        VriCommandBufferAllocateDesc desc = {
            .command_pool = cmd_pool,
            .command_buffer_count = MAX_CONCURRENT_FRAMES,
        };
        if (VRI_ERROR(vri_command_buffers_allocate(device, &desc, cmd_buffers))) {
            printf("Couldn't allocate command buffers\n");
            return 1;
        }

        if (VRI_ERROR(vri_fence_create(device, 1, &frame_fence))) {
            printf("Couldn't create frame fence\n");
            return 1;
        }
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
        .texture_count = MAX_CONCURRENT_FRAMES,
        .frames_in_flight = MAX_CONCURRENT_FRAMES,
    };
    vri_swapchain_create(device, &swapchain_desc, &window.swapchain);

    if (vri_fence_create(device, 1, &image_available_fence) != VRI_SUCCESS) {
        printf("Couldn't create image available fence");
        return 1;
    }

    {
        VriCommandPoolDesc desc = {
            .queue_type = VRI_QUEUE_TYPE_GRAPHICS,
            .flags = VRI_COMMAND_POOL_FLAG_BIT_RESET_COMMAND_BUFFER,
        };
        if (vri_command_pool_create(device, &desc, &cmd_pool) != VRI_SUCCESS) {
            printf("Couldn't create command pool");
            return 1;
        }
    }

    uint64_t frame_number = 0;
    uint64_t frame_complete_counter = 0;
    uint64_t image_acquire_counter = 0;

    while (!platform_window_should_close(window.platform)) {
        platform_process_messages();

        // Limit frames in flight
        if (frame_number >= MAX_CONCURRENT_FRAMES) {
            uint64_t wait_value = frame_number - MAX_CONCURRENT_FRAMES + 1;

            VriFence wait_fences[] = {frame_fence};
            uint64_t wait_values[] = {wait_value};

            VriResult wait_result = vri_fences_wait(device, wait_fences, wait_values, 1, true, UINT64_MAX);
            if (VRI_ERROR(wait_result)) {
                printf("Failed to wait for frame fence\n");
                break;
            }
        }

        uint32_t image_index = 0;
        uint64_t acquire_value = ++image_acquire_counter;
        if (VRI_ERROR(vri_swapchain_acquire_next_image(device, window.swapchain, image_available_fence, acquire_value, &image_index))) {
            printf("Failed to acquire next image\n");
            break;
        }

        const VriCommandBuffer command_buffer = cmd_buffers[current_frame];
        vri_command_buffer_reset(command_buffer);

        VriCommandBufferBeginDesc cmd_buf_desc = {0};
        if (VRI_ERROR(vri_command_buffer_begin(command_buffer, &cmd_buf_desc))) {
            printf("Command Buffer Begin Error\n");
        }

        // TODO: Record, render, draw...etc

        if (VRI_ERROR(vri_command_buffer_end(command_buffer))) {
            printf("Command Buffer End Error\n");
        }

        uint64_t submit_signal_value = ++frame_complete_counter;

        VriFenceWaitDesc wait_desc = {
            .fence = image_available_fence,
            .value = acquire_value,
        };
        VriFenceSignalDesc signal_desc = {
            .fence = frame_fence,
            .value = submit_signal_value,
        };

        VriQueueSubmitDesc submit_desc = {
            .command_buffer_count = 1,
            .p_command_buffers = &command_buffer,
            .fence_wait_count = 1,
            .p_fences_wait = &wait_desc,
            .fence_signal_count = 1,
            .p_fences_signal = &signal_desc,
        };

        if (VRI_ERROR(vri_queue_submit(graphics_queue, &submit_desc, 1))) {
            printf("Failed to submit command buffer\n");
            break;
        }

        // Present
        VriQueuePresentDesc present_desc = {
            .swapchain_count = 1,
            .p_swapchains = &window.swapchain,
            .p_image_indices = &image_index,
            .wait_fence_count = 1,
            .p_wait_fences = &frame_fence,
            .p_wait_values = &submit_signal_value,
        };

        if (VRI_ERROR(vri_queue_present(graphics_queue, &present_desc))) {
            printf("Failed to present\n");
            break;
        }

        // Select next frame to render
        current_frame = (current_frame + 1) % MAX_CONCURRENT_FRAMES;
        frame_number++;
    }

    // Clean-up
    vri_fence_destroy(device, frame_fence);
    vri_command_buffers_free(device, cmd_pool, 2, cmd_buffers);
    vri_command_pool_destroy(device, cmd_pool);
    vri_fence_destroy(device, image_available_fence);
    vri_swapchain_destroy(device, window.swapchain);
    vri_device_destroy(device);
    vri_report_live_objects();

    return 0;
}
