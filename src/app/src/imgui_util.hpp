#pragma once

#include "pch.hpp"

#include "cl_renderer.hpp"

namespace scpp {

enum class ScaleBehavior {
    DoNotScale,
    OnlyScaleDown,
    ScaleToFit,
};

enum class FlipBehavior {
    DoNotFlip,
    FlipHorizontally,
    FlipVertically,
    FlipBoth,
};

static inline auto ImGuiUtilGetContentScale(ImVec2 size, ScaleBehavior scaleBehavior = ScaleBehavior::OnlyScaleDown) -> float {
    ImVec2 avail  = ImGui::GetContentRegionAvail();
    float  imgW   = size.x;
    float  imgH   = size.y;
    float  scaleX = avail.x / imgW;
    float  scaleY = avail.y / imgH;
    float  scale  = 1.f;
    switch (scaleBehavior) {
    case ScaleBehavior::DoNotScale:
        break;
    case ScaleBehavior::OnlyScaleDown:
        scale = (std::min)((std::min)(scaleX, scaleY), 1.f);
        break;
    case ScaleBehavior::ScaleToFit:
        scale = (std::min)(scaleX, scaleY);
        break;
    }
    return scale;
}

static inline auto ImGuiUtilGetImageSize(ImVec2 size, ScaleBehavior scaleBehavior = ScaleBehavior::OnlyScaleDown) -> ImVec2 {
    float scale = ImGuiUtilGetContentScale(size, scaleBehavior);
    return ImVec2(size.x * scale, size.y * scale);
}

static constexpr inline auto ImGuiUtilGetUVs(FlipBehavior flipBehavior) -> std::pair<ImVec2, ImVec2> {
    if (flipBehavior == FlipBehavior::DoNotFlip) {
        return {ImVec2(0, 0), ImVec2(1, 1)};
    } else if (flipBehavior == FlipBehavior::FlipHorizontally) {
        return {ImVec2(1, 0), ImVec2(0, 1)};
    } else if (flipBehavior == FlipBehavior::FlipVertically) {
        return {ImVec2(0, 1), ImVec2(1, 0)};
    } else { // FlipBoth
        return {ImVec2(1, 1), ImVec2(0, 0)};
    }
}

static constexpr auto c_bgColor = ImVec4(0.f, 0.f, 0.f, 1.f);

static inline void ImGuiUtilImageRender(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);
}

static constexpr ImU32 c_lineColor     = IM_COL32(255, 255, 255, 128);
static constexpr float c_lineThickness = 1.5f;

static inline void ImGuiUtilRenderLumaWF(
    const CLGLTextureRGBA& texture,
    SourceYUVRange         yuvRange,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {

    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw Horizontal lines on the waveform in interval of 32

    if (yuvRange == SourceYUVRange::Full) {
        for (int i = 0; i <= 256; i += 32) {
            float y = topLeft.y + imgSize.y - (i / 255.f) * imgSize.y;
            drawList->AddLine(
                ImVec2(topLeft.x, y),
                ImVec2(topLeft.x + imgSize.x, y),
                c_lineColor, c_lineThickness);
        }
    } else { // Limited range
        for (int i = 0; i <= 256; i += 32) {
            float y = topLeft.y + imgSize.y - ((16.f + i * (219.f / 255.f)) / 255.f) * imgSize.y;
            drawList->AddLine(
                ImVec2(topLeft.x, y),
                ImVec2(topLeft.x + imgSize.x, y),
                c_lineColor, c_lineThickness);
        }
    }
}

static inline void ImGuiUtilRenderRGBWF(
    const CLGLTextureRGBA& texture,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();
    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i <= 256; i += 32) {
        float y = topLeft.y + imgSize.y - (i / 255.f) * imgSize.y;
        drawList->AddLine(
            ImVec2(topLeft.x, y),
            ImVec2(topLeft.x + imgSize.x, y),
            c_lineColor, c_lineThickness);
    }
}

static inline constexpr auto GetCIEPrimaries(SourceColorSpace colorSpace) -> std::array<ImVec2, 3> {
    switch (colorSpace) {
    case SourceColorSpace::BT601_525:
        return {ImVec2(0.630f, 0.340f), ImVec2(0.310f, 0.595f), ImVec2(0.155f, 0.070f)};
    case SourceColorSpace::BT601_625:
        return {ImVec2(0.640f, 0.330f), ImVec2(0.290f, 0.600f), ImVec2(0.150f, 0.060f)};
    case SourceColorSpace::BT709:
    case SourceColorSpace::SRGB:
        return {ImVec2(0.640f, 0.330f), ImVec2(0.300f, 0.600f), ImVec2(0.150f, 0.060f)};
    case SourceColorSpace::BT2020:
        return {ImVec2(0.708f, 0.292f), ImVec2(0.170f, 0.797f), ImVec2(0.131f, 0.046f)};
    }

    return {ImVec2(0.f, 0.f), ImVec2(0.f, 0.f), ImVec2(0.f, 0.f)};
}

static inline constexpr auto c_cieTriangleColorspaces =
    std::to_array<SourceColorSpace>({SourceColorSpace::BT601_525,
                                     SourceColorSpace::BT601_625,
                                     SourceColorSpace::BT709,
                                     SourceColorSpace::SRGB,
                                     SourceColorSpace::BT2020});

static inline consteval auto GetCIELocus() -> std::array<ImVec2, 100> {
    auto g = [](float x, float u, float t1, float t2) -> float {
        return (x < u)
                   ? exp_constexpr(-(t1 * t1) * (x - u) * (x - u) / 2.f)
                   : exp_constexpr(-(t2 * t2) * (x - u) * (x - u) / 2.f);
    };

    auto x = [&](float l) -> float {
        return 1.056f * g(l, 599.8f, 0.0264f, 0.0323f) +
               0.362f * g(l, 442.0f, 0.0624f, 0.0374f) -
               0.065f * g(l, 501.1f, 0.049f, 0.0382f);
    };

    auto y = [&](float l) -> float {
        return 0.821f * g(l, 568.8f, 0.0213f, 0.0247f) +
               0.286f * g(l, 530.9f, 0.0613f, 0.0322f);
    };

    auto z = [&](float l) -> float {
        return 1.217f * g(l, 437.0f, 0.0845f, 0.0278f) +
               0.681f * g(l, 459.0f, 0.0385f, 0.0725f);
    };

    std::array<ImVec2, 100> locus;

    float lStart = 440.f;
    float lEnd   = 650.f;
    for (int i = 0; i < 100; i++) {
        float l = lStart + (lEnd - lStart) * (i / 99.f);
        float xVal = x(l);
        float yVal = y(l);
        float zVal = z(l);
        float X    = xVal / (xVal + yVal + zVal);
        float Y    = yVal / (xVal + yVal + zVal);

        locus[i] = ImVec2(X, Y);
    }

    return locus;
}

static inline constexpr auto c_cieLocus = GetCIELocus();

static inline void ImGuiUtilRenderCIE(
    const CLGLTextureRGBA& texture,
    SourceColorSpace       selectedColorSpace,
    ScaleBehavior          scaleBehavior = ScaleBehavior::OnlyScaleDown,
    FlipBehavior           flipBehavior  = FlipBehavior::DoNotFlip) {
    const auto imgSize     = ImGuiUtilGetImageSize(texture.size.ToImVec2(), scaleBehavior);
    const auto [uv0, uv1]  = ImGuiUtilGetUVs(flipBehavior);
    const auto imTextureID = static_cast<ImTextureID>(texture.glTextureID);
    const auto topLeft     = ImGui::GetCursorScreenPos();

    ImGui::ImageWithBg(imTextureID, imgSize, uv0, uv1, c_bgColor);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw horizontal and vertical grid lines at .1 intervals every second one slightly less opaque
    for (int i = 0; i <= 10; i++) {
        float x     = topLeft.x + (i / 10.f) * imgSize.x;
        float y     = topLeft.y + (i / 10.f) * imgSize.y;
        ImU32 color = (i % 2 == 0) ? IM_COL32(255, 255, 255, 128) : IM_COL32(255, 255, 255, 64);
        drawList->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + imgSize.y), color, c_lineThickness);
        drawList->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + imgSize.x, y), color, c_lineThickness);
    }

    // Draw the triangles for each color space

    for (const auto colorSpace : c_cieTriangleColorspaces) {
        auto primaries = GetCIEPrimaries(colorSpace);

        ImVec2 p1    = ImVec2(topLeft.x + primaries[0].x * imgSize.x, topLeft.y + (1.f - primaries[0].y) * imgSize.y);
        ImVec2 p2    = ImVec2(topLeft.x + primaries[1].x * imgSize.x, topLeft.y + (1.f - primaries[1].y) * imgSize.y);
        ImVec2 p3    = ImVec2(topLeft.x + primaries[2].x * imgSize.x, topLeft.y + (1.f - primaries[2].y) * imgSize.y);
        ImU32  color = (colorSpace == selectedColorSpace) ? IM_COL32(255, 255, 255, 192) : IM_COL32(255, 255, 255, 64);
        drawList->AddTriangle(p1, p2, p3, color);
    }

    // Draw the CIE locus

    for (size_t i = 0; i < c_cieLocus.size() - 1; i++) {
        ImVec2 p1 = ImVec2(topLeft.x + c_cieLocus[i].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i].y) * imgSize.y);
        ImVec2 p2 = ImVec2(topLeft.x + c_cieLocus[i + 1].x * imgSize.x, topLeft.y + (1.f - c_cieLocus[i + 1].y) * imgSize.y);
        drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 64), c_lineThickness);
    }
}

} // namespace scpp