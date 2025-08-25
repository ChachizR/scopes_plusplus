#include "common.cl"
#include "colorspaces.cl"

#define FC_MAP_SIZE 256

__kernel void createFalseColorImage(
    __global const float* yuv,
    uint width, uint height,
    __global const uchar4* colorMap,
    write_only image2d_t   outImage) {

    size_t gid = get_global_id(0);

    if (gid >= width * height)
        return;

    uchar y = (uchar)(yuv[gid * 3] * 255.f);

    float4 color = ucharColorToFloat4(colorMap[y]);

    write_imagef(outImage, coordsVecFromIndex(gid, width), color);
}