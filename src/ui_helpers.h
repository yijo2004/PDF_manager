#pragma once

#include "imgui.h"

void ApplyCustomTheme();
bool PrimaryButton(const char *label, const ImVec2 &size);
bool SecondaryButton(const char *label, const ImVec2 &size);
bool SubtleButton(const char *label, const ImVec2 &size);
bool DangerButton(const char *label, const ImVec2 &size);
void SectionTitle(const char *title);
void StatusBadge(const char *label, const ImVec4 &color);
