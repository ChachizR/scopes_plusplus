#include "common.cl"

__kernel void convertSource_RGBA_8888_to_RGBA_YUV(__global const uchar* in_src,
                                                  write_only image2d_t  out_img_rgba,
                                                  __global uchar*       out_rgba,
                                                  __global uchar*       out_yuv,
                                                  uint                  width,
                                                  uint                  height,
                                                  int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a;
    uchar y, u, v;

    r = in_src[rgbaPB + 0];
    g = in_src[rgbaPB + 1];
    b = in_src[rgbaPB + 2];
    a = in_src[rgbaPB + 3];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;
    float fa = (float)a / 255.f;

    float4 color = (float4)(fr, fg, fb, fa);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_RGBX_8888_to_RGBA_YUV(__global const uchar* in_src,
                                                  write_only image2d_t  out_img_rgba,
                                                  __global uchar*       out_rgba,
                                                  __global uchar*       out_yuv,
                                                  uint                  width,
                                                  uint                  height,
                                                  int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a;
    uchar y, u, v;

    r = in_src[rgbaPB + 0];
    g = in_src[rgbaPB + 1];
    b = in_src[rgbaPB + 2];
    a = in_src[rgbaPB + 3];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_BGRA_8888_to_RGBA_YUV(__global const uchar* in_src,
                                                  write_only image2d_t  out_img_rgba,
                                                  __global uchar*       out_rgba,
                                                  __global uchar*       out_yuv,
                                                  uint                  width,
                                                  uint                  height,
                                                  int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a;
    uchar y, u, v;

    b = in_src[rgbaPB + 0];
    g = in_src[rgbaPB + 1];
    r = in_src[rgbaPB + 2];
    a = in_src[rgbaPB + 3];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;
    float fa = (float)a / 255.f;

    float4 color = (float4)(fr, fg, fb, fa);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_BGRX_8888_to_RGBA_YUV(__global const uchar* in_src,
                                                  write_only image2d_t  out_img_rgba,
                                                  __global uchar*       out_rgba,
                                                  __global uchar*       out_yuv,
                                                  uint                  width,
                                                  uint                  height,
                                                  int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a;
    uchar y, u, v;

    b = in_src[rgbaPB + 0];
    g = in_src[rgbaPB + 1];
    r = in_src[rgbaPB + 2];
    a = in_src[rgbaPB + 3];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_ARGB_8888_to_RGBA_YUV(__global const uchar* in_src,
                                                  write_only image2d_t  out_img_rgba,
                                                  __global uchar*       out_rgba,
                                                  __global uchar*       out_yuv,
                                                  uint                  width,
                                                  uint                  height,
                                                  int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a;
    uchar y, u, v;

    a = in_src[rgbaPB + 0];
    r = in_src[rgbaPB + 1];
    g = in_src[rgbaPB + 2];
    b = in_src[rgbaPB + 3];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;
    float fa = (float)a / 255.f;

    float4 color = (float4)(fr, fg, fb, fa);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_RGB_888_to_RGBA_YUV(__global const uchar* in_src,
                                                write_only image2d_t  out_img_rgba,
                                                __global uchar*       out_rgba,
                                                __global uchar*       out_yuv,
                                                uint                  width,
                                                uint                  height,
                                                int                   colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;
    uint rgbPB  = gid * 3;

    uchar r, g, b, a = 255;
    uchar y, u, v;

    r = in_src[rgbPB + 0];
    g = in_src[rgbPB + 1];
    b = in_src[rgbPB + 2];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);

    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_BGR_888_InvY_to_RGBA_YUV(__global const uchar* in_src,
                                                     write_only image2d_t  out_img_rgba,
                                                     __global uchar*       out_rgba,
                                                     __global uchar* out_yuv, uint width,
                                                     uint height, int colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a = 255;
    uchar y, u, v;

    int2 coords = coordsVecFromIndex(gid, width);
    uint bgrPB  = indexFromCoords(coords.x, height - coords.y - 1, width) * 3;

    b = in_src[bgrPB + 0];
    g = in_src[bgrPB + 1];
    r = in_src[bgrPB + 2];

    rgbToYuv(r, g, b, &y, &u, &v, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);
    write_imagef(out_img_rgba, coords, color);
}

__kernel void convertSource_UYVY_422_to_RGBA_YUV(__global const uchar* in_src,
                                                 write_only image2d_t  out_img_rgba,
                                                 __global uchar*       out_rgba,
                                                 __global uchar* out_yuv, uint width,
                                                 uint height, int colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint uyvyPB = gid * 2;
    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a = 255;
    uchar y, u, v;

    y = in_src[uyvyPB + 1];

    if (gid % 2 == 0) {
        u = in_src[uyvyPB];
        v = in_src[uyvyPB + 2];
    } else {
        u = in_src[uyvyPB - 2];
        v = in_src[uyvyPB];
    }

    yuvToRgb(y, u, v, &r, &g, &b, colorspace);

    out_rgba[rgbaPB + 0] = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB + 0] = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);
    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_YUYV_422_to_RGBA_YUV(__global const uchar* in_src,
                                                 write_only image2d_t  out_img_rgba,
                                                 __global uchar*       out_rgba,
                                                 __global uchar* out_yuv, uint width,
                                                 uint height, int colorspace) {
    size_t gid = get_global_id(0);

    if (gid >= width * height) {
        return;
    }

    uint yuyvPB = gid * 2;
    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a = 255;
    uchar y, u, v;

    y = in_src[yuyvPB];

    if (gid % 2 == 0) {
        u = in_src[yuyvPB + 1];
        v = in_src[yuyvPB + 3];
    } else {
        u = in_src[yuyvPB - 1];
        v = in_src[yuyvPB + 1];
    }

    yuvToRgb(y, u, v, &r, &g, &b, colorspace);

    out_rgba[rgbaPB]     = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB]     = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);
    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}

__kernel void convertSource_NV12_to_RGBA_YUV(__global const uchar* in_src,
                                             write_only image2d_t  out_img_rgba,
                                             __global uchar*       out_rgba,
                                             __global uchar* out_yuv, uint width,
                                             uint height, int colorspace) {
    size_t gid = get_global_id(0);

    uint rgbaPB = gid * 4;
    uint yuvPB  = gid * 3;

    uchar r, g, b, a = 255;
    uchar y, u, v;

    y = in_src[gid];

    uint uvStart = width * height;

    uint pX, pY;
    coordsFromIndex(gid, width, &pX, &pY);

    uint pUV = indexFromCoords(pX / 2, pY / 2, width / 2);

    u = in_src[uvStart + pUV * 2];
    v = in_src[uvStart + pUV * 2 + 1];

    yuvToRgb(y, u, v, &r, &g, &b, colorspace);

    out_rgba[rgbaPB]     = r;
    out_rgba[rgbaPB + 1] = g;
    out_rgba[rgbaPB + 2] = b;
    out_rgba[rgbaPB + 3] = a;

    out_yuv[yuvPB]     = y;
    out_yuv[yuvPB + 1] = u;
    out_yuv[yuvPB + 2] = v;

    float fr = (float)r / 255.f;
    float fg = (float)g / 255.f;
    float fb = (float)b / 255.f;

    float4 color = (float4)(fr, fg, fb, 1.0f);
    write_imagef(out_img_rgba, coordsVecFromIndex(gid, width), color);
}