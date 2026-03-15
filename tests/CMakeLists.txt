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

# Test sources (kendi main'i olmayan, Catch2 main kullanır)
set(TEST_SOURCES
    test_network.cpp
    test_hydraulic.cpp
    test_geometry.cpp
)

# Test executable
add_executable(vkt_tests ${TEST_SOURCES})

target_link_libraries(vkt_tests
    Catch2::Catch2WithMain
)

# Project source files that tests need (UI/Vulkan hariç)
target_sources(vkt_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/mep/NetworkGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/mep/HydraulicSolver.cpp
    ${CMAKE_SOURCE_DIR}/src/mep/Database.cpp
)

# CTest entegrasyonu
include(CTest)
include(Catch)
catch_discover_tests(vkt_tests)
