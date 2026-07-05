#pragma once

struct GLFWwindow;

GLFWwindow *InitWindow(int width, int height, const char *title);
void InitPDFium();
void InitImGui(GLFWwindow *window, const char *glslVersion);
void ApplyAppFont(GLFWwindow *window, bool manualMode, int fontSizePx);
int ChooseAutoAppFontSizePx(GLFWwindow *window);
void Shutdown(GLFWwindow *window);
