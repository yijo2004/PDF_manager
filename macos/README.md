# Building PDF Manager for macOS

The macOS build targets Apple Silicon and macOS 13 or newer. It creates a
Finder-launchable app containing PDFium and the application font.

## Prerequisites

Install Apple's Command Line Tools and CMake:

```sh
xcode-select --install
brew install cmake
```

## Build

From the repository root:

```sh
sh macos/build.sh
```

The result is `build-macos/PDF Manager.app`. Open it from Finder or run:

```sh
open "build-macos/PDF Manager.app"
```

The first configuration downloads pinned GLFW and arm64 PDFium archives.
Setlists are stored in `~/Library/Application Support/PDF Manager/setlists.dat`.

The bundle is ad-hoc signed for local use. Public distribution would require a
Developer ID certificate and Apple notarization.
