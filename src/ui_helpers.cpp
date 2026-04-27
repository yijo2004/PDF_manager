#include "ui_helpers.h"

#include "imgui.h"

void ApplyCustomTheme()
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 5.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.SeparatorTextPadding = ImVec2(8.0f, 3.0f);
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.078f, 0.086f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.110f, 0.122f, 0.96f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.090f, 0.095f, 0.105f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.095f, 0.100f, 0.112f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.220f, 0.235f, 0.260f, 0.85f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.095f, 0.100f, 0.112f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.130f, 0.145f, 0.165f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.170f, 0.190f, 0.220f, 0.85f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.235f, 0.285f, 0.355f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.270f, 0.340f, 0.430f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.155f, 0.175f, 0.205f, 0.95f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.220f, 0.275f, 0.350f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.255f, 0.330f, 0.430f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.120f, 0.130f, 0.148f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.170f, 0.195f, 0.230f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.200f, 0.240f, 0.295f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.120f, 0.135f, 0.158f, 0.95f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.220f, 0.275f, 0.350f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.175f, 0.215f, 0.275f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.220f, 0.235f, 0.260f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.390f, 0.570f, 0.800f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.330f, 0.480f, 0.680f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.420f, 0.600f, 0.830f, 1.00f);
}

bool PrimaryButton(const char *label, const ImVec2 &size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.255f, 0.390f, 0.570f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.330f, 0.500f, 0.720f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.220f, 0.335f, 0.500f, 1.00f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void SectionTitle(const char *title)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.78f, 0.86f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}
