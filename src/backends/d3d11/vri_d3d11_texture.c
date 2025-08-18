#include "vri_d3d11_texture.h"

#include "vri_d3d11_common.h"
#include "vri_d3d11_device.h"

static VriResult d3d11_texture_create(VriDevice device, const VriTextureDesc *p_desc, VriTexture *p_texture);
static VriBool   fill_texture_details_from_resource(VriTexture texture, ID3D11Resource *resource);
static size_t    get_texture_size(void);

void d3d11_register_texture_functions(VriDeviceDispatchTable *table) {
    table->pfn_texture_create = d3d11_texture_create;
    table->pfn_texture_destroy = d3d11_texture_destroy;
}

static VriResult d3d11_texture_create(VriDevice device, const VriTextureDesc *p_desc, VriTexture *p_texture) {
    ID3D11Device5   *d3d11_device = ((VriD3D11Device *)device->p_backend_data)->p_device;
    VriDebugCallback dbg = device->debug_callback;

    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    uint32_t    cpu_access_flags = 0;
    switch (p_desc->usage) {
        case VRI_MEMORY_TYPE_GPU_ONLY:
            break;
        case VRI_MEMORY_TYPE_UPLOAD:
            usage = D3D11_USAGE_DYNAMIC;
            cpu_access_flags = D3D11_CPU_ACCESS_WRITE;
            break;
        case VRI_MEMORY_TYPE_READBACK:
            usage = D3D11_USAGE_STAGING;
            cpu_access_flags = D3D11_CPU_ACCESS_READ;
            break;
    }

    uint32_t bind_flags = 0;
    if (p_desc->usage & VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE)
        bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    if (p_desc->usage & VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE_STORAGE)
        bind_flags |= D3D11_BIND_UNORDERED_ACCESS;
    if (p_desc->usage & VRI_TEXTURE_USAGE_BIT_COLOR_ATTACHMENT)
        bind_flags |= D3D11_BIND_RENDER_TARGET;
    if (p_desc->usage & VRI_TEXTURE_USAGE_BIT_DEPTH_STENCIL_ATTACHMENT)
        bind_flags |= D3D11_BIND_DEPTH_STENCIL;

    DXGI_FORMAT format = vri_to_dxgi_format(p_desc->format)->typeless;

    HRESULT         hr = E_FAIL;
    ID3D11Resource *texture_res = NULL;
    switch (p_desc->type) {
        case VRI_TEXTURE_TYPE_TEXTURE_1D: {
            D3D11_TEXTURE1D_DESC desc = {
                .Width = p_desc->width,
                .Format = format,
                .MipLevels = p_desc->mip_count,
                .ArraySize = p_desc->layer_count,
                .Usage = usage,
                .CPUAccessFlags = cpu_access_flags,
                .BindFlags = bind_flags};

            hr = d3d11_device->lpVtbl->CreateTexture1D(d3d11_device, &desc, NULL, (ID3D11Texture1D **)&texture_res);
        } break;

        case VRI_TEXTURE_TYPE_TEXTURE_2D: {
            D3D11_TEXTURE2D_DESC desc = {
                .Width = p_desc->width,
                .Height = p_desc->height,
                .Format = format,
                .MipLevels = p_desc->mip_count,
                .ArraySize = p_desc->layer_count,
                .SampleDesc.Count = p_desc->sample_count,
                .Usage = usage,
                .CPUAccessFlags = cpu_access_flags,
                .BindFlags = bind_flags};

            hr = d3d11_device->lpVtbl->CreateTexture2D(d3d11_device, &desc, NULL, (ID3D11Texture2D **)&texture_res);
        } break;

        case VRI_TEXTURE_TYPE_TEXTURE_3D: {
            D3D11_TEXTURE3D_DESC desc = {
                .Width = p_desc->width,
                .Height = p_desc->height,
                .Depth = p_desc->depth,
                .Format = format,
                .MipLevels = p_desc->mip_count,
                .Usage = usage,
                .CPUAccessFlags = cpu_access_flags,
                .BindFlags = bind_flags};

            hr = d3d11_device->lpVtbl->CreateTexture3D(d3d11_device, &desc, NULL, (ID3D11Texture3D **)&texture_res);
        } break;

        default:
            dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Invalid texture type provided.");
            return VRI_ERROR_INVALID_API_USAGE;
    }

    if (FAILED(hr)) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create D3D11 Texture resource.");
        return (hr == E_OUTOFMEMORY) ? VRI_ERROR_OUT_OF_MEMORY : VRI_ERROR_SYSTEM_FAILURE;
    }

    // Allocate the new texture struct
    size_t tex_size = get_texture_size();
    *p_texture = vri_object_allocate(device, &device->allocation_callback, tex_size, VRI_OBJECT_TEXTURE);
    if (!*p_texture) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Couldn't allocate memory for Texture struct");
        texture_res->lpVtbl->Release(texture_res);
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    // Fill out the newly allocated OUT texture struct
    VriTextureDesc *tex_desc = &(*p_texture)->desc;
    tex_desc->width = p_desc->width;
    tex_desc->height = p_desc->height;
    tex_desc->depth = p_desc->depth;
    tex_desc->format = p_desc->format;
    tex_desc->mip_count = p_desc->mip_count;
    tex_desc->layer_count = p_desc->layer_count;

    // Add the internal data, as well
    (*p_texture)->p_backend_data = *p_texture + 1;
    VriD3D11Texture *internal_tex = (*p_texture)->p_backend_data;
    internal_tex->p_resource = texture_res;

    return VRI_SUCCESS;
}

VriResult d3d11_texture_create_from_resource(VriDevice device, ID3D11Resource **resource, VriTexture *p_texture) {
    VriDebugCallback dbg = device->debug_callback;

    // Allocate the new texture struct
    size_t tex_size = get_texture_size();
    *p_texture = vri_object_allocate(device, &device->allocation_callback, tex_size, VRI_OBJECT_TEXTURE);
    if (!*p_texture) {
        dbg.pfn_message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Couldn't allocate memory for Texture struct");
        return VRI_ERROR_OUT_OF_MEMORY;
    }

    // Add the internal data
    (*p_texture)->p_backend_data = *p_texture + 1;
    VriD3D11Texture *internal_tex = (*p_texture)->p_backend_data;

    // Attempt to fill out the details of the texture from the resource
    if (!fill_texture_details_from_resource(*p_texture, *resource)) {
        device->allocation_callback.pfn_free(*p_texture, tex_size, 8);
        return VRI_ERROR_INVALID_API_USAGE;
    }

    // Store the resource and take ownership
    internal_tex->p_resource = *resource;
    *resource = NULL;

    return VRI_SUCCESS;
}

void d3d11_texture_destroy(VriDevice device, VriTexture p_texture) {
    if (p_texture) {
        VriD3D11Texture *internal = p_texture->p_backend_data;

        if (internal) {
            COM_SAFE_RELEASE(internal->p_resource);
        }

        size_t tex_size = get_texture_size();
        device->allocation_callback.pfn_free(p_texture, tex_size, 8);
    }
}

static VriBool fill_texture_details_from_resource(VriTexture texture, ID3D11Resource *resource) {
    VriTextureDesc *texture_desc = &texture->desc;

    D3D11_RESOURCE_DIMENSION type = {0};
    resource->lpVtbl->GetType(resource, &type);

    uint32_t bind_flags = 0;
    if (type == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
        ID3D11Texture1D     *t = (ID3D11Texture1D *)resource;
        D3D11_TEXTURE1D_DESC desc = {0};
        t->lpVtbl->GetDesc(t, &desc);

        bind_flags = desc.BindFlags;

        texture_desc->type = VRI_TEXTURE_TYPE_TEXTURE_1D;
        texture_desc->format = *vri_dxgi_format_to_vri(desc.Format);
        texture_desc->width = desc.Width;
        texture_desc->height = 1;
        texture_desc->depth = 1;
        texture_desc->mip_count = desc.MipLevels;
        texture_desc->layer_count = desc.ArraySize;
        texture_desc->sample_count = 1;
    } else if (type == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        ID3D11Texture2D     *t = (ID3D11Texture2D *)resource;
        D3D11_TEXTURE2D_DESC desc = {0};
        t->lpVtbl->GetDesc(t, &desc);

        bind_flags = desc.BindFlags;

        texture_desc->type = VRI_TEXTURE_TYPE_TEXTURE_2D;
        texture_desc->format = *vri_dxgi_format_to_vri(desc.Format);
        texture_desc->width = desc.Width;
        texture_desc->height = desc.Height;
        texture_desc->depth = 1;
        texture_desc->mip_count = desc.MipLevels;
        texture_desc->layer_count = desc.ArraySize;
        texture_desc->sample_count = desc.SampleDesc.Count;
    } else if (type == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        ID3D11Texture3D     *t = (ID3D11Texture3D *)resource;
        D3D11_TEXTURE3D_DESC desc = {0};
        t->lpVtbl->GetDesc(t, &desc);

        bind_flags = desc.BindFlags;

        texture_desc->type = VRI_TEXTURE_TYPE_TEXTURE_3D;
        texture_desc->format = *vri_dxgi_format_to_vri(desc.Format);
        texture_desc->width = desc.Width;
        texture_desc->height = desc.Height;
        texture_desc->depth = desc.Depth;
        texture_desc->mip_count = desc.MipLevels;
        texture_desc->layer_count = 1;
        texture_desc->sample_count = 1;
    } else {
        // Something went wrong with getting the resource type
        return false;
    }

    if (bind_flags & D3D11_BIND_SHADER_RESOURCE)
        texture_desc->usage |= VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE;
    if (bind_flags & D3D11_BIND_UNORDERED_ACCESS)
        texture_desc->usage |= VRI_TEXTURE_USAGE_BIT_SHADER_RESOURCE_STORAGE;
    if (bind_flags & D3D11_BIND_RENDER_TARGET)
        texture_desc->usage |= VRI_TEXTURE_USAGE_BIT_COLOR_ATTACHMENT;
    if (bind_flags & D3D11_BIND_DEPTH_STENCIL)
        texture_desc->usage |= VRI_TEXTURE_USAGE_BIT_DEPTH_STENCIL_ATTACHMENT;

    return true;
}

static size_t get_texture_size(void) {
    return sizeof(struct VriTexture_T) + // Base device size
           sizeof(VriD3D11Texture);      // Internal backend size
}
