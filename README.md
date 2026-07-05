# PDF Manager

PDF Manager is a desktop PDF viewer and setlist manager built with C++20,
Dear ImGui, PDFium, GLFW, and OpenGL 3.

## Supported Platforms

| Platform | Architecture | Toolchain |
| --- | --- | --- |
| macOS 13 or later | Apple Silicon (`arm64`) | Xcode Command Line Tools |
| Windows | 64-bit (`x64`) | Visual Studio 2022 |

Linux and Intel macOS builds are not currently configured.

## Dependencies

The repository includes the Dear ImGui sources used by both platforms. Other
dependencies are handled differently:

- **macOS:** CMake downloads pinned GLFW 3.4 and ARM64 PDFium archives during
  the first configure. Internet access is therefore required for a clean build.
- **Windows:** GLFW headers/libraries and PDFium headers/import library are read
  from `vendor/`. The runtime `pdfium.dll` is stored at the repository root.

Both builds require `font.ttf` at the repository root. CMake packages the font
and PDFium runtime with the application automatically.

## Build on macOS

Install CMake and the Xcode Command Line Tools first:

```sh
xcode-select --install
brew install cmake
```

The macOS generator is single-configuration, so Debug and Release use separate
build directories.

### Debug

```sh
cmake -S . -B build-macos-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos-debug --target PdfApp --parallel
open "build-macos-debug/PDF Manager.app"
```

### Release

```sh
cmake -S . -B build-macos-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos-release --target PdfApp --parallel
open "build-macos-release/PDF Manager.app"
```

The result is a self-contained application bundle containing `font.ttf` and
`libpdfium.dylib`:

```text
build-macos-<configuration>/PDF Manager.app
```

The build adjusts the PDFium runtime path and applies an ad-hoc signature. This
is suitable for local use; distributing the app requires a Developer ID
signature and notarization.

## Build on Windows

Install CMake and Visual Studio 2022 with the **Desktop development with C++**
workload. Run these commands from PowerShell. Visual Studio is a
multi-configuration generator, so Debug and Release share one build directory.

### Configure

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
```

### Debug

```powershell
cmake --build build-windows --config Debug --target PdfApp --parallel
.\build-windows\Debug\PdfApp.exe
```

### Release

```powershell
cmake --build build-windows --config Release --target PdfApp --parallel
.\build-windows\Release\PdfApp.exe
```

CMake copies `pdfium.dll` and `font.ttf` beside the executable after each
build.

## Debug and Release Differences

- **Debug** prioritizes debugging: compiler optimizations are reduced and debug
  information is generated. It is larger and slower but easier to inspect in a
  debugger.
- **Release** enables compiler optimizations and is the appropriate build for
  normal use and distribution testing.
- PDFium is a prebuilt dependency on both platforms; selecting Debug does not
  rebuild PDFium as a debug library.

## Platform Behavior Differences

- Both platforms use native file and folder dialogs.
- On macOS, the green window button uses zoom/maximize instead of native
  fullscreen Space. Native fullscreen can leave the deprecated NSOpenGL/GLFW
  surface rendering without functional keyboard or mouse input on current
  macOS versions.
- ImGui panels can dock inside the main window on both platforms. Detached
  operating-system windows are disabled on macOS and remain available on
  Windows.
- macOS produces an application bundle named `PDF Manager.app`; Windows
  produces `PdfApp.exe`.

## Repository Layout

```text
src/                 Application source
macos/               macOS bundle metadata
vendor/imgui/        Dear ImGui source and backends
vendor/glfw/         Windows GLFW headers and libraries
vendor/pdfium/       Windows PDFium headers and import library
font.ttf             Runtime UI font
pdfium.dll           Windows PDFium runtime
CMakeLists.txt       Cross-platform build configuration
```
