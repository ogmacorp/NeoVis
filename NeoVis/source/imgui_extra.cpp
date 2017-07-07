// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "imgui_extra.h"

using namespace ImGui;

void ImGui::ImageHover(sf::Texture &tex, bool &hovering, int &hoverIndex, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    if (border_col.w > 0.0f)
        bb.Max += ImVec2(2, 2);
    ItemSize(bb);
    if (!ItemAdd(bb, NULL))
        return;

    if (border_col.w > 0.0f)
    {
        window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(border_col), 0.0f);
        window->DrawList->AddImage((void*)tex.getNativeHandle(), bb.Min + ImVec2(1, 1), bb.Max - ImVec2(1, 1), uv0, uv1, GetColorU32(tint_col));
    }
    else
    {
        window->DrawList->AddImage((void*)tex.getNativeHandle(), bb.Min, bb.Max, uv0, uv1, GetColorU32(tint_col));
    }

    ImVec2 mousePos = GetMousePos();

    hovering = bb.Contains(mousePos);

    if (hovering) {
        ImVec2 bbSize = bb.GetSize();

        float ratioX = (mousePos.x - bb.Min.x) / bbSize.x;
        float ratioY = (mousePos.y - bb.Min.y) / bbSize.y;

        int bitX = static_cast<int>(ratioX * tex.getSize().x);
        int bitY = static_cast<int>(ratioY * tex.getSize().y);

        hoverIndex = bitX + bitY * tex.getSize().x;
    }
}