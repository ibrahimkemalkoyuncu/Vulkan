# Test framework: Catch2 (header-only, single include)
# Download catch2 header if not present
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Include project headers
include_directories(
    ${CMAKE_SOURCE_DIR}/vkt
    ${CMAKE_SOURCE_DIR}/third_party
)

# LibreDWG (DWGReader için)
set(LIBREDWG_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include/libredwg")
set(LIBREDWG_LIBRARY     "C:/vcpkg/installed/x64-windows/lib/libredwg.lib")
if(EXISTS "${LIBREDWG_LIBRARY}")
    add_compile_definitions(HAVE_LIBREDWG)
    include_directories(${LIBREDWG_INCLUDE_DIR})
endif()

# Test sources (kendi main'i olmayan, Catch2 main kullanır)
set(TEST_SOURCES
    test_network.cpp
    test_hydraulic.cpp
    test_geometry.cpp
    test_riser.cpp
    test_cad.cpp
    test_persistence.cpp
    test_dxf_import.cpp
    test_dwg_colors.cpp
)

# Test executable
add_executable(vkt_tests ${TEST_SOURCES})

target_link_libraries(vkt_tests
    Catch2::Catch2WithMain
)
if(EXISTS "${LIBREDWG_LIBRARY}")
    target_link_libraries(vkt_tests ${LIBREDWG_LIBRARY})
endif()

# Project source files that tests need (UI/Vulkan hariç)
target_sources(vkt_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mep/NetworkGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/mep/HydraulicSolver.cpp
    ${CMAKE_SOURCE_DIR}/src/mep/Database.cpp
    ${CMAKE_SOURCE_DIR}/src/mep/RiserDiagram.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FloorManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ProjectManager.cpp
    # CAD entity kaynakları
    ${CMAKE_SOURCE_DIR}/src/cad/Entity.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Line.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Circle.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Arc.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Ellipse.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Spline.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Polyline.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Layer.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/LayerManager.cpp
    # Persistence
    ${CMAKE_SOURCE_DIR}/src/core/Document.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Persistence.cpp
    # DXF reader
    ${CMAKE_SOURCE_DIR}/src/cad/DXFReader.cpp
    # DWG reader (LibreDWG bağımlılığı var)
    ${CMAKE_SOURCE_DIR}/src/cad/DWGReader.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Text.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Hatch.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Dimension.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/Space.cpp
    ${CMAKE_SOURCE_DIR}/src/cad/SpaceManager.cpp
)

# CTest entegrasyonu
include(CTest)
include(Catch)
catch_discover_tests(vkt_tests)
