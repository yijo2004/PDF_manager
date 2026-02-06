cmake_minimum_required(VERSION 3.14)
project(PdfViewer)

set(CMAKE_CXX_STANDARD 20)

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
set(APP_SOURCES
    src/main.cpp
    src/pdf_viewer.cpp
    src/pdf_library.cpp
    src/file_dialog.cpp
    src/setlist_gen.cpp
)

set(APP_HEADERS
    src/pdf_viewer.h
    src/pdf_library.h
    src/file_dialog.h
    src/setlist_gen.h
)

# Remove WIN32 to make console show up
add_executable(PdfApp WIN32 ${APP_SOURCES} ${APP_HEADERS})

# Include src directory for our headers
target_include_directories(PdfApp PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link everything together
# We link "glfw3" (the name of the .lib file inside lib-vc2022) and "opengl32" (standard Windows lib)
# "comdlg32" is needed for the Windows file dialog
# "ole32" and "shell32" are needed for the folder picker (IFileDialog)
target_link_libraries(PdfApp PRIVATE imgui ${PDFIUM_LIB} glfw3 opengl32 comdlg32 ole32 shell32)
if(MSVC)
    set_target_properties(PdfApp PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()