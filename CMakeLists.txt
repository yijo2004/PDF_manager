cmake_minimum_required(VERSION 3.14)
project(PdfViewer)

set(CMAKE_CXX_STANDARD 17)

# -----------------------------------------------------------------------------
# 1. Setup Paths (Manual Layout)
# -----------------------------------------------------------------------------
# We assume you put your libraries in the "vendor" folder
set(VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor")

# --- Setup GLFW Manually ---
set(GLFW_PATH "${VENDOR_DIR}/glfw")
include_directories(${GLFW_PATH}/include)

# Point to the library folder for Visual Studio (vc2022 is standard for recent VS)
# If you are using an older VS, pick the folder that matches your version
link_directories(${GLFW_PATH}/lib-vc2022) 

# --- Setup PDFium Manually ---
set(PDFIUM_PATH "${VENDOR_DIR}/pdfium")
include_directories(${PDFIUM_PATH}/include)

if(WIN32)
    # On Windows, we link the .dll.lib import library
    set(PDFIUM_LIB "${PDFIUM_PATH}/lib/pdfium.dll.lib")
else()
    set(PDFIUM_LIB "${PDFIUM_PATH}/lib/libpdfium.a")
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

# -----------------------------------------------------------------------------
# 3. Setup Main Executable
# -----------------------------------------------------------------------------
add_executable(PdfApp src/main.cpp)

# Link everything together
# We link "glfw3" (the name of the .lib file inside lib-vc2022) and "opengl32" (standard Windows lib)
target_link_libraries(PdfApp PRIVATE imgui ${PDFIUM_LIB} glfw3 opengl32)