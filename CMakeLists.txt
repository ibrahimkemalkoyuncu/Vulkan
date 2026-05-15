cmake_minimum_required(VERSION 3.20)
project(MekanikTesisatDraw VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt MOC/UIC/RCC otomatik işleme
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# vcpkg toolchain (LibreDWG için)
if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Vulkan SDK
find_package(Vulkan REQUIRED)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Svg)

# LibreDWG (DWG dosya okuma) - Enabled with basic entity support
set(LIBREDWG_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include/libredwg")
set(LIBREDWG_LIBRARY "C:/vcpkg/installed/x64-windows/lib/libredwg.lib")

if(EXISTS "${LIBREDWG_INCLUDE_DIR}/dwg.h" AND EXISTS "${LIBREDWG_LIBRARY}")
    add_compile_definitions(HAVE_LIBREDWG)
    set(LIBREDWG_FOUND TRUE)
    message(STATUS "LibreDWG found - DWG support ENABLED (basic entities)")
else()
    set(LIBREDWG_FOUND FALSE)
    message(WARNING "LibreDWG not found - DWG support DISABLED")
endif()

# ── Shader compilation (glslc) ──────────────────────────────────────────────
find_program(GLSLC_EXECUTABLE glslc
    HINTS "$ENV{VULKAN_SDK}/Bin"
          "C:/VulkanSDK/1.3.296.0/Bin"
          "C:/VulkanSDK/1.3.275.0/Bin"
          "C:/VulkanSDK/1.3.261.1/Bin")

if(GLSLC_EXECUTABLE)
    set(SHADER_SRC_DIR  "${CMAKE_SOURCE_DIR}/shaders")
    set(SHADER_OUT_DIR  "${CMAKE_BINARY_DIR}/bin/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

    set(SHADER_SOURCES
        basic.vert  basic.frag
        gbuffer.vert  gbuffer.frag
        fullscreen.vert
        ssao.frag  ssao_blur.frag  composite.frag
        clash_detection.comp
    )

    set(SPV_FILES "")
    foreach(SHADER ${SHADER_SOURCES})
        set(SRC "${SHADER_SRC_DIR}/${SHADER}")
        set(SPV "${SHADER_OUT_DIR}/${SHADER}.spv")
        add_custom_command(
            OUTPUT  ${SPV}
            COMMAND ${GLSLC_EXECUTABLE} ${SRC} -o ${SPV}
            DEPENDS ${SRC}
            COMMENT "Compiling shader: ${SHADER}"
            VERBATIM)
        list(APPEND SPV_FILES ${SPV})
    endforeach()

    add_custom_target(VKTShaders ALL DEPENDS ${SPV_FILES})
    message(STATUS "glslc found — SSAO shaders will be compiled at build time")
else()
    message(WARNING "glslc not found — SSAO will be disabled at runtime (shaders/*.spv missing)")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/vkt
    ${CMAKE_SOURCE_DIR}/third_party
    ${Vulkan_INCLUDE_DIRS}
)

if(LIBREDWG_FOUND)
    include_directories(${LIBREDWG_INCLUDE_DIR})
    # Also add parent directory for direct includes
    include_directories("C:/vcpkg/installed/x64-windows/include")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/Document.cpp
    src/core/Persistence.cpp
    src/core/FloorManager.cpp
    src/core/ProjectManager.cpp
    src/renderer/VulkanRenderer.cpp
    src/renderer/VulkanWindow.cpp
    src/renderer/Gizmo.cpp
    src/ui/MainWindow.cpp
    src/ui/DXFImportDialog.cpp
    src/ui/MimariBelirleDialog.cpp
    src/ui/STFixturePanel.cpp
    src/ui/SpacePanel.cpp
    src/ui/PrintLayout.cpp
    src/ui/FixturePropertiesPanel.cpp
    src/ui/CommandBar.cpp
    src/ui/SnapOverlay.cpp
    src/ui/PBRMaterialEditor.cpp
    src/mep/NetworkGraph.cpp
    src/mep/HydraulicSolver.cpp
    src/mep/Database.cpp
    src/mep/ScheduleGenerator.cpp
    src/mep/RiserDiagram.cpp
    src/mep/ClashDetection.cpp
    src/mep/XLSXWriter.cpp
    src/geom/BRep.cpp
    src/cad/Entity.cpp
    src/cad/Layer.cpp
    src/cad/LayerManager.cpp
    src/cad/Line.cpp
    src/cad/Polyline.cpp
    src/cad/Arc.cpp
    src/cad/Circle.cpp
    src/cad/Ellipse.cpp
    src/cad/Spline.cpp
    src/cad/SelectionManager.cpp
    src/cad/Viewport.cpp
    src/cad/Space.cpp
    src/cad/Dimension.cpp
    src/cad/Text.cpp
    src/cad/Hatch.cpp
    src/cad/FixtureSymbolLibrary.cpp
    src/cad/DXFReader.cpp
    src/cad/DXFWriter.cpp
    src/cad/DWGReader.cpp  # Placeholder implementation (UI support)
    src/cad/SpaceManager.cpp
    src/cad/SnapManager.cpp
)

# Header files
set(HEADERS
    vkt/core/Application.hpp
    vkt/core/Document.hpp
    vkt/core/Command.hpp
    vkt/core/Persistence.hpp
    vkt/core/FloorManager.hpp
    vkt/core/ProjectManager.hpp
    vkt/geom/Math.hpp
    vkt/geom/Quaternion.hpp
    vkt/geom/BRep.hpp
    vkt/mep/NetworkGraph.hpp
    vkt/mep/HydraulicSolver.hpp
    vkt/mep/MaterialProperties.hpp
    vkt/mep/Database.hpp
    vkt/mep/ScheduleGenerator.hpp
    vkt/mep/RiserDiagram.hpp
    vkt/render/VulkanRenderer.hpp
    vkt/render/VulkanWindow.hpp
    vkt/render/Gizmo.hpp
    vkt/ui/MainWindow.hpp
    vkt/ui/DXFImportDialog.hpp
    vkt/ui/MimariBelirleDialog.hpp
    vkt/ui/STFixturePanel.hpp
    vkt/ui/SpacePanel.hpp
    vkt/ui/PrintLayout.hpp
    vkt/ui/FixturePropertiesPanel.hpp
    vkt/ui/CommandBar.hpp
    vkt/ui/SnapOverlay.hpp
    vkt/ui/PBRMaterialEditor.hpp
    vkt/cad/Entity.hpp
    vkt/cad/Layer.hpp
    vkt/cad/LayerManager.hpp
    vkt/cad/Line.hpp
    vkt/cad/Polyline.hpp
    vkt/cad/Arc.hpp
    vkt/cad/Circle.hpp
    vkt/cad/SelectionManager.hpp
    vkt/cad/Viewport.hpp
    vkt/cad/Space.hpp
    vkt/cad/Dimension.hpp
    vkt/cad/Text.hpp
    vkt/cad/FixtureSymbolLibrary.hpp
    vkt/cad/DXFReader.hpp
    vkt/cad/DXFWriter.hpp
    vkt/cad/DWGReader.hpp
    vkt/cad/SpaceManager.hpp
    vkt/cad/SnapManager.hpp
    vkt/core/Commands.hpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

if(GLSLC_EXECUTABLE)
    add_dependencies(${PROJECT_NAME} VKTShaders)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    Qt6::Svg
)

# Opsiyonel: LibreDWG
if(LIBREDWG_FOUND)
    target_link_libraries(${PROJECT_NAME} ${LIBREDWG_LIBRARY})
endif()

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Unit Tests (opsiyonel, BUILD_TESTING=ON ile aktif)
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
