# Copilot Instructions for VKT (Mekanik Tesisat Draw)

**Project Overview**: Professional Plumbing/MEP CAD software using C++17, Vulkan, and Qt6.
**Goal**: High-performance, standard-compliant (TS EN 806-3, EN 12056) engineering CAD.

## 🏗 Architecture & Patterns

- **Modular Design**:
  - `vkt::core`: App lifecycle (`Application`), data (`Document`), and Undo/Redo (`Command` pattern).
  - `vkt::cad`: Scene graph entities (`Entity` base in `vkt/cad/Entity.hpp`).
  - `vkt::mep`: Engineering logic. `HydraulicSolver` handles pressure loss/flow calcs. `NetworkGraph` represents connectivity.
  - `vkt::render`: Vulkan-based rendering engine. Uses BVH for picking and runtime Shader compilation (`ShaderCompiler`).
  - `vkt::ui`: Qt6 Widgets backend.

- **Key Data Types**:
  - **EntityId**: Defined as `using EntityId = uint64_t;` in `vkt::` namespace (not `vkt::cad::`). Use `vkt::EntityId`.
  - **Color**: `vkt::cad::Color` struct. Check implementation for missing colors (Yellow/Cyan/Magenta might need adding).
  - **Geometry**: `vkt::geom` namespace holds `Vec3`, `BoundingBox`. Use fully qualified names or explicit using directives.
  - **Polymorphism**: `Entity` uses `std::unique_ptr`. Ensure base class declares virtual methods before overriding in children.

- **File Structure**:
  - **Headers**: `vkt/<module>/*.hpp` (Public API).
  - **Sources**: `src/<module>/*.cpp` (Implementation).
  - **Third Party**: `third_party/` (headers like `nlohmann/json.hpp`).

- **Engineering Standards**:
  - Strictly adhere to TS EN 806-3 (Water Supply) and EN 12056 (Drainage/Wastewater). 
  - `HydraulicSolver` is the source of truth for these calculations.

## 🛠 Build & Workflow

- **Build System**: CMake (min 3.20).
- **Dependencies**: 
  - REQUIRED: Qt6 (Core, Gui, Widgets), Vulkan SDK.
  - OPTIONAL: LibreDWG (via vcpkg) for `.dwg` support.
- **Commands**:
  - Build: `cmake -S . -B build` -> `cmake --build build --config Release`
  - Run: Executable is in `build/bin/Release/MekanikTesisatDraw.exe` (Windows).

## 💻 Coding Conventions

- **C++ Standard**: C++17. Use `std::unique_ptr` for ownership.
- **Namespaces**: 
  - Root: `vkt::`. 
  - Sub-namespaces: `cad`, `geom`, `mep`, `render`, `ui`.
  - Avoid `using namespace vkt::cad;` in headers. Explicitly qualify types like `vkt::geom::BoundingBox`.
- **Serialization**: Use `nlohmann::json` for saving/loading entities.
- **Rendering**: 
  - `VulkanRenderer` uses Deferred Rendering concepts.
  - Tessellation shaders used for smooth curves (Splines).

## 🚨 Critical Implementation Details

- **MEP Calculations**: When modifying `HydraulicSolver`, ensure `CalculateHeadLoss` or `OptimizeDrainageSlopes` remains compliant with physical formulas (Darcy-Weisbach, Haaland).
- **Entity Management**: 
  - New entities must inherit from `vkt::cad::Entity`.
  - Check `Entity.hpp` base definition before overriding methods (e.g., `Move`, `Scale`, `Rotate`, `Mirror`). If base lacks them, add virtual declarations first.
- **UI/Logic Separation**: `MainWindow` (Qt) manipulates `Document` (Core), which notifies generic listeners. Avoid business logic inside UI classes.

## 🔍 Context Files to Reference
- `vkt/core/Document.hpp`: For data management and command history.
- `vkt/mep/HydraulicSolver.hpp`: For understanding the engineering calculation engine.
- `vkt/cad/Entity.hpp`: Base class for entities; check for `EntityId`, `Color` and virtual methods.
- `vkt/geom/Math.hpp`: For `Vec3`, `BoundingBox` definitions.
