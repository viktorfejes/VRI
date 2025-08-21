#include "vri_d3d11_queue.h"

#define QUEUE_STRUCT_SIZE (sizeof(VriQueue))

VriResult d3d11_queue_create(VriDevice device, VriQueue *p_queue) {
    // Convinience assignment for the debug messages
    VriDebugCallback dbg = device->debug_callback;

    // Attempt to allocate the full internal struct
    size_t queue_size = QUEUE_STRUCT_SIZE;
    *p_queue = vri_object_allocate(device, &device->allocation_callback, queue_size, VRI_OBJECT_QUEUE);
    if (!*p_queue) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Allocation for queue struct failed.");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    // NOTE: For D3D11 Queues there won't be any backends as we are storing the parent device
    // in the base field so we can just reach back to that for the immediate context which we'll need.

    // TODO: Fill up dispatch table!

    return VRI_SUCCESS;
}

void d3d11_queue_destroy(VriDevice device, VriQueue queue) {
    if (queue) {
        device->allocation_callback.pfn_free(queue, QUEUE_STRUCT_SIZE, 8);
    }
}
