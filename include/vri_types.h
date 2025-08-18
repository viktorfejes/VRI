#pragma once

#include "vri_macros.h"

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct vri_device_t* VriDevice;
typedef struct vri_swapchain vri_swapchain_t;
typedef struct vri_texture vri_texture_t;
typedef struct vri_queue vri_queue_t;
typedef struct vri_buffer vri_buffer_t;
typedef struct vri_cmd_buffer vri_cmd_buffer_t;
typedef struct vri_fence_t *VriFence;
typedef struct vri_cmd_pool_t *VriCommandPool;

// Special fence value for swapchain semaphore
#define VRI_SWAPCHAIN_SEMAPHORE ((uint64_t)-1)

/* Overall error type for Vri */
typedef enum {
    VRI_SUCCESS = 0,
    VRI_FAILURE,
    VRI_UNSUPPORTED,
    VRI_INVALID_ARGUMENT,
} VriResult;

/* Message severity enum for callbacks */
typedef enum {
    VRI_MESSAGE_SEVERITY_INFO,
    VRI_MESSAGE_SEVERITY_WARNING,
    VRI_MESSAGE_SEVERITY_ERROR,
    VRI_MESSAGE_SEVERITY_FATAL,
} vri_message_severity_t;

/* Main enum for graphics API selection */
typedef enum {
    VRI_GPU_API_NONE,
    VRI_GPU_API_D3D11,
    VRI_GPU_API_D3D12,
    VRI_GPU_API_VULKAN,
    VRI_GPU_API_WEBGPU,
    VRI_GPU_API_METAL,
    VRI_GPU_API_OPENGL,
} vri_gpu_api_t;

typedef enum {
    VRI_GPU_TYPE_UNKNOWN,
    VRI_GPU_TYPE_INTEGRATED,
    VRI_GPU_TYPE_DISCRETE,
} vri_gpu_type_t;

typedef enum {
    VRI_VENDOR_UNKNOWN,
    VRI_VENDOR_INTEL,
    VRI_VENDOR_AMD,
    VRI_VENDOR_NVIDIA
} vri_vendor_t;

typedef enum {
    VRI_QUEUE_TYPE_GRAPHICS,
    VRI_QUEUE_TYPE_COMPUTE,
    VRI_QUEUE_TYPE_TRANSFER,
    VRI_QUEUE_TYPE_COUNT
} vri_queue_type_t;

typedef enum {
    VRI_R8G8B8A8_UNORM,
    VRI_FORMAT_COUNT
} vri_format_t;

typedef enum {
    VRI_BUFFER_USAGE_VERTEX_BUFFER,
    VRI_BUFFER_USAGE_INDEX_BUFFER,
} vri_buffer_usage_t;

typedef enum {
    VRI_MEMORY_TYPE_GPU_ONLY, // DEVICE (D3D11: DEFAULT, no CPU access)
    VRI_MEMORY_TYPE_UPLOAD,   // DEVICE_UPLOAD/HOST_UPLOAD (D3D11: DYNAMIC, CPU_WRITE)
    VRI_MEMORY_TYPE_READBACK, // HOST_READBACK (D3D11: STAGING, CPU_READ)
} vri_memory_type_t;

typedef enum {
    VRI_TEXTURE_TYPE_TEXTURE_1D,
    VRI_TEXTURE_TYPE_TEXTURE_2D,
    VRI_TEXTURE_TYPE_TEXTURE_3D,
    VRI_TEXTURE_TYPE_TEXTURE_CUBE,
} vri_texture_type_t;

typedef enum {
    VRI_SWAPCHAIN_FORMAT_REC709_8BIT_SRGB,
    VRI_SWAPCHAIN_FORMAT_REC709_16BIT_LINEAR,
    VRI_SWAPCHAIN_FORMAT_COUNT
} vri_swapchain_format_t;

typedef enum {
    VRI_SWAPCHAIN_FLAG_BIT_NONE = 0,
    VRI_SWAPCHAIN_FLAG_BIT_VSYNC = 1 << 0,
    VRI_SWAPCHAIN_FLAG_BIT_WAITABLE = 1 << 1,
    VRI_SWAPCHAIN_FLAG_BIT_ALLOW_TEARING = 1 << 2,
} vri_swapchain_flag_bit_t;

typedef enum {
    VRI_TEXTURE_USAGE_BIT_NONE = 0,
    VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE = 1 << 0,
    VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE_STORAGE = 1 << 1,
    VRI_TEXTURE_USAGE_BIT_COLOR_ATTACHMENT = 1 << 2,
    VRI_TEXTURE_USAGE_BIT_DEPTH_STENCIL_ATTACHMENT,
    VRI_TEXTURE_USAGE_BIT_SHADING_RATE_ATTACHMENT,
} vri_texture_usage_bit_t;

typedef struct {
    uint64_t luid;
    uint32_t device_id;
    vri_vendor_t vendor;
    uint64_t vram;
    uint32_t shared_system_memory;
    vri_gpu_type_t type;
    uint32_t queue_count[VRI_QUEUE_TYPE_COUNT];
} vri_adapter_desc_t;

typedef struct {
    void (*message_callback)(vri_message_severity_t severity, const char *message);
} vri_debug_callback_t;

typedef struct {
    void *(*allocate)(size_t size, size_t alignment);
    void (*free)(void *memory, size_t size, size_t alignment);
} vri_allocation_callback_t;

typedef struct {
    vri_gpu_api_t gpu_api;
    vri_adapter_desc_t adapter_desc;

    vri_debug_callback_t debug_callback;
    vri_allocation_callback_t allocation_callback;

    bool enable_api_validation;
} vri_device_desc_t;

typedef struct {
    uint64_t size;
    vri_buffer_usage_t usage;
    vri_memory_type_t memory_type;
} vri_buffer_desc_t;

typedef struct {
    uint64_t src_offset;
    uint64_t dst_offset;
    uint64_t size;
} vri_buffer_copy_region_t;

typedef struct vri_window_win32 {
    void *hwnd;
} vri_window_win32_t;

typedef struct vri_window_xcb {
    void *connection;
    void *window;
} vri_window_xcb_t;

typedef struct vri_window_wl {
    void *display;
    void *surface;
} vri_window_wl_t;

typedef struct vri_window_metal {
    void *caMetalLayer;
} vri_window_metal_t;

typedef union {
    vri_window_win32_t win32;
    vri_window_xcb_t xcb;
    vri_window_wl_t wl;
    vri_window_metal_t metal;
} vri_window_t;

typedef struct {
    vri_window_t window;
    uint32_t width;
    uint32_t height;
    vri_swapchain_format_t format;
    uint32_t flags;
    uint8_t texture_count;
    uint8_t frames_in_flight;
} vri_swapchain_desc_t;

typedef struct {
    vri_texture_type_t type;
    vri_format_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t usage;
    uint32_t sample_count;
    uint32_t mip_count;
    uint32_t layer_count;
} vri_texture_desc_t;

struct vri_interface_swapchain;
struct vri_interface_core;

typedef struct vri_interface_core {
    VriResult (*create_fence)(VriDevice device, uint64_t initial_value, VriFence *fence);
    void (*destroy_fence)(VriDevice device, VriFence fence);

    VriResult (*create_command_pool)(VriDevice device, VriCommandPool *command_pool_ptr);
    void (*destroy_command_pool)(VriDevice device, VriCommandPool command_pool);

    VriResult (*buffer_create)(VriDevice device, vri_buffer_desc_t *buffer_desc, vri_buffer_t **out_buffer);
    VriResult (*buffer_map)(vri_buffer_t *staging_buffer, void **mapped_ptr);
    VriResult (*buffer_unmap)(vri_buffer_t *staging_buffer);
    VriResult (*cmd_copy_buffer)(vri_cmd_buffer_t *cmd_buffer, vri_buffer_t *staging_buffer, vri_buffer_t *target_buffer, vri_buffer_copy_region_t *copy_region);

    bool (*queue_submit)(vri_queue_t *queue);

    bool (*texture_create)(VriDevice device, vri_texture_desc_t *texture_desc, vri_texture_t *out_texture);
} vri_interface_core_t;

typedef struct vri_interface_swapchain {
    VriResult (*swapchain_create)(VriDevice device, const vri_swapchain_desc_t *swapchain_desc, vri_swapchain_t **out_swapchain);
    void (*swapchain_destroy)(VriDevice device, vri_swapchain_t *swapchain);

    VriResult (*acquire_next_image)(VriDevice device, vri_swapchain_t *swapchain, VriFence fence, uint32_t *image_index);
    VriResult (*swapchain_present)(VriDevice device, vri_swapchain_t *swapchain, VriFence);
} vri_interface_swapchain_t;
