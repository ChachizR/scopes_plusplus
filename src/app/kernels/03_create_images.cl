inline uchar clamp8(int value) {
    return value < 0 ? 0 : value > 255 ? 255
                                       : value;
}

#define WF_WIDTH 580
#define WF_HEIGHT 256
#define SC_WIDTH 256
#define SC_HEIGHT 256

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
    size_t gx = get_global_size(0);

    size_t gid    = y * gx + x;
    size_t readPB = gid * 3;
    size_t rgbaPB = ((255 - y) * gx + x) * 4;

    if (x >= WF_WIDTH || y >= WF_HEIGHT)
        return;

    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

    // ===== LUMA =====

    g = (float)clamp8(in_hist_yuv[readPB + 0]) / 255.f;

    write_imagef(out_wf_luma, (int2)(x, y), (float4)(0.f, g, 0.f, g));
}