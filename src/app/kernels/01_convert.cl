#include "common.cl"
#include "colorspaces.cl"

__kernel void convertSource_RGBA_8888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    float4 rgba_in = (float4)(in_src[gid * 4 + 2] / 255.f,
                              in_src[gid * 4 + 3] / 255.f,
                              in_src[gid * 4 + 0] / 255.f,
                              in_src[gid * 4 + 1] / 255.f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_RGBX_8888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    float4 rgba_in = (float4)(in_src[gid * 4 + 0] / 255.f,
                              in_src[gid * 4 + 1] / 255.f,
                              in_src[gid * 4 + 2] / 255.f,
                              1.0f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_BGRA_8888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    float4 rgba_in = (float4)(in_src[gid * 4 + 2] / 255.f,
                              in_src[gid * 4 + 1] / 255.f,
                              in_src[gid * 4 + 0] / 255.f,
                              in_src[gid * 4 + 3] / 255.f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_BGRX_8888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);
    if (gid >= width * height)
        return;
    float4 rgba_in        = (float4)(in_src[gid * 4 + 0] / 255.f,
                              in_src[gid * 4 + 1] / 255.f,
                              in_src[gid * 4 + 2] / 255.f,
                              1.0f);
    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_ARGB_8888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {

    size_t gid = get_global_id(0);
    if (gid >= width * height)
        return;

    float4 rgba_in = (float4)(in_src[gid * 4 + 1] / 255.f,
                              in_src[gid * 4 + 2] / 255.f,
                              in_src[gid * 4 + 3] / 255.f,
                              in_src[gid * 4 + 0] / 255.f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_RGB_888(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {

    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    float4 rgba_in = (float4)(in_src[gid * 3 + 0] / 255.f,
                              in_src[gid * 3 + 1] / 255.f,
                              in_src[gid * 3 + 2] / 255.f,
                              1.0f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_BGR_888_InvY(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);
    if (gid >= width * height)
        return;

    int2 coords = coordsVecFromIndex(gid, width);
    uint bgrPB  = indexFromCoords(coords.x, height - coords.y - 1, width) * 3;

    float4 rgba_in = (float4)(in_src[bgrPB + 2] / 255.f,
                              in_src[bgrPB + 1] / 255.f,
                              in_src[bgrPB + 0] / 255.f,
                              1.0f);

    float3 rgb_linear     = eotf3(rgba_in.xyz, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgb_709        = (float4)(oetf3(rgb_709_linear, CS_BT709), rgba_in.w);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgb_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgb_709.x;
    out_rgba[gid * 4 + 1] = rgb_709.y;
    out_rgba[gid * 4 + 2] = rgb_709.z;
    out_rgba[gid * 4 + 3] = rgb_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgb_709);
}

__kernel void convertSource_UYVY_422(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    uchar y = in_src[gid * 2 + 1];
    uchar u = (gid % 2 == 0) ? in_src[gid * 2] : in_src[gid * 2 - 2];
    uchar v = (gid % 2 == 0) ? in_src[gid * 2 + 2] : in_src[gid * 2];

    float3 yuv_in = (float3)(y / 255.f, u / 255.f, v / 255.f);

    float3 rgb_in         = yuv_limited_to_RGB(yuv_in, src_colorspace);
    float3 rgb_linear     = eotf3(rgb_in, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgba_709       = (float4)(oetf3(rgb_709_linear, CS_BT709), 1.0f);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgba_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgba_709.x;
    out_rgba[gid * 4 + 1] = rgba_709.y;
    out_rgba[gid * 4 + 2] = rgba_709.z;
    out_rgba[gid * 4 + 3] = rgba_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgba_709);
}

__kernel void convertSource_YUYV_422(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    uchar y = in_src[gid * 2 + 0];
    uchar u = (gid % 2 == 0) ? in_src[gid * 2 + 1] : in_src[gid * 2 - 1];
    uchar v = (gid % 2 == 0) ? in_src[gid * 2 + 3] : in_src[gid * 2 + 1];

    float3 yuv_in = (float3)(y / 255.f, u / 255.f, v / 255.f);

    float3 rgb_in         = yuv_limited_to_RGB(yuv_in, src_colorspace);
    float3 rgb_linear     = eotf3(rgb_in, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgba_709       = (float4)(oetf3(rgb_709_linear, CS_BT709), 1.0f);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgba_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgba_709.x;
    out_rgba[gid * 4 + 1] = rgba_709.y;
    out_rgba[gid * 4 + 2] = rgba_709.z;
    out_rgba[gid * 4 + 3] = rgba_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgba_709);
}

__kernel void convertSource_NV12(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    uint uvStart = width * height;

    uchar y = in_src[gid];

    int2 uvCoords = coordsVecFromIndex(gid, width);
    uint uvX      = uvCoords.x / 2; // 4:2:0
    uint uvY      = uvCoords.y / 2;

    uint pUV = indexFromCoords(uvX, uvY, width / 2);

    uchar u = in_src[uvStart + pUV * 2];
    uchar v = in_src[uvStart + pUV * 2 + 1];

    float3 yuv_in         = (float3)(y / 255.f, u / 255.f, v / 255.f);
    float3 rgb_in         = yuv_limited_to_RGB(yuv_in, src_colorspace);
    float3 rgb_linear     = eotf3(rgb_in, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgba_709       = (float4)(oetf3(rgb_709_linear, CS_BT709), 1.0f);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgba_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgba_709.x;
    out_rgba[gid * 4 + 1] = rgba_709.y;
    out_rgba[gid * 4 + 2] = rgba_709.z;
    out_rgba[gid * 4 + 3] = rgba_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgba_709);
}

__kernel void convertSource_UYVA_4224(
    __global const uchar* in_src,
    write_only image2d_t  out_img_rgba,
    __global float*       out_rgba,
    __global float*       out_yuv,
    __global float*       out_xyz,
    uint                  width,
    uint                  height,
    uint                  stride,
    colorspace_t          src_colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    uchar y = in_src[gid * 2 + 1];
    uchar u = (gid % 2 == 0) ? in_src[gid * 2] : in_src[gid * 2 - 2];
    uchar v = (gid % 2 == 0) ? in_src[gid * 2 + 2] : in_src[gid * 2];
    float a = in_src[stride * height + gid] / 255.f;

    float3 yuv_in = (float3)(y / 255.f, u / 255.f, v / 255.f);

    float3 rgb_in         = yuv_limited_to_RGB(yuv_in, src_colorspace);
    float3 rgb_linear     = eotf3(rgb_in, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgba_709       = (float4)(oetf3(rgb_709_linear, CS_BT709), a);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgba_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgba_709.x;
    out_rgba[gid * 4 + 1] = rgba_709.y;
    out_rgba[gid * 4 + 2] = rgba_709.z;
    out_rgba[gid * 4 + 3] = rgba_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), rgba_709);
}

__kernel void convertSource_P216(
    __global const ushort* in_src,
    write_only image2d_t   out_img_rgba,
    __global float*        out_rgba,
    __global float*        out_yuv,
    __global float*        out_xyz,
    uint                   width,
    uint                   height,
    uint                   line_stride_in_bytes,
    colorspace_t           src_colorspace) {
    const size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    if (line_stride_in_bytes == 0)
        line_stride_in_bytes = width * 2u;

    // Guard even width (422 needs pairs)
    if ((width & 1u) != 0u)
        return;

    const uint y_stride  = line_stride_in_bytes >> 1u;
    const uint uv_stride = y_stride;
    const uint uv_plane  = y_stride * height;

    const int2 xy = coordsVecFromIndex(gid, width);

    const uint yIdx = xy.y * y_stride + xy.x;

    const ushort Y16 = in_src[yIdx];

    const uint uvX = xy.x >> 1;
    const uint uvY = xy.y;

    const uint uvPairOffset = uvY * uv_stride + uvX * 2u;

    ushort U16 = in_src[uv_plane + uvPairOffset + 0];
    ushort V16 = in_src[uv_plane + uvPairOffset + 1];

    const float norm = 1.f / 65535.f;

    float3 yuv_in = (float3)((float)Y16 * norm, (float)U16 * norm, (float)V16 * norm);

    // limit Y to 4096 to 60160 range
    // yuv_in.x = yuv_in.x * 219.f / 255.f + 16.f / 255.f;

    clamp(yuv_in, 0.f, 1.f);

    float3 rgb_in         = yuv_full_to_RGB(yuv_in, src_colorspace);
    float3 rgb_linear     = eotf3(rgb_in, src_colorspace);
    float3 rgb_709_linear = linearRGB_to_linearRGB_709(rgb_linear, src_colorspace);
    float4 rgba_709       = (float4)(oetf3(rgb_709_linear, CS_BT709), 1.0f);
    float3 yuv_709        = rgb_709_to_YUV_709_Limited(rgba_709.xyz);
    float3 xyz            = linearRGB_709_to_XYZ(rgb_709_linear, src_colorspace);

    out_rgba[gid * 4 + 0] = rgba_709.x;
    out_rgba[gid * 4 + 1] = rgba_709.y;
    out_rgba[gid * 4 + 2] = rgba_709.z;
    out_rgba[gid * 4 + 3] = rgba_709.w;

    out_yuv[gid * 3 + 0] = yuv_709.x;
    out_yuv[gid * 3 + 1] = yuv_709.y;
    out_yuv[gid * 3 + 2] = yuv_709.z;

    out_xyz[gid * 3 + 0] = xyz.x;
    out_xyz[gid * 3 + 1] = xyz.y;
    out_xyz[gid * 3 + 2] = xyz.z;

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), (float4)(rgb_in, 1.0f));
}