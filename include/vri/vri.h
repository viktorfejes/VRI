#ifndef VRI_H
#define VRI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vri_platform.h"

#define VRI_MAKE_VERSION(major, minor, patch) \
    (((major) << 22) | ((minor) << 12) | (patch))

#define VRI_VERSION_MAJOR(version) ((version) >> 22)
#define VRI_VERSION_MINOR(version) (((version) >> 12) & 0x3ff)
#define VRI_VERSION_PATCH(version) ((version) & 0xfff)

#define VRI_HEADER_VERSION VRI_MAKE_VERSION(0, 1, 0)
#define VRI_NULL_HANDLE    0

#define VRI_DEFINE_HANDLE(object) typedef struct object##_T *object;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#    define VRI_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
#else
#    define VRI_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif

typedef uint32_t VriFlags;
typedef uint64_t VriDeviceSize;
typedef bool     VriBool;

VRI_DEFINE_HANDLE(VriDevice)
VRI_DEFINE_HANDLE(VriQueue)
VRI_DEFINE_HANDLE(VriCommandBuffer)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriCommandPool)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriFence)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriSwapchain)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriTexture)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriPipeline)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriPipelineLayout)
VRI_DEFINE_NON_DISPATCHABLE_HANDLE(VriShaderModule)

#define VRI_TRUE                1
#define VRI_FALSE               0
#define VRI_SWAPCHAIN_SEMAPHORE ((uint64_t)-1)

#define VRI_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define VRI_MIN(a, b)     ((a) < (b) ? (a) : (b))
#define VRI_MAX(a, b)     ((a) > (b) ? (a) : (b))

typedef enum {
    VRI_ERROR_SYSTEM_FAILURE = -5,
    VRI_ERROR_DEVICE_REMOVED = -4,
    VRI_ERROR_UNSUPPORTED = -3,
    VRI_ERROR_OUT_OF_MEMORY = -2,
    VRI_ERROR_INVALID_API_USAGE = -1,
    VRI_SUCCESS = 0,
    VRI_INCOMPLETE = 1,
    VRI_SUBOPTIMAL = 2,
    VRI_TIMEOUT = 3,
    VRI_RESULT_MAX_ENUM = 0x7FFFFFFF
} VriResult;
#define VRI_OK(result)    ((result) >= VRI_SUCCESS)
#define VRI_ERROR(result) ((result) < VRI_SUCCESS)

typedef enum {
    VRI_MESSAGE_SEVERITY_INFO = 0,
    VRI_MESSAGE_SEVERITY_WARNING = 1,
    VRI_MESSAGE_SEVERITY_ERROR = 2,
    VRI_MESSAGE_SEVERITY_FATAL = 3,
    VRI_MESSAGE_SEVERITY_MAX_ENUM = 0x7FFFFFFF
} VriMessageSeverity;

typedef enum {
    VRI_BACKEND_NONE = 0,
    VRI_BACKEND_D3D11 = 1,
    VRI_BACKEND_D3D12 = 2,
    VRI_BACKEND_VULKAN = 3,
    VRI_BACKEND_WEBGPU = 4,
    VRI_BACKEND_METAL = 5,
    VRI_BACKEND_OPENGL = 6,
    VRI_BACKEND_MAX_ENUM = 0x7FFFFFFF
} VriBackend;

typedef enum {
    VRI_GPU_TYPE_UNKNOWN = 0,
    VRI_GPU_TYPE_INTEGRATED = 1,
    VRI_GPU_TYPE_DISCRETE = 2,
    VRI_GPU_TYPE_MAX_ENUM = 0x7FFFFFFF
} VriGpuType;

typedef enum {
    VRI_GPU_VENDOR_UNKNOWN = 0,
    VRI_GPU_VENDOR_INTEL = 1,
    VRI_GPU_VENDOR_AMD = 2,
    VRI_GPU_VENDOR_NVIDIA = 3,
    VRI_GPU_VENDOR_MAX_ENUM = 0x7FFFFFFF
} VriGpuVendor;

typedef enum {
    VRI_FORMAT_UNDEFINED = 0,
    VRI_FORMAT_R8G8B8A8_UNORM,
    VRI_FORMAT_COUNT,
    VRI_FORMAT_MAX_ENUM = 0x7FFFFFFF
} VriFormat;

typedef enum {
    VRI_COLORSPACE_SRGB_NONLINEAR = 0,
    VRI_COLORSPACE_SRGB_LINEAR,
    VRI_COLORSPACE_BT709_NONLINEAR,
    VRI_COLORSPACE_BT709_LINEAR,
    VRI_COLORSPACE_P3_NONLINEAR,
    VRI_COLORSPACE_P3_LINEAR,
    VRI_COLORSPACE_BT2020_NONLINEAR,
    VRI_COLORSPACE_BT2020_LINEAR,
    VRI_COLORSPACE_HDR10_ST2084,
    VRI_COLORSPACE_HDR10_HLG,
    VRI_COLORSPACE_EXTENDED_SRGB_LINEAR,
    VRI_COLORSPACE_COUNT,
    VRI_COLORSPACE_MAX_ENUM = 0x7FFFFFFF
} VriColorSpace;

typedef enum {
    VRI_QUEUE_TYPE_GRAPHICS,
    VRI_QUEUE_TYPE_COMPUTE,
    VRI_QUEUE_TYPE_TRANSFER,
    VRI_QUEUE_TYPE_PRESENT,
    VRI_QUEUE_TYPE_COUNT,
    VRI_QUEUE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VriQueueType;

typedef enum {
    VRI_MEMORY_TYPE_GPU_ONLY, // DEVICE (D3D11: DEFAULT, no CPU access)
    VRI_MEMORY_TYPE_UPLOAD,   // DEVICE_UPLOAD/HOST_UPLOAD (D3D11: DYNAMIC, CPU_WRITE)
    VRI_MEMORY_TYPE_READBACK, // HOST_READBACK (D3D11: STAGING, CPU_READ)
    VRI_MEMORY_TYPE_MAX_ENUM = 0x7FFFFFFF
} VriMemoryType;

typedef enum {
    VRI_TEXTURE_TYPE_TEXTURE_1D,
    VRI_TEXTURE_TYPE_TEXTURE_2D,
    VRI_TEXTURE_TYPE_TEXTURE_3D,
    VRI_TEXTURE_TYPE_TEXTURE_CUBE,
    VRI_TEXTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VriTextureType;

typedef enum {
    VRI_COMMAND_BUFFER_STATE_INVALID = 0,
    VRI_COMMAND_BUFFER_STATE_INITIAL = 1,
    VRI_COMMAND_BUFFER_STATE_RECORDING = 2,
    VRI_COMMAND_BUFFER_STATE_EXECUTABLE = 3,
    VRI_COMMAND_BUFFER_STATE_PENDING = 4,
    VRI_COMMAND_BUFFER_STATE_MAX_ENUM = 0x7FFFFFFF
} VriCommandBufferState;

typedef enum {
    VRI_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
    VRI_PRIMITIVE_TOPOLOGY_LINE_LIST = 1,
    VRI_PRIMITIVE_TOPOLOGY_LINE_STRIP = 2,
    VRI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3,
    VRI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 4,
    VRI_PRIMITIVE_TOPOLOGY_COUNT,
    VRI_PRIMITIVE_TOPOLOGY_MAX_ENUM = 0x7FFFFFFF
} VriPrimitiveTopology;

typedef enum {
    VRI_FRONT_FACE_COUNTER_CLOCKWISE = 0,
    VRI_FRONT_FACE_CLOCKWISE = 1,
    VRI_FRONT_FACE_MAX_ENUM = 0x7FFFFFFF
} VriFrontFace;

typedef enum {
    VRI_FILL_MODE_FILL = 0,
    VRI_FILL_MODE_LINE = 1,
    VRI_FILL_MODE_POINT = 2,
    VRI_FILL_MODE_MAX_ENUM = 0x7FFFFFFF
} VriFillMode;

typedef enum {
    VRI_CULL_MODE_NONE = 0,
    VRI_CULL_MODE_FRONT = 1,
    VRI_CULL_MODE_BACK = 2,
    VRI_CULL_MODE_COUNT,
    VRI_CULL_MODE_MAX_ENUM = 0x7FFFFFFF
} VriCullMode;

typedef enum {
    VRI_QUEUE_FLAG_BIT_GRAPHICS = 1 << 0,
    VRI_QUEUE_FLAG_BIT_COMPUTE = 1 << 1,
    VRI_QUEUE_FLAG_BIT_TRANSFER = 1 << 2,
    VRI_QUEUE_FLAG_BIT_PRESENT = 1 << 3
} VriQueueFlagBits;
typedef VriFlags VriQueueFlags;

typedef enum {
    VRI_TEXTURE_USAGE_BIT_NONE = 0,
    VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE = 1 << 0,
    VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE_STORAGE = 1 << 1,
    VRI_TEXTURE_USAGE_BIT_COLOR_ATTACHMENT = 1 << 2,
    VRI_TEXTURE_USAGE_BIT_DEPTH_STENCIL_ATTACHMENT = 1 << 3,
    VRI_TEXTURE_USAGE_BIT_SHADING_RATE_ATTACHMENT = 1 << 4,
} VriTextureUsageBits;
typedef VriFlags VriTextureUsage;

typedef enum {
    VRI_SWAPCHAIN_FLAG_BIT_NONE = 0,
    VRI_SWAPCHAIN_FLAG_BIT_VSYNC = 1 << 0,
    VRI_SWAPCHAIN_FLAG_BIT_WAITABLE = 1 << 1,
    VRI_SWAPCHAIN_FLAG_BIT_ALLOW_TEARING = 1 << 2,
} VriSwapchainFlagBits;
typedef VriFlags VriSwapchainFlags;

typedef enum {
    VRI_COMMAND_POOL_FLAG_BIT_NONE = 0,
    VRI_COMMAND_POOL_FLAG_BIT_RESET_COMMAND_BUFFER = 1 << 0,
    VRI_COMMAND_POOL_FLAG_BIT_TRANSIENT = 1 << 1,
    VRI_COMMAND_POOL_FLAG_BIT_PROTECTED = 1 << 2
} VriCommandPoolFlagBits;
typedef VriFlags VriCommandPoolFlags;

typedef enum {
    VRI_COMMAND_POOL_RESET_FLAG_BIT_NONE = 0,
    VRI_COMMAND_POOL_RESET_FLAG_BIT_RELEASE_RESOURCES = 1 << 0
} VriCommandPoolResetFlagBits;
typedef VriFlags VriCommandPoolResetFlags;

typedef enum {
    VRI_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 1 << 0,
    VRI_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = 1 << 1
} VriCommandBufferUsageBits;
typedef VriFlags VriCommandBufferUsage;

typedef enum {
    VRI_SHADER_STAGE_FLAG_BIT_NONE = 0,
    VRI_SHADER_STAGE_FLAG_BIT_VERTEX = 1 << 0,
    VRI_SHADER_STAGE_FLAG_BIT_TESSELATION_CONTROL = 1 << 1,
    VRI_SHADER_STAGE_FLAG_BIT_TESSELATION_EVALUATION = 1 << 2,
    VRI_SHADER_STAGE_FLAG_BIT_GEOMETRY = 1 << 3,
    VRI_SHADER_STAGE_FLAG_BIT_FRAGMENT = 1 << 4,
    VRI_SHADER_STAGE_FLAG_BIT_COMPUTE = 1 << 5
} VriShaderStageFlagBits;
typedef VriFlags VriShaderStageFlags;

typedef void (*PFN_VriMessageCallback)(
    VriMessageSeverity severity,
    const char        *p_message);

typedef void *(*PFN_VriAllocationFunction)(
    size_t size,
    size_t alignment);

typedef void (*PFN_VriFreeFunction)(
    void  *p_memory,
    size_t size,
    size_t alignment);

typedef struct {
    PFN_VriMessageCallback pfn_message_callback;
} VriDebugCallback;

typedef struct {
    PFN_VriAllocationFunction pfn_allocate;
    PFN_VriFreeFunction       pfn_free;
} VriAllocationCallback;

typedef struct {
    VriQueueType type;
    uint32_t     count;
    const float *priorities;
} VriQueueDesc;

typedef struct {
    VriQueueFlags flags;
    uint32_t      count;
} VriQueueFamilyProps;

typedef struct {
    uint64_t     luid;
    uint32_t     device_id;
    VriGpuVendor vendor;
    VriGpuType   type;
    uint64_t     vram;
    uint32_t     shared_system_memory;
    uint32_t     queue_count[VRI_QUEUE_TYPE_COUNT];
} VriAdapterProps;

typedef struct {
    VriBackend             backend;
    const VriAdapterProps *p_adapter_props;
    const VriQueueDesc    *p_queue_descs;
    uint32_t               queue_desc_count;
    VriDebugCallback       debug_callback;
    VriAllocationCallback  allocation_callback;
    VriBool                enable_api_validation;
} VriDeviceDesc;

typedef struct {
    VriTextureType  type;
    VriFormat       format;
    uint32_t        width;
    uint32_t        height;
    uint32_t        depth;
    VriTextureUsage usage;
    uint32_t        sample_count;
    uint32_t        mip_count;
    uint32_t        layer_count;
} VriTextureDesc;

typedef struct {
    void *p_hwnd;
    void *p_connection;
    void *p_window;
    void *p_display;
    void *p_surface;
    void *p_caMetalLayer;
} VriWindowDesc;

typedef struct {
    VriWindowDesc       *p_window_desc;
    uint32_t             width;
    uint32_t             height;
    VriFormat            format;
    VriColorSpace        color_space;
    VriSwapchainFlagBits flags;
    uint8_t              texture_count;
    uint8_t              frames_in_flight;
} VriSwapchainDesc;

typedef struct {
    VriQueueType        queue_type;
    VriCommandPoolFlags flags;
} VriCommandPoolDesc;

typedef struct {
    VriCommandPool command_pool;
    uint32_t       command_buffer_count;
} VriCommandBufferAllocateDesc;

typedef struct {
    VriCommandBufferUsage usage;
} VriCommandBufferBeginDesc;

typedef struct {
    VriFence fence;
    uint64_t value;
} VriFenceWaitDesc;
typedef VriFenceWaitDesc VriFenceSignalDesc;

typedef struct {
    VriPrimitiveTopology topology;
} VriInputAssemblyDesc;

typedef struct {
    VriFillMode  fill_mode;
    VriCullMode  cull_mode;
    VriFrontFace front_face;
    VriBool      depth_clamp_enable;
} VriRasterizationStateDesc;

typedef struct {
    int placeholder;
} VriBlendStateDesc;

typedef struct {
    int placeholder;
} VriPipelineLayoutDesc;

typedef struct {
    VriShaderStageFlagBits stage;
    size_t                 size;
    const void            *p_bytecode;
    const char            *p_entry_point;
} VriShaderModuleDesc;

typedef struct {
    const VriPipelineLayout          pipeline_layout;
    VriShaderModuleDesc             *p_shaders;
    uint32_t                         shader_count;
    const VriInputAssemblyDesc      *p_input_assembly_state;
    const VriRasterizationStateDesc *p_rasterization_state;
} VriGraphicsPipelineDesc;

typedef struct {
    VriShaderModuleDesc *p_shader;
} VriComputePipelineDesc;

typedef struct {
    const VriCommandBuffer   *p_command_buffers;
    uint32_t                  command_buffer_count;
    const VriFenceWaitDesc   *p_fences_wait;
    uint32_t                  fence_wait_count;
    const VriFenceSignalDesc *p_fences_signal;
    uint32_t                  fence_signal_count;
} VriQueueSubmitDesc;

typedef struct {
    const VriSwapchain *p_swapchains;
    uint32_t            swapchain_count;
    uint32_t           *p_image_indices; // Which image in the swapchain to present
    VriFence           *p_wait_fences;
    uint64_t           *p_wait_values; // Timeline values to wait for
    uint32_t            wait_fence_count;
    VriResult          *p_results; // Per-swapchain results (can be NULL)
} VriQueuePresentDesc;

typedef void (*PFN_VriDeviceDestroy)(VriDevice device);
typedef VriResult (*PFN_VriCommandPoolCreate)(VriDevice device, const VriCommandPoolDesc *p_desc, VriCommandPool *p_command_pool);
typedef void (*PFN_VriCommandPoolDestroy)(VriDevice device, VriCommandPool command_pool);
typedef void (*PFN_VriCommandPoolReset)(VriDevice device, VriCommandPool command_pool, VriCommandPoolResetFlags flags);
typedef VriResult (*PFN_VriCommandBuffersAllocate)(VriDevice device, const VriCommandBufferAllocateDesc *p_desc, VriCommandBuffer *p_command_buffers);
typedef void (*PFN_VriCommandBuffersFree)(VriDevice device, VriCommandPool command_pool, uint32_t command_buffer_count, const VriCommandBuffer *p_command_buffers);
typedef void (*PFN_VriCmdBindPipeline)(VriCommandBuffer command_buffer, VriPipeline pipeline);
typedef VriResult (*PFN_VriShaderModuleCreate)(VriDevice device, const VriShaderModuleDesc *p_desc, VriShaderModule *p_shader_module);
typedef VriResult (*PFN_VriPipelineLayoutCreate)(VriDevice device, const VriPipelineLayoutDesc *p_desc, VriPipelineLayout *p_pipeline_layout);
typedef VriResult (*PFN_VriPipelineCreateGraphics)(VriDevice device, const VriGraphicsPipelineDesc *p_desc, VriPipeline *p_pipeline);
typedef VriResult (*PFN_VriPipelineCreateCompute)(VriDevice device, const VriComputePipelineDesc *p_desc, VriPipeline *p_pipeline);
typedef VriResult (*PFN_VriTextureCreate)(VriDevice device, const VriTextureDesc *p_desc, VriTexture *p_texture);
typedef void (*PFN_VriTextureDestroy)(VriDevice device, VriTexture texture);
typedef VriResult (*PFN_VriFenceCreate)(VriDevice device, uint64_t initial_value, VriFence *p_fence);
typedef void (*PFN_VriFenceDestroy)(VriDevice device, VriFence fence);
typedef uint64_t (*PFN_VriFenceGetValue)(VriDevice device, VriFence fence);
typedef VriResult (*PFN_VriFencesWait)(VriDevice device, const VriFence *p_fences, const uint64_t *p_values, uint32_t fence_count, VriBool wait_all, uint64_t timeout_ns);
typedef VriResult (*PFN_VriSwapchainCreate)(VriDevice device, const VriSwapchainDesc *p_desc, VriSwapchain *p_swapchain);
typedef void (*PFN_VriSwapchainDestroy)(VriDevice device, VriSwapchain swapchain);
typedef VriResult (*PFN_VriSwapchainAcquireNextImage)(VriDevice device, VriSwapchain swapchain, VriFence fence, uint64_t signal_value, uint32_t *p_image_index);
typedef VriResult (*PFN_VriSwapchainPresent)(VriDevice device, VriSwapchain swapchain, VriFence fence);

VriResult vri_adapters_enumerate(
    VriAdapterProps *p_props,
    uint32_t        *p_props_count);

void vri_report_live_objects(
    void);

VriResult vri_device_create(
    const VriDeviceDesc *p_desc,
    VriDevice           *p_device);

void vri_device_destroy(
    VriDevice device);

void vri_device_get_queue(
    VriDevice    device,
    VriQueueType queue_type,
    uint32_t     queue_index,
    VriQueue    *p_queue);

VriResult vri_command_pool_create(
    VriDevice                 device,
    const VriCommandPoolDesc *p_desc,
    VriCommandPool           *p_command_pool);

void vri_command_pool_destroy(
    VriDevice      device,
    VriCommandPool command_pool);

void vri_command_pool_reset(
    VriDevice                device,
    VriCommandPool           command_pool,
    VriCommandPoolResetFlags flags);

VriResult vri_command_buffers_allocate(
    VriDevice                           device,
    const VriCommandBufferAllocateDesc *p_desc,
    VriCommandBuffer                   *p_command_buffers);

void vri_command_buffers_free(
    VriDevice               device,
    VriCommandPool          command_pool,
    uint32_t                command_buffer_count,
    const VriCommandBuffer *p_command_buffers);

void vri_cmd_bind_pipeline(
    VriCommandBuffer command_buffer,
    VriPipeline      pipeline);

VriResult vri_pipeline_layout_create(
    VriDevice                    device,
    const VriPipelineLayoutDesc *p_desc,
    VriPipelineLayout           *p_pipeline_layout);

VriResult vri_pipeline_create_graphics(
    VriDevice                      device,
    const VriGraphicsPipelineDesc *p_desc,
    VriPipeline                   *p_pipeline);

VriResult vri_pipeline_create_compute(
    VriDevice                     device,
    const VriComputePipelineDesc *p_desc,
    VriPipeline                  *p_pipeline);

VriResult vri_texture_create(
    VriDevice             device,
    const VriTextureDesc *p_desc,
    VriTexture           *p_texture);

void vri_texture_destroy(
    VriDevice  device,
    VriTexture texture);

VriResult vri_fence_create(
    VriDevice device,
    uint64_t  initial_value,
    VriFence *p_fence);

void vri_fence_destroy(
    VriDevice device,
    VriFence  fence);

uint64_t vri_fence_get_value(
    VriDevice device,
    VriFence  fence);

VriResult vri_fences_wait(
    VriDevice device,
    VriFence *p_fences,
    uint64_t *p_values,
    uint32_t  fence_count,
    VriBool   wait_all,
    uint64_t  timeout_ns);

VriResult vri_swapchain_create(
    VriDevice               device,
    const VriSwapchainDesc *p_desc,
    VriSwapchain           *p_swapchain);

void vri_swapchain_destroy(
    VriDevice    device,
    VriSwapchain swapchain);

VriResult vri_swapchain_acquire_next_image(
    VriDevice    device,
    VriSwapchain swapchain,
    VriFence     fence,
    uint64_t     signal_value,
    uint32_t    *p_image_index);

VriResult vri_swapchain_present(
    VriDevice    device,
    VriSwapchain swapchain,
    VriFence     fence);

typedef VriResult (*PFN_VriCommandBufferBegin)(VriCommandBuffer command_buffer, const VriCommandBufferBeginDesc *p_desc);
typedef VriResult (*PFN_VriCommandBufferEnd)(VriCommandBuffer command_buffer);
typedef VriResult (*PFN_VriCommandBufferReset)(VriCommandBuffer command_buffer);

VriResult vri_command_buffer_begin(
    VriCommandBuffer                 command_buffer,
    const VriCommandBufferBeginDesc *p_desc);

VriResult vri_command_buffer_end(
    VriCommandBuffer command_buffer);

VriResult vri_command_buffer_reset(
    VriCommandBuffer command_buffer);

typedef VriResult (*PFN_VriQueueSubmit)(VriQueue queue, const VriQueueSubmitDesc *p_submits, uint32_t submit_count);
typedef VriResult (*PFN_VriQueueWaitIdle)(VriQueue queue);
typedef VriResult (*PFN_VriQueuePresent)(VriQueue queue, const VriQueuePresentDesc *p_present);

VriResult vri_queue_submit(
    VriQueue                  queue,
    const VriQueueSubmitDesc *p_submits,
    uint32_t                  submit_count);

VriResult vri_queue_wait_idle(
    VriQueue queue);

VriResult vri_queue_present(
    VriQueue                   queue,
    const VriQueuePresentDesc *p_present);

#ifdef __cplusplus
}
#endif

#endif
