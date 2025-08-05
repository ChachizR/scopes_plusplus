#include "common.cl"

__kernel void createWaveformImages(__global const uint* in_hist_rgb,
                                   __global const uint* in_hist_yuv,
                                   write_only image2d_t out_wf_luma,
                                   write_only image2d_t out_wf_rgb,
                                   write_only image2d_t out_wf_rgb_parade,
                                   write_only image2d_t out_wf_rgb_blacks,
                                   write_only image2d_t out_wf_yuv_parade,
                                   uint src_width, int colorspace, float brightness) {
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

    g = (float)clamp8(in_hist_yuv[readPB + 0] * brightness) / 255.f;

    write_imagef(out_wf_luma, xy, (float4)(0.f, g, 0.f, g));

    // ===== RGB =====

    r = (float)clamp8(in_hist_rgb[readPB + 0] * brightness) / 255.f;
    g = (float)clamp8(in_hist_rgb[readPB + 1] * brightness) / 255.f;
    b = (float)clamp8(in_hist_rgb[readPB + 2] * brightness) / 255.f;
    a = max(r, max(g, b));

    write_imagef(out_wf_rgb, xy, (float4)(r, g, b, a));

    // ===== RGB PARADE =====

    {
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
            a = r;
        } else if (x < 2 * WF_WIDTH / 3) {
            r = 0.f;
            g = ((float)(in_hist_rgb[src_gid * 3 + 1] +
                         in_hist_rgb[src_gid * 3 + 4] +
                         in_hist_rgb[src_gid * 3 + 7]) *
                 brightness / 3.f) /
                255.f;
            b = 0.f;
            a = g;
        } else {
            r = 0.f;
            g = 0.f;
            b = ((float)(in_hist_rgb[src_gid * 3 + 2] +
                         in_hist_rgb[src_gid * 3 + 5] +
                         in_hist_rgb[src_gid * 3 + 8]) *
                 brightness / 3.f) /
                255.f;
            a = b;
        }

        write_imagef(out_wf_rgb_parade, xy, (float4)(r, g, b, a));
    }

    // ===== RGB BLACKLEVEL =====

    {
        uint src_y   = y * 0.15f;
        uint src_gid = src_y * gx + x;

        r = (float)clamp8(in_hist_rgb[src_gid * 3 + 0] * brightness) / 255.f;
        g = (float)clamp8(in_hist_rgb[src_gid * 3 + 1] * brightness) / 255.f;
        b = (float)clamp8(in_hist_rgb[src_gid * 3 + 2] * brightness) / 255.f;
        a = max(r, max(g, b));

        write_imagef(out_wf_rgb_blacks, xy, (float4)(r, g, b, a));
    }

    // ===== YUV PARADE =====

    {
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
            r = Y;
            g = Y;
            b = Y;
            a = Y;
        } else if (x < 2 * WF_WIDTH / 3) {
            uchar ru, gu, bu;
            yuvToRgb(128, y, 128, &ru, &gu, &bu, colorspace);
            r = (float)ru * U / 255.f;
            g = (float)gu * U / 255.f;
            b = (float)bu * U / 255.f;
            a = U;

        } else {
            uchar rv, gv, bv;
            yuvToRgb(128, 128, y, &rv, &gv, &bv, colorspace);
            r = (float)rv * V / 255.f;
            g = (float)gv * V / 255.f;
            b = (float)bv * V / 255.f;
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
    uint src_width, float brightness) {

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    if (x >= SC_WIDTH || y >= SC_HEIGHT)
        return;

    size_t gid = y * SC_WIDTH + x;

    size_t rgbaPB = gid * 4;

    int2 xy = (int2)(x, y);

    float  R, G, B, A;
    float4 color;

    // ===== UV SCOPE =====

    {
        A = (float)in_hist2d_uv_rgba[rgbaPB + 3] / 255.f * brightness;
        // R = (float)in_hist2d_uv_rgba[rgbaPB + 0] / 255.f * A;
        // G = (float)in_hist2d_uv_rgba[rgbaPB + 1] / 255.f * A;
        // B = (float)in_hist2d_uv_rgba[rgbaPB + 2] / 255.f * A;
        uchar r, g, b;
        yuvToRgb(128, x, SC_HEIGHT - y, &r, &g, &b, CS_BT709);

        R = (float)r / 255.f;
        G = (float)g / 255.f;
        B = (float)b / 255.f;

        color = (float4)(R, G, B, A);
        write_imagef(out_sc_uv, xy, color);
    }

    // ===== XYZ SCOPE =====

    {
        A = (float)in_hist2d_xyz_rgba[rgbaPB + 3] / 255.f * brightness;
        // R = (float)in_hist2d_xyz_rgba[rgbaPB + 0] / 255.f * A;
        // G = (float)in_hist2d_xyz_rgba[rgbaPB + 1] / 255.f * A;
        // B = (float)in_hist2d_xyz_rgba[rgbaPB + 2] / 255.f * A;

        color = (float4)(A, A, A, A);
        write_imagef(out_sc_xyz, xy, color);
    }

    // ===== DIAMOND SCOPE =====

    {
        A = (float)in_hist2d_dia_rgba[rgbaPB + 3] / 255.f * brightness;
        // R = (float)in_hist2d_dia_rgba[rgbaPB + 0] / 255.f * A;
        // G = (float)in_hist2d_dia_rgba[rgbaPB + 1] / 255.f * A;
        // B = (float)in_hist2d_dia_rgba[rgbaPB + 2] / 255.f * A;

        color = (float4)(A, A, A, A);
        write_imagef(out_sc_dia, xy, color);
    }
}