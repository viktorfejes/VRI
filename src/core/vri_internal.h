#ifndef VRI_INTERNAL_H
#define VRI_INTERNAL_H

#include "vri/vri.h"

#define MAX_QUEUES_PER_TYPE 4

typedef enum {
    VRI_OBJECT_DEVICE,
    VRI_OBJECT_COMMAND_POOL,
    VRI_OBJECT_COMMAND_BUFFER,
    VRI_OBJECT_QUEUE,
    VRI_OBJECT_TEXTURE,
    VRI_OBJECT_FENCE,
    VRI_OBJECT_SWAPCHAIN,
} VriObjectType;

typedef struct {
    VriObjectType       type;
    struct VriDevice_T *p_device;
} VriObjectBase;

typedef struct {
    PFN_VriDeviceDestroy             pfn_device_destroy;
    PFN_VriCommandPoolCreate         pfn_command_pool_create;
    PFN_VriCommandPoolDestroy        pfn_command_pool_destroy;
    PFN_VriCommandPoolReset          pfn_command_pool_reset;
    PFN_VriCommandBuffersAllocate    pfn_command_buffers_allocate;
    PFN_VriCommandBuffersFree        pfn_command_buffers_free;
    PFN_VriTextureCreate             pfn_texture_create;
    PFN_VriTextureDestroy            pfn_texture_destroy;
    PFN_VriFenceCreate               pfn_fence_create;
    PFN_VriFenceDestroy              pfn_fence_destroy;
    PFN_VriSwapchainCreate           pfn_swapchain_create;
    PFN_VriSwapchainDestroy          pfn_swapchain_destroy;
    PFN_VriSwapchainAcquireNextImage pfn_swapchain_acquire_next_image;
    PFN_VriSwapchainPresent          pfn_swapchain_present;
} VriDeviceDispatchTable;

typedef struct {
    PFN_VriCommandBufferBegin pfn_command_buffer_begin;
    PFN_VriCommandBufferEnd   pfn_command_buffer_end;
    PFN_VriCommandBufferReset pfn_command_buffer_reset;
} VriCommandBufferDispatchTable;

typedef struct {
    PFN_VriQueueSubmit  pfn_queue_submit;
    PFN_VriQueuePresent pfn_queue_present;
} VriQueueDispatchTable;

struct VriDevice_T {
    VriObjectBase          base;
    VriDeviceDispatchTable dispatch;
    VriBackend             backend;
    VriAllocationCallback  allocation_callback;
    VriDebugCallback       debug_callback;
    VriAdapterProps        adapter_props;
    VriQueue               queues[VRI_QUEUE_TYPE_COUNT][MAX_QUEUES_PER_TYPE];
    uint32_t               queue_counts[VRI_QUEUE_TYPE_COUNT];
    void                  *p_backend_data;
};

struct VriCommandPool_T {
    VriObjectBase base;
    void         *p_backend_data;
};

struct VriCommandBuffer_T {
    VriObjectBase                 base;
    VriCommandBufferDispatchTable dispatch;
    VriCommandBufferState         state;
    void                         *p_backend_data;
};

struct VriQueue_T {
    VriObjectBase         base;
    VriQueueDispatchTable dispatch;
    VriQueueType          type;
    void                 *p_backend_data;
};

struct VriTexture_T {
    VriObjectBase  base;
    VriTextureDesc desc;
    void          *p_backend_data;
};

struct VriFence_T {
    VriObjectBase base;
    void         *p_backend_data;
};

struct VriSwapchain_T {
    VriObjectBase base;
    void         *p_backend_data;
};

void  vri_object_base_init(VriDevice device, VriObjectBase *base, VriObjectType type);
void *vri_object_allocate(VriDevice device, const VriAllocationCallback *alloc, size_t size, VriObjectType type);

#endif
