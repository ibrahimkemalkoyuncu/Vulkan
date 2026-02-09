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
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

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
    src/core/Window.cpp
    src/core/Persistence.cpp
    src/renderer/VulkanRenderer.cpp
    src/ui/UI.cpp
    src/ui/MainWindow.cpp
    src/ui/DXFImportDialog.cpp
    src/ui/SpacePanel.cpp
    src/mep/NetworkGraph.cpp
    src/mep/HydraulicSolver.cpp
    src/mep/Database.cpp
    src/mep/ScheduleGenerator.cpp
    src/geom/BRep.cpp
    src/cad/Entity.cpp
    src/cad/Layer.cpp
    src/cad/LayerManager.cpp
    src/cad/Line.cpp
    src/cad/Polyline.cpp
    src/cad/Arc.cpp
    src/cad/Circle.cpp
    src/cad/SelectionManager.cpp
    src/cad/Viewport.cpp
    src/cad/Space.cpp
    src/cad/DXFReader.cpp
    src/cad/DWGReader.cpp  # Placeholder implementation (UI support)
    src/cad/SpaceManager.cpp
)

# Header files
set(HEADERS
    vkt/core/Application.hpp
    vkt/core/Document.hpp
    vkt/core/Window.hpp
    vkt/core/Command.hpp
    vkt/core/Persistence.hpp
    vkt/geom/Math.hpp
    vkt/geom/BRep.hpp
    vkt/mep/NetworkGraph.hpp
    vkt/mep/HydraulicSolver.hpp
    vkt/mep/Database.hpp
    vkt/mep/ScheduleGenerator.hpp
    vkt/render/VulkanRenderer.hpp
    vkt/ui/UI.hpp
    vkt/ui/MainWindow.hpp
    vkt/ui/DXFImportDialog.hpp
    vkt/ui/SpacePanel.hpp
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
    vkt/cad/DXFReader.hpp
    vkt/cad/DWGReader.hpp
    vkt/cad/SpaceManager.hpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Opsiyonel: LibreDWG
if(LIBREDWG_FOUND)
    target_link_libraries(${PROJECT_NAME} ${LIBREDWG_LIBRARY})
endif()

# Output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
