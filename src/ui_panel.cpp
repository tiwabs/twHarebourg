#include "ui_panel.h"

#include "app.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <vector>

void DrawNormalModePanel(App& app) {
    Grid&  grid  = app.GetGrid();
    Logic& logic = app.GetLogic();

    // Map selector + gestion.
    {
        const int mapCount = grid.MapCount();
        if (mapCount > 0) {
            std::vector<const char*> names;
            names.reserve(mapCount);
            for (int i = 0; i < mapCount; ++i)
                names.push_back(grid.MapName(i).c_str());

            int sel = grid.ActiveMapIndex();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##mapsel", &sel, names.data(), mapCount)) {
                if (grid.SwitchToMap(sel)) {
                    logic.player = std::nullopt;
                    logic.target = std::nullopt;
                }
            }
        }

        const float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (ImGui::Button("Editer", ImVec2(halfW, 0.0f)))
            app.EnterDraw();

        ImGui::SameLine();
        if (ImGui::Button("+ Nouvelle", ImVec2(halfW, 0.0f)))
            app.EnterDrawAfterNewMap();

        if (grid.MapCount() > 1) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(120, 30, 30, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(170, 50, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(190, 60, 60, 255));
            if (ImGui::Button("Supprimer cette map", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                const int idx = grid.ActiveMapIndex();
                grid.DeleteMap(idx);
                grid.SwitchToMap(std::max(0, idx - 1));
                logic.player = std::nullopt;
                logic.target = std::nullopt;
                grid.SaveToFile("config.json");
            }
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::Separator();

    // Placement toggle (F2).
    {
        const bool on = !app.Overlay().ClickThrough();
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 130, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 170, 80, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 110, 50, 255));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(65, 65, 65, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 90, 90, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(50, 50, 50, 255));
        }
        if (ImGui::Button(on ? "(F2) Placement : ON" : "(F2) Placement : OFF",
                          ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            app.Overlay().SetClickThrough(!app.Overlay().ClickThrough());
        ImGui::PopStyleColor(3);
        if (!app.Overlay().ClickThrough())
            ImGui::TextDisabled("G = joueur   D = cible");
    }

    ImGui::Separator();

    // HP + Confusion.
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::SliderInt("##hp", &logic.hpPercent, 0, 100, "HP : %d%%");
    ImGui::Text("Confusion : %s", logic.ConfusionLabel().c_str());

    ImGui::Separator();

    // Melee stacks.
    {
        ImGui::Text("Melee :");
        ImGui::SameLine();
        if (ImGui::SmallButton("(PgDn)-##mel")) logic.DecMelee();
        ImGui::SameLine();
        ImGui::Text("%d", logic.meleeStacks);
        ImGui::SameLine();
        if (ImGui::SmallButton("+(PgUp)##mel")) logic.IncMelee();
        ImGui::SameLine();
        if (ImGui::SmallButton("x(Suppr)##mel")) logic.ResetMelee();
    }

    ImGui::Separator();

    // Calibration toggle (F8).
    {
        const bool on = app.CurrentMode() == App::ModeKind::GridEdit;
        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(110, 85, 15, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(150, 115, 25, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(90, 70, 10, 255));
        }
        if (ImGui::Button(on ? "(F8) Calibration : ON" : "(F8) Calibration grille",
                          ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            if (on) app.EnterPlacement();
            else    app.EnterGridEdit();
        }
        if (on) {
            ImGui::PopStyleColor(3);
            Grid& g = app.GetGrid();
            ImGui::TextDisabled("Fleches = origine   Molette = taille");
            ImGui::TextDisabled("(%d, %d)  %dx%d", g.OriginX(), g.OriginY(), g.TileW(), g.TileH());
        }
    }

    ImGui::Separator();
    if (ImGui::Button("(F10) Quitter", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        app.Overlay().RequestClose();
}
