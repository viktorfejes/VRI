#ifndef VRI_INTERNAL_H
#define VRI_INTERNAL_H

#include "vri/vri.h"

typedef enum {
    VRI_OBJECT_DEVICE,
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
    PFN_VriTextureCreate             pfn_texture_create;
    PFN_VriTextureDestroy            pfn_texture_destroy;
    PFN_VriFenceCreate               pfn_fence_create;
    PFN_VriFenceDestroy              pfn_fence_destroy;
    PFN_VriSwapchainCreate           pfn_swapchain_create;
    PFN_VriSwapchainDestroy          pfn_swapchain_destroy;
    PFN_VriSwapchainAcquireNextImage pfn_swapchain_acquire_next_image;
    PFN_VriSwapchainPresent          pfn_swapchain_present;
} VriDeviceDispatchTable;

struct VriDevice_T {
    VriObjectBase           base;
    VriDeviceDispatchTable *p_dispatch;
    VriBackend              backend;
    VriAllocationCallback   allocation_callback;
    VriDebugCallback        debug_callback;
    VriAdapterDesc          adapter_desc;
    void                   *p_backend_data;
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

void  vri_object_base_init(struct VriDevice_T *device, VriObjectBase *base, VriObjectType type);
void *vri_object_allocate(struct VriDevice_T *device, const VriAllocationCallback *alloc, size_t size, VriObjectType type);

#endif
