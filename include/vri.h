#pragma once

#include "vri_types.h"

VriResult vri_adapters_enumerate(vri_adapter_desc_t *adapter_descs, uint32_t *adapter_desc_count);

VriResult vri_device_create(const vri_device_desc_t *device_desc, VriDevice *device);
void vri_device_destroy(VriDevice *device);

VriResult vri_swapchain_create(VriDevice device, const vri_swapchain_desc_t *swapchain_desc, vri_swapchain_t **out_swapchain);
void vri_swapchain_destroy(VriDevice device, vri_swapchain_t *swapchain);

VriResult vri_interface_get_core(const VriDevice device, vri_interface_core_t *interface);
VriResult vri_interface_get_swapchain(const VriDevice device, vri_interface_swapchain_t *interface);

// D3D exclusive
void vri_report_live_objects(void);
