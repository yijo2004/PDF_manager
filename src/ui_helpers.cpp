#include "ui_helpers.h"

#include "imgui.h"

void ApplyCustomTheme()
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 6.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.SeparatorTextPadding = ImVec2(8.0f, 3.0f);
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.13f, 0.17f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.26f, 0.29f, 0.35f, 0.70f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.19f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.29f, 0.39f, 0.70f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.41f, 0.56f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.37f, 0.49f, 0.66f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.30f, 0.41f, 0.80f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.41f, 0.57f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.37f, 0.50f, 0.68f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.24f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.36f, 0.50f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.32f, 0.44f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.29f, 0.38f, 1.00f);
}

bool PrimaryButton(const char *label, const ImVec2 &size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.43f, 0.70f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.33f, 0.53f, 0.82f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.39f, 0.64f, 1.00f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void SectionTitle(const char *title)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.86f, 1.0f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}
