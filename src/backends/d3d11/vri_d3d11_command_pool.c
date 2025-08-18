#include "vri_d3d11_command_pool.h"

static VriResult d3d11_command_pool_create(VriDevice device, const VriCommandPoolDesc *p_desc, VriCommandPool *p_command_pool);
static void      d3d11_command_pool_destroy(VriDevice device, VriCommandPool command_pool);
static void      d3d11_command_pool_reset(VriDevice device, VriCommandPool command_pool, VriCommandPoolResetFlags flags);

void d3d11_register_command_pool_functions(VriDeviceDispatchTable *table) {
    table->pfn_command_pool_create = d3d11_command_pool_create;
    table->pfn_command_pool_destroy = d3d11_command_pool_destroy;
    table->pfn_command_pool_reset = d3d11_command_pool_reset;
}

static VriResult d3d11_command_pool_create(VriDevice device, const VriCommandPoolDesc *p_desc, VriCommandPool *p_command_pool) {
    (void)p_desc;
    VriDebugCallback dbg = device->debug_callback;

    size_t alloc_size = sizeof(struct VriCommandPool_T);
    *p_command_pool = vri_object_allocate(device, &device->allocation_callback, alloc_size, VRI_OBJECT_COMMAND_POOL);
    if (!*p_command_pool) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Couldn't allocate command pool");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    return VRI_SUCCESS;
}

static void d3d11_command_pool_destroy(VriDevice device, VriCommandPool command_pool) {
    if (command_pool) {
        size_t alloc_size = sizeof(struct VriCommandPool_T);
        device->allocation_callback.pfn_free(command_pool, alloc_size, 8);
    }
}

static void d3d11_command_pool_reset(VriDevice device, VriCommandPool command_pool, VriCommandPoolResetFlags flags) {
    // NOP
    (void)device;
    (void)command_pool;
    (void)flags;
}
