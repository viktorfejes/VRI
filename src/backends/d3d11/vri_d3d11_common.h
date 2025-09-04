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

const static D3D11_PRIMITIVE_TOPOLOGY d3d11_topologies[VRI_PRIMITIVE_TOPOLOGY_COUNT] = {
    [VRI_PRIMITIVE_TOPOLOGY_POINT_LIST] = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
    [VRI_PRIMITIVE_TOPOLOGY_LINE_LIST] = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
    [VRI_PRIMITIVE_TOPOLOGY_LINE_STRIP] = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
    [VRI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    [VRI_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
};

const static D3D11_CULL_MODE d3d11_cull_modes[VRI_CULL_MODE_COUNT] = {
    [VRI_CULL_MODE_NONE] = D3D11_CULL_NONE,
    [VRI_CULL_MODE_BACK] = D3D11_CULL_BACK,
    [VRI_CULL_MODE_FRONT] = D3D11_CULL_FRONT,
};

const static D3D11_COMPARISON_FUNC d3d11_compare_ops[VRI_COMPARE_COUNT] = {
    [VRI_COMPARE_NEVER] = D3D11_COMPARISON_NEVER,
    [VRI_COMPARE_LESS] = D3D11_COMPARISON_LESS,
    [VRI_COMPARE_EQUAL] = D3D11_COMPARISON_EQUAL,
    [VRI_COMPARE_LESS_EQUAL] = D3D11_COMPARISON_LESS_EQUAL,
    [VRI_COMPARE_GREATER] = D3D11_COMPARISON_GREATER,
    [VRI_COMPARE_NOT_EQUAL] = D3D11_COMPARISON_NOT_EQUAL,
    [VRI_COMPARE_GREATER_EQUAL] = D3D11_COMPARISON_GREATER_EQUAL,
    [VRI_COMPARE_ALWAYS] = D3D11_COMPARISON_ALWAYS,
};

const static D3D11_STENCIL_OP d3d11_stencil_ops[VRI_STENCIL_OP_COUNT] = {
    [VRI_STENCIL_OP_KEEP] = D3D11_STENCIL_OP_KEEP,
    [VRI_STENCIL_OP_ZERO] = D3D11_STENCIL_OP_ZERO,
    [VRI_STENCIL_OP_REPLACE] = D3D11_STENCIL_OP_REPLACE,
    [VRI_STENCIL_OP_INCR_CLAMP] = D3D11_STENCIL_OP_INCR_SAT,
    [VRI_STENCIL_OP_DECR_CLAMP] = D3D11_STENCIL_OP_DECR_SAT,
    [VRI_STENCIL_OP_INVERT] = D3D11_STENCIL_OP_INVERT,
    [VRI_STENCIL_OP_INCR_WRAP] = D3D11_STENCIL_OP_INCR,
    [VRI_STENCIL_OP_DECR_WRAP] = D3D11_STENCIL_OP_DECR,
};

const static D3D11_BLEND_OP d3d11_blend_ops[VRI_BLEND_OP_COUNT] = {
    [VRI_BLEND_OP_ADD] = D3D11_BLEND_OP_ADD,
    [VRI_BLEND_OP_SUBTRACT] = D3D11_BLEND_OP_SUBTRACT,
    [VRI_BLEND_OP_REVERSE_SUBTRACT] = D3D11_BLEND_OP_REV_SUBTRACT,
    [VRI_BLEND_OP_MIN] = D3D11_BLEND_OP_MIN,
    [VRI_BLEND_OP_MAX] = D3D11_BLEND_OP_MAX,
};

const static D3D11_BLEND d3d11_blend_factors[VRI_BLEND_COUNT] = {
    [VRI_BLEND_ZERO] = D3D11_BLEND_ZERO,
    [VRI_BLEND_ONE] = D3D11_BLEND_ONE,
    [VRI_BLEND_SRC_COLOR] = D3D11_BLEND_SRC_COLOR,
    [VRI_BLEND_ONE_MINUS_SRC_COLOR] = D3D11_BLEND_INV_SRC_COLOR,
    [VRI_BLEND_DST_COLOR] = D3D11_BLEND_DEST_COLOR,
    [VRI_BLEND_ONE_MINUS_DST_COLOR] = D3D11_BLEND_INV_DEST_COLOR,
    [VRI_BLEND_SRC_ALPHA] = D3D11_BLEND_SRC_ALPHA,
    [VRI_BLEND_ONE_MINUS_SRC_ALPHA] = D3D11_BLEND_INV_SRC_ALPHA,
    [VRI_BLEND_DST_ALPHA] = D3D11_BLEND_DEST_ALPHA,
    [VRI_BLEND_ONE_MINUS_DST_ALPHA] = D3D11_BLEND_INV_DEST_ALPHA,
    [VRI_BLEND_CONSTANT_COLOR] = D3D11_BLEND_BLEND_FACTOR,
    [VRI_BLEND_ONE_MINUS_CONSTANT_COLOR] = D3D11_BLEND_INV_BLEND_FACTOR,
    [VRI_BLEND_CONSTANT_ALPHA] = D3D11_BLEND_BLEND_FACTOR,
    [VRI_BLEND_ONE_MINUS_CONSTANT_ALPHA] = D3D11_BLEND_INV_BLEND_FACTOR,
    [VRI_BLEND_SRC_ALPHA_SATURATE] = D3D11_BLEND_SRC_ALPHA_SAT,
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

static inline D3D11_PRIMITIVE_TOPOLOGY vri_topology_to_d3d11_topology(VriPrimitiveTopology topology) {
    return d3d11_topologies[topology];
}

static inline D3D11_CULL_MODE vri_cull_mode_to_d3d11(VriCullMode cull_mode) {
    return d3d11_cull_modes[cull_mode];
}

static inline D3D11_COMPARISON_FUNC vri_compare_op_to_d3d11(VriCompareOp compare_op) {
    return d3d11_compare_ops[compare_op];
}

static inline D3D11_STENCIL_OP vri_stencil_op_to_d3d11(VriStencilOp stencil_op) {
    return d3d11_stencil_ops[stencil_op];
}

static inline D3D11_BLEND_OP vri_blend_op_to_d3d11(VriBlendOp blend_op) {
    return d3d11_blend_ops[blend_op];
}

static inline D3D11_BLEND vri_blend_factor_to_d3d11(VriBlendFactor blend_factor) {
    return d3d11_blend_factors[blend_factor];
}

#endif
