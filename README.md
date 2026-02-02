# PDF Viewer (ImGui + PDFium)

A simple PDF viewer built with C++, ImGui, PDFium, and GLFW.

## Dependencies
* **ImGui** (Docking Branch)
* **PDFium** (Google's PDF rendering engine)
* **GLFW** (Windowing)
* **OpenGL 3**

## Directory Structure
Ensure your `vendor/` folder is set up correctly:
* `vendor/imgui/` (Source files)
* `vendor/glfw/` (Has `include/` and `lib-vc2022/`)
* `vendor/pdfium/` (Has `include/`, `lib/`, and `bin/`)

## How to Build (Windows)

1.  **Generate Project**
    ```powershell
    mkdir build
    cd build
    cmake ..
    ```

2.  **Compile**
    ```powershell
    cmake --build .
    ```

3.  **Setup Runtime (Crucial)**
    Copy the PDFium DLL to your executable folder:
    ```powershell
    copy ..\vendor\pdfium\bin\pdfium.dll .\Debug\
    ```

4.  **Run**
    ```powershell
    .\Debug\PdfApp.exe
    ```