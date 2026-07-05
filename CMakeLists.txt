cmake_minimum_required(VERSION 3.14)
if(APPLE)
    project(PdfViewer LANGUAGES C CXX OBJC OBJCXX)
else()
    project(PdfViewer LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# 1. Setup Paths (Manual Layout)
# -----------------------------------------------------------------------------
# We assume you put your libraries in the "vendor" folder
set(VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor")

if(WIN32)
    # --- Windows GLFW (prebuilt Visual Studio libraries) ---
    set(GLFW_PATH "${VENDOR_DIR}/glfw")
    include_directories(${GLFW_PATH}/include)
    link_directories(${GLFW_PATH}/lib-vc2022)

    # --- Windows PDFium (prebuilt import library) ---
    set(PDFIUM_PATH "${VENDOR_DIR}/pdfium")
    include_directories(${PDFIUM_PATH}/include)
    # On Windows, we link the .dll.lib import library
    set(PDFIUM_LIB "${PDFIUM_PATH}/lib/pdfium.dll.lib")
endif()

if(APPLE)
    # -------------------------------------------------------------------------
    # macOS dependencies
    # Keep these separate from the existing Windows vendor layout. Both
    # archives are pinned so clean builds remain reproducible.
    # -------------------------------------------------------------------------
    include(FetchContent)
    cmake_policy(PUSH)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw_macos
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
    )
    FetchContent_MakeAvailable(glfw_macos)

    FetchContent_Declare(
        pdfium_macos
        URL https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F7665/pdfium-mac-arm64.tgz
        URL_HASH SHA256=e5e3d7c92365394cf001754a191db4732dac5de13d06085902b6880f3aac3814
    )
    FetchContent_GetProperties(pdfium_macos)
    if(NOT pdfium_macos_POPULATED)
        FetchContent_Populate(pdfium_macos)
    endif()

    set(PDFIUM_PATH "${pdfium_macos_SOURCE_DIR}")
    set(PDFIUM_LIB "${PDFIUM_PATH}/lib/libpdfium.dylib")
    include_directories(${PDFIUM_PATH}/include)
    cmake_policy(POP)
endif()

# -----------------------------------------------------------------------------
# 2. Setup ImGui
# -----------------------------------------------------------------------------
set(IMGUI_DIR "${VENDOR_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
if(APPLE)
    # The ImGui GLFW backend includes GLFW headers directly.
    target_link_libraries(imgui PUBLIC glfw)
endif()

# -----------------------------------------------------------------------------
# 3. Setup Main Executable
# -----------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/app_init.cpp
    src/ui_helpers.cpp
    src/ui_panels.cpp
    src/pdf_viewer.cpp
    src/pdf_library.cpp
    src/setlist_gen.cpp
)

# File dialogs have native implementations on Windows and macOS. Other
# platforms continue to use the existing stub implementation.
if(APPLE)
    list(APPEND APP_SOURCES
        src/file_dialog_macos.mm
        src/macos_window.mm
    )
else()
    list(APPEND APP_SOURCES src/file_dialog.cpp)
endif()

set(APP_HEADERS
    src/app_init.h
    src/ui_helpers.h
    src/ui_panels.h
    src/pdf_viewer.h
    src/pdf_library.h
    src/file_dialog.h
    src/setlist_gen.h
)

if(APPLE)
    list(APPEND APP_HEADERS src/macos_window.h)
endif()

if(APPLE)
    add_executable(PdfApp MACOSX_BUNDLE ${APP_SOURCES} ${APP_HEADERS})
else()
    # Remove WIN32 to make the console show up on Windows.
    add_executable(PdfApp WIN32 ${APP_SOURCES} ${APP_HEADERS})
endif()

# Include src directory for our headers
target_include_directories(PdfApp PRIVATE ${CMAKE_SOURCE_DIR}/src)

# -----------------------------------------------------------------------------
# 4. Platform-specific linking and packaging
# -----------------------------------------------------------------------------
if(WIN32)
    # comdlg32 provides the file dialog; ole32 and shell32 provide the folder
    # picker. Keep the existing Visual Studio library name for GLFW.
    target_link_libraries(PdfApp PRIVATE
        imgui ${PDFIUM_LIB} glfw3 opengl32 comdlg32 ole32 shell32)
endif()

if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)

    target_link_libraries(PdfApp PRIVATE
        imgui
        glfw
        ${PDFIUM_LIB}
        ${COCOA_FRAMEWORK}
        ${OPENGL_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
    target_compile_definitions(PdfApp PRIVATE GL_SILENCE_DEPRECATION)

    set_target_properties(PdfApp PROPERTIES
        OUTPUT_NAME "PDF Manager"
        MACOSX_BUNDLE_BUNDLE_NAME "PDF Manager"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pdfmanager.app"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist.in"
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

    add_custom_command(TARGET PdfApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:PdfApp>/Contents/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:PdfApp>/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PDFIUM_LIB}"
            "$<TARGET_BUNDLE_DIR:PdfApp>/Contents/Frameworks/libpdfium.dylib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/font.ttf"
            "$<TARGET_BUNDLE_DIR:PdfApp>/Contents/Resources/font.ttf"
        COMMAND install_name_tool -id "@rpath/libpdfium.dylib"
            "$<TARGET_BUNDLE_DIR:PdfApp>/Contents/Frameworks/libpdfium.dylib"
        COMMAND install_name_tool -change "./libpdfium.dylib"
            "@rpath/libpdfium.dylib" "$<TARGET_FILE:PdfApp>"
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:PdfApp>"
        COMMENT "Bundling macOS runtime files and applying an ad-hoc signature"
        VERBATIM
    )
endif()

if(MSVC)
    set_target_properties(PdfApp PROPERTIES
        LINK_FLAGS "/ENTRY:mainCRTStartup"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PdfApp>"
    )
endif()

if(WIN32)
    add_custom_command(TARGET PdfApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/font.ttf"
            "$<TARGET_FILE_DIR:PdfApp>/font.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/pdfium.dll"
            "$<TARGET_FILE_DIR:PdfApp>/pdfium.dll"
    )
endif()
