#pragma once
#include "imgui.h"
#include <algorithm>

// ─── Horizontal Splitter (Drag Up/Down to Adjust Height) ──────────────────────
inline bool DrawHorizontalSplitter(const char* id, float& height, float default_h, float min_h = 80.0f, float max_h = 1600.0f) {
    ImGui::PushID(id);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float bar_h = 8.0f;
    ImVec2 size(avail_w, bar_h);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    ImGui::InvisibleButton("##split_h_btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        ImGui::SetTooltip("Drag to resize height (Double-click to reset to %.0f px)", default_h);
    }

    if (active) {
        float dy = ImGui::GetIO().MouseDelta.y;
        height += dy;
        if (height < min_h) height = min_h;
        if (height > max_h) height = max_h;
    }

    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        height = default_h;
    }

    // Visual rendering
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float mid_y = cursor.y + bar_h * 0.5f;

    ImU32 line_col = active  ? IM_COL32(50, 165, 255, 255)
                   : hovered ? IM_COL32(40, 130, 220, 210)
                   : IM_COL32(50, 60, 85, 140);
    draw_list->AddLine(ImVec2(cursor.x, mid_y), ImVec2(cursor.x + avail_w, mid_y), line_col,
                       active ? 2.5f : (hovered ? 2.0f : 1.0f));

    // Three grip dots in the center
    float cx = cursor.x + avail_w * 0.5f;
    ImU32 dot_col = active  ? IM_COL32(255, 255, 255, 255)
                  : hovered ? IM_COL32(180, 220, 255, 240)
                  : IM_COL32(90, 110, 145, 160);
    draw_list->AddCircleFilled(ImVec2(cx - 12, mid_y), 2.0f, dot_col);
    draw_list->AddCircleFilled(ImVec2(cx,      mid_y), 2.0f, dot_col);
    draw_list->AddCircleFilled(ImVec2(cx + 12, mid_y), 2.0f, dot_col);

    ImGui::PopID();
    return active;
}

// ─── Vertical Splitter (Drag Left/Right to Adjust Width) ──────────────────────
inline bool DrawVerticalSplitter(const char* id, float& width, float default_w, float min_w, float max_w, float height = -1.0f) {
    ImGui::PushID(id);
    if (height <= 0.0f) height = ImGui::GetContentRegionAvail().y;
    float bar_w = 8.0f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 size(bar_w, height);

    ImGui::InvisibleButton("##split_v_btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::SetTooltip("Drag to resize width (Double-click to reset to %.0f px)", default_w);
    }

    if (active) {
        float dx = ImGui::GetIO().MouseDelta.x;
        width += dx;
        if (width < min_w) width = min_w;
        if (width > max_w) width = max_w;
    }

    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        width = default_w;
    }

    // Visual rendering
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float mid_x = cursor.x + bar_w * 0.5f;

    ImU32 line_col = active  ? IM_COL32(50, 165, 255, 255)
                   : hovered ? IM_COL32(40, 130, 220, 210)
                   : IM_COL32(50, 60, 85, 140);
    draw_list->AddLine(ImVec2(mid_x, cursor.y), ImVec2(mid_x, cursor.y + height), line_col,
                       active ? 2.5f : (hovered ? 2.0f : 1.0f));

    // Three grip dots in the center
    float cy = cursor.y + height * 0.5f;
    ImU32 dot_col = active  ? IM_COL32(255, 255, 255, 255)
                  : hovered ? IM_COL32(180, 220, 255, 240)
                  : IM_COL32(90, 110, 145, 160);
    draw_list->AddCircleFilled(ImVec2(mid_x, cy - 12), 2.0f, dot_col);
    draw_list->AddCircleFilled(ImVec2(mid_x, cy),      2.0f, dot_col);
    draw_list->AddCircleFilled(ImVec2(mid_x, cy + 12), 2.0f, dot_col);

    ImGui::PopID();
    return active;
}
