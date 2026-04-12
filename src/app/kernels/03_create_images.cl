#include "common.cl"
#include "colorspaces.cl"

__kernel void createWaveformImages(
    __global const uint* in_hist_rgb,
    __global const uint* in_hist_yuv,
    write_only image2d_t out_wf_luma,
    write_only image2d_t out_wf_rgb,
    write_only image2d_t out_wf_rgb_parade,
    write_only image2d_t out_wf_rgb_blacks,
    write_only image2d_t out_wf_yuv_parade,
    uint src_width, int colorspace, float brightness,
    RenderFeatureFlags features) {
    size_t x  = get_global_id(0);
    size_t y  = get_global_id(1);
    int2   xy = (int2)(x, y);
    size_t gx = get_global_size(0);

    size_t gid    = y * gx + x;
    size_t readPB = gid * 3;
    size_t rgbaPB = ((255 - y) * gx + x) * 4;

    if (x >= WF_WIDTH || y >= WF_HEIGHT)
        return;

    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

    // ===== LUMA =====

    if (features & RENDER_FEATURE_WF_LUMA) {
        a = (float)clamp8(in_hist_yuv[readPB + 0] * brightness) / 255.f;
        write_imagef(out_wf_luma, xy, (float4)(0.f, 1.f, 0.f, a));
    }

    // ===== RGB =====

    if (features & RENDER_FEATURE_WF_RGB) {
        r = (float)in_hist_rgb[readPB + 0] * brightness / 255.f;
        g = (float)in_hist_rgb[readPB + 1] * brightness / 255.f;
        b = (float)in_hist_rgb[readPB + 2] * brightness / 255.f;
        a = 1.f;

        write_imagef(out_wf_rgb, xy, (float4)(r, g, b, a));
    }

    // ===== RGB PARADE =====

    if (features & RENDER_FEATURE_WF_RGBPARADE) {
        uint src_x   = (x % (WF_WIDTH / 3)) * 3;
        uint src_gid = y * gx + src_x;

        if (x < WF_WIDTH / 3) {
            r = ((float)(in_hist_rgb[src_gid * 3 + 0] +
                         in_hist_rgb[src_gid * 3 + 3] +
                         in_hist_rgb[src_gid * 3 + 6]) *
                 brightness / 3.f) /
                255.f;
            g = 0.f;
            b = 0.f;
            a = 1.f;
        } else if (x < 2 * WF_WIDTH / 3) {
            r = 0.f;
            g = ((float)(in_hist_rgb[src_gid * 3 + 1] +
                         in_hist_rgb[src_gid * 3 + 4] +
                         in_hist_rgb[src_gid * 3 + 7]) *
                 brightness / 3.f) /
                255.f;
            b = 0.f;
            a = 1.f;
        } else {
            r = 0.f;
            g = 0.f;
            b = ((float)(in_hist_rgb[src_gid * 3 + 2] +
                         in_hist_rgb[src_gid * 3 + 5] +
                         in_hist_rgb[src_gid * 3 + 8]) *
                 brightness / 3.f) /
                255.f;
            a = 1.f;
        }

        write_imagef(out_wf_rgb_parade, xy, (float4)(r, g, b, a));
    }

    // ===== RGB BLACKLEVEL =====

    if (features & RENDER_FEATURE_WF_RGBBLACKS) {
        uint src_y   = y * 0.137255f;
        uint src_gid = src_y * gx + x;

        r = (float)in_hist_rgb[src_gid * 3 + 0] * brightness / 255.f;
        g = (float)in_hist_rgb[src_gid * 3 + 1] * brightness / 255.f;
        b = (float)in_hist_rgb[src_gid * 3 + 2] * brightness / 255.f;
        a = 1.f;

        write_imagef(out_wf_rgb_blacks, xy, (float4)(r, g, b, a));
    }

    // ===== YUV PARADE =====

    if (features & RENDER_FEATURE_WF_YUVPARADE) {
        uint src_x   = (x % (WF_WIDTH / 3)) * 3;
        uint src_gid = y * gx + src_x;

        float Y = ((float)(in_hist_yuv[src_gid * 3 + 0] +
                           in_hist_yuv[src_gid * 3 + 3] +
                           in_hist_yuv[src_gid * 3 + 6]) *
                   brightness / 3.f) /
                  255.f;

        float U = ((float)(in_hist_yuv[src_gid * 3 + 1] +
                           in_hist_yuv[src_gid * 3 + 4] +
                           in_hist_yuv[src_gid * 3 + 7]) *
                   brightness / 3.f) /
                  255.f;

        float V = ((float)(in_hist_yuv[src_gid * 3 + 2] +
                           in_hist_yuv[src_gid * 3 + 5] +
                           in_hist_yuv[src_gid * 3 + 8]) *
                   brightness / 3.f) /
                  255.f;
        if (x < WF_WIDTH / 3) {
            r = 255.f;
            g = 255.f;
            b = 255.f;
            a = Y;
        } else if (x < 2 * WF_WIDTH / 3) {

            float3 rgb = yuv_709_Limited_to_RGB_709((float3)(0.5f, y / 255.f, 0.5f));

            r = rgb.x;
            g = rgb.y;
            b = rgb.z;
            a = U;

        } else {
            float3 rgb = yuv_709_Limited_to_RGB_709((float3)(0.5f, 0.5f, y / 255.f));

            r = rgb.x;
            g = rgb.y;
            b = rgb.z;
            a = V;
        }

        write_imagef(out_wf_yuv_parade, xy, (float4)(r, g, b, a));
    }
}

__kernel void createScopeImages(
    __global const uint* in_hist2d_uv_rgba,
    __global const uint* in_hist2d_xyz_rgba,
    __global const uint* in_hist2d_dia_rgba,
    write_only image2d_t out_sc_uv,
    write_only image2d_t out_sc_xyz,
    write_only image2d_t out_sc_dia,
    uint src_width, float brightness,
    RenderFeatureFlags features) {

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= SC_WIDTH || y >= SC_HEIGHT)
        return;

    size_t gid = y * SC_WIDTH + x;

    size_t rgbaPB = gid * 4;

    int2   xy = (int2)(x, y);
    float2 XY = (float2)(x / 255.f, y / 255.f);

    float4 rgba = (float4)(0.f, 0.f, 0.f, 0.f);

    // ===== UV SCOPE =====

    if (features & RENDER_FEATURE_SC_UV) {
        rgba.w = (float)in_hist2d_uv_rgba[rgbaPB + 3] / 255.f * brightness;

        rgba.xyz = yuv_709_Limited_to_RGB_709((float3)(0.5f, x / 255.f, (SC_HEIGHT - y) / 255.f));

        write_imagef(out_sc_uv, xy, rgba);
    }

    // ===== XYZ SCOPE =====

    if (features & RENDER_FEATURE_SC_XYZ) {
        rgba.w = (float)in_hist2d_xyz_rgba[rgbaPB + 3] / 255.f * brightness;

        rgba.xyz = xy_to_RGB_709(XY.x, XY.y, 0.5f);

        write_imagef(out_sc_xyz, xy, rgba);
    }

    // ===== DIAMOND SCOPE =====

    if (features & RENDER_FEATURE_SC_DIA) {
        rgba.w = (float)in_hist2d_dia_rgba[rgbaPB + 3] / 255.f * brightness;

        if (XY.y <= 0.5f) {
            rgba.x = 0.f;
            rgba.y = 1.5f - XY.x - 2.f * XY.y;
            rgba.z = 0.5f + XY.x - 2.f * XY.y;
        } else {
            rgba.x = XY.x + 2.f * XY.y - 1.5f;
            rgba.y = 2.f * XY.y - XY.x - 0.5f;
            rgba.z = 0.f;
        }

        write_imagef(out_sc_dia, xy, rgba);
    }
}
