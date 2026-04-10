#pragma once

struct GLFWwindow;

GLFWwindow *InitWindow(int width, int height, const char *title);
void InitPDFium();
void InitImGui(GLFWwindow *window, const char *glslVersion);
void Shutdown(GLFWwindow *window);
