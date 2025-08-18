#ifndef VRI_BACKEND_D3D11_COMMON_H
#define VRI_BACKEND_D3D11_COMMON_H

// d3d11_common.h
// Internal COM utility macros and helpers for the D3D11 backend.
// Not part of the public RHI API.

#include "../../core/vri_internal.h"
#include <d3d11_4.h>

#define COM_RELEASE(obj) ((obj)->lpVtbl->Release((obj)))
#define COM_SAFE_RELEASE(obj)              \
    do {                                   \
        if ((obj)) {                       \
            (obj)->lpVtbl->Release((obj)); \
            (obj) = NULL;                  \
        }                                  \
    } while (0)

#define COM_IID_PPV_ARGS(type, ppType) \
    &IID_##type, (void **)(ppType)

#define COM_QUERY_INTERFACE(obj, type, out) \
    (obj)->lpVtbl->QueryInterface((obj), IID_PPV_ARGS_C(type, out))

typedef struct {
    DXGI_FORMAT typeless;
    DXGI_FORMAT typed;
} VriDXGIFormatPair;

const static VriDXGIFormatPair vri_to_dxgi[VRI_FORMAT_COUNT] = {
    [VRI_FORMAT_R8G8B8A8_UNORM] = {
        .typeless = DXGI_FORMAT_R8G8B8A8_TYPELESS,
        .typed = DXGI_FORMAT_R8G8B8A8_UNORM},
};

const static VriFormat dxgi_to_vri[] = {
    [DXGI_FORMAT_R8G8B8A8_UNORM] = VRI_FORMAT_R8G8B8A8_UNORM,
};

// Best attempt at translation
const static DXGI_COLOR_SPACE_TYPE vri_to_dxgi_color_space[VRI_COLORSPACE_COUNT] = {
    [VRI_COLORSPACE_SRGB_NONLINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
    [VRI_COLORSPACE_SRGB_LINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
    [VRI_COLORSPACE_BT709_NONLINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
    [VRI_COLORSPACE_BT709_LINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
    [VRI_COLORSPACE_P3_NONLINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,
    [VRI_COLORSPACE_P3_LINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
    [VRI_COLORSPACE_BT2020_NONLINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,
    [VRI_COLORSPACE_BT2020_LINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
    [VRI_COLORSPACE_HDR10_ST2084] = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,
    [VRI_COLORSPACE_HDR10_HLG] = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,
    [VRI_COLORSPACE_EXTENDED_SRGB_LINEAR] = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
};

// NOTE: Leaving this here for now in case I want to use it in the future
static inline void COM_safe_release(IUnknown **pp_obj) {
    if (pp_obj && *pp_obj) {
        (*pp_obj)->lpVtbl->Release(*pp_obj);
        *pp_obj = NULL;
    }
}

static inline const VriDXGIFormatPair *vri_to_dxgi_format(VriFormat format) {
    return &vri_to_dxgi[format];
}

static inline const VriFormat *vri_dxgi_format_to_vri(DXGI_FORMAT format) {
    return &dxgi_to_vri[format];
}

static inline const DXGI_COLOR_SPACE_TYPE *vri_color_space_to_dxgi(VriColorSpace color_space) {
    return &vri_to_dxgi_color_space[color_space];
}

#endif
