inline uchar clamp8(int value) {
    return value < 0 ? 0 : value > 255 ? 255
                                       : value;
}

inline uchar preMultiply(uchar a, uchar b) {
    return (uchar)rint(((float)a * (float)b) / 255.f);
}

#define CS_BT601 1
#define CS_BT709 2
#define CS_BT2020 3

void yuvToRgb(uchar y, uchar u, uchar v, uchar* r, uchar* g, uchar* b,
              int colorspace) {
    int oY = y - 16;
    int oU = u - 128;
    int oV = v - 128;

    float Y = 1.16438f * oY;

    if (colorspace == CS_BT601) {
        *r = clamp8((int)rint(Y + 1.59603f * oV));
        *g = clamp8((int)rint(Y - 0.39176f * oU - 0.81297f * oV));
        *b = clamp8((int)rint(Y + 2.01723f * oU));
    } else if (colorspace == CS_BT709) {
        *r = clamp8((int)rint(Y + 1.79274f * oV));
        *g = clamp8((int)rint(Y - 0.21325f * oU - 0.53291f * oV));
        *b = clamp8((int)rint(Y + 2.1124f * oU));
    } else if (colorspace == CS_BT2020) {
        *r = clamp8((int)rint(Y + 1.67867f * oV));
        *g = clamp8((int)rint(Y - 0.18733f * oU - 0.65042f * oV));
        *b = clamp8((int)rint(Y + 2.14177f * oU));
    }
}

void rgbToYuv(uchar r, uchar g, uchar b, uchar* y, uchar* u, uchar* v,
              int colorspace) {
    if (colorspace == CS_BT601) {
        *y = clamp8(16 + (int)rint(r * 0.25679f + g * 0.50413f + b * 0.09791f));
        *u = clamp8(128 + (int)rint(-0.14822f * r - 0.29099f * g + 0.43922f * b));
        *v = clamp8(128 + (int)rint(0.43922f * r - 0.36779f * g - 0.07143f * b));
    } else if (colorspace == CS_BT709) {
        // BT.709 RGB Full 0 - 255 to 16 - 235
        *y = clamp8(16 + (int)rint(r * 0.18259f + g * 0.61423f + b * 0.06201f));
        *u = clamp8(128 + (int)rint(r * -0.10064f + g * -0.33857f + b * 0.43922f));
        *v = clamp8(128 + (int)rint(r * 0.43922f + g * -0.39894f + b * -0.04027f));
    } else if (colorspace == CS_BT2020) {
        *y = clamp8(16 + (int)rint(0.22561f * r + 0.58228f * g + 0.05093f * b));
        *u = clamp8(128 + (int)rint(-0.12266f * r - 0.31656f * g + 0.43922f * b));
        *v = clamp8(128 + (int)rint(0.43922f * r - 0.40389f * g - 0.03533f * b));
    }
}

#define WF_WIDTH 580
#define WF_HEIGHT 256
#define SC_WIDTH 256
#define SC_HEIGHT 256

#define WAVEFORM_BINS 256

inline void coordsFromIndex(uint index, uint width, uint* x, uint* y) {
    *x = index % width;
    *y = index / width;
}

inline int2 coordsVecFromIndex(uint index, uint width) {
    return (int2)(index % width, index / width);
}

inline uint indexFromCoords(uint x, uint y, uint width) {
    return y * width + x;
}

inline uint indexFromCoordsVec(int2 coords, uint width) {
    return coords.y * width + coords.x;
}