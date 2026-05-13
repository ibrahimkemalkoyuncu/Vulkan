# CLAUDE.md — VKT Mekanik Tesisat Draw

Vulkan + Qt6 tabanlı, Türkiye/AB standartlarına uygun profesyonel MEP (Mekanik-Elektrik-Tesisat) CAD uygulaması. **Birincil hedef: 4M FINE SANI'yi her açıdan geride bırakmak** — daha hızlı render, daha doğru hidrolik hesaplar, daha modern arayüz ve açık genişletilebilir mimari.

---

## Neden Bu Proje Var?

**4M FINE SANI**, Türkiye'de yaygın kullanılan tesisat CAD + hesap yazılımıdır ancak temel kısıtları vardır:

| Kısıt | VKT'nin Hedefi |
|-------|---------------|
| Yavaş, eski render motoru (GDI/OpenGL eski) | Vulkan — GPU-native, yüzlerce bin entity akıcı |
| Kapalı, genişletilemeyen mimari | Modüler C++17, açık katmanlar |
| Sınırlı DXF/DWG desteği | DXFReader + LibreDWG entegrasyonu |
| Manuel hidrolik hesap akışı | Otomatik çözücü — LU/DU, Darcy-Weisbach, kritik yol |
| Zayıf undo/redo | Full Command Pattern, sınırsız geçmiş |
| Modern standart uyumu eksik | TS EN 806-3 + EN 12056-2 tam implementasyon |

Her yeni özellik veya karar için şu soruyu sor: **"Bu 4M FINE SANI'nin yapamadığı ya da kötü yaptığı bir şey mi?"** — Cevap evet ise önceliklendir.

---

## Proje Genel Bakışı

- **Amaç**: Su tesisatı ve drenaj sistemleri tasarımı + hidrolik analiz
- **Rakip Hedef**: 4M FINE SANI'yi teknik ve kullanılabilirlik olarak geride bırakmak
- **Standartlar**: TS EN 806-3 (su temini), EN 12056-2 (drenaj/atıksu)
- **Platform**: Windows (Win32 + Vulkan + Qt6)
- **Dil**: C++17
- **Lisans**: Proprietary (© 2026)
- **Versiyon**: 1.0.0

---

## Tech Stack

| Katman | Teknoloji |
|--------|-----------|
| Grafik | Vulkan (GPU-hızlandırmalı render) |
| UI | Qt6 (Core, Widgets, Gui, Signals/Slots) |
| Build | CMake 3.20+, CMakePresets.json |
| Paket | vcpkg (LibreDWG opsiyonel) |
| Serialization | nlohmann/json (third_party/) |
| Test | Catch2 |
| Shader | GLSL (Vulkan SPIR-V — glslc gerekli) |

---

## Dizin Yapısı

```
vulkan/
├── src/                    # Implementasyon dosyaları (.cpp)
│   ├── main.cpp
│   ├── core/               # Uygulama yaşam döngüsü, doküman, komutlar
│   ├── cad/                # CAD entity'leri, katmanlar, DXF/DWG okuyucu
│   ├── geom/               # B-Rep ve geometri motorları
│   ├── mep/                # MEP mühendislik hesapları
│   ├── renderer/           # Vulkan render pipeline
│   └── ui/                 # Qt6 arayüz sınıfları
├── vkt/                    # Public header dosyaları (.hpp)
│   ├── core/, cad/, geom/, mep/, render/, ui/
├── shaders/                # GLSL vertex/fragment shader'ları
│   ├── basic.vert
│   └── basic.frag
├── tests/                  # Catch2 birim testleri
│   ├── test_hydraulic.cpp
│   ├── test_network.cpp
│   └── test_geometry.cpp
├── third_party/            # nlohmann/json.hpp
└── .github/copilot-instructions.md  # Mimari rehber
```

---

## Build

### Gereksinimler

- CMake 3.20+
- Qt6 (Core, Gui, Widgets) — test edilmiş: Qt 6.10.1
- Vulkan SDK (`glslc` shader derleyici dahil)
- Visual Studio 2022 (MSVC)
- vcpkg (LibreDWG desteği için opsiyonel)

### Build Komutları

```bash
# Debug build (testler dahil)
cmake -S . -B build/default -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64" \
  -DBUILD_TESTING=ON
cmake --build build/default

# Preset ile
cmake --preset default
cmake --build --preset default-build

# Release
cmake --preset release
cmake --build --preset release-build

# Testleri çalıştır
ctest --build-config Debug --output-on-failure -V
```

### Çalıştırma

```bash
./build/default/bin/MekanikTesisatDraw.exe
# veya
run_vkt.bat
```

---

## Mimari

### Katman Diyagramı

```
Qt6 UI Layer (ui)
      ↓
Core Layer — Application (Singleton), Document, CommandManager
      ↓
 ┌────────────┬────────────┬────────────┐
CAD Layer   MEP Layer   Geom Layer
(cad)       (mep)       (geom)
      ↓
Vulkan Renderer (render)
```

### Tasarım Desenleri

| Desen | Kullanım |
|-------|----------|
| Singleton | `Application`, `Database` |
| Command | Undo/Redo → `CommandManager` |
| Composite | Document → Entity koleksiyonu |
| Strategy | `HydraulicSolver` (supply vs drainage mod) |
| Observer | Qt Signals/Slots (veri → UI güncellemesi) |
| Graph | `NetworkGraph` (borular, bağlantılar, armatürler) |

### Veri Akışı

```
Kullanıcı Etkileşimi (Mouse/Keyboard)
  → Qt Event (MainWindow slot)
  → Command Object (ör. DrawPipeCommand)
  → Document::ExecuteCommand()
      ├→ CommandHistory (Undo/Redo stack)
      ├→ NetworkGraph güncelle
      └→ CAD entity ekle
  → VulkanRenderer::Render()
      ├→ GPU vertex buffer senkronizasyonu
      └→ Vulkan draw komutları
  → Ekran çıktısı
```

---

## Temel Modüller

### `vkt::core` — Uygulama Çekirdeği

- **Application**: Singleton, doküman yaşam döngüsü
- **Document**: Bir CAD projesinin tüm verisi — `NetworkGraph` + `vector<Entity>` + katman haritası + komut geçmişi + JSON serialize/deserialize
- **Command / CommandManager**: Abstract `Execute()` / `Undo()` base class + undo/redo stack

### `vkt::cad` — CAD Katmanı

- **Entity** (abstract): `uint64_t` ID, `EntityType`, renk, katman, dönüşümler (`Move`, `Rotate`, `Scale`, `Mirror`), `Clone()`, `Serialize()`/`Deserialize()`
- Subclass'lar: `Line`, `Polyline`, `Circle`, `Arc`, `Text`, `Space`
- **DXFReader / DWGReader**: Dosya format parse edici
- **SelectionManager**: Ray-cast tabanlı seçim
- **SpaceManager**: Kapalı polyline'lardan + segment birleştirme ile mahal tespiti; otomatik tip tahmini (Türkçe/İngilizce isim eşleştirme)
- **SnapManager**: Endpoint/Midpoint/Center/Nearest + Intersection (line-line, polyline) + Perpendicular + adaptive grid; öncelik tabanlı seçim
- **FloorManager**: Çok katlı bina yönetimi
- **DXFWriter**: DXF export
- **Dimension / Text / FixtureSymbolLibrary**: Ölçülendirme, metin, armatür sembol kütüphanesi
- **Viewport**: 2D/3D görünüm dönüşümleri

### `vkt::mep` — MEP Mühendislik Motoru

**NetworkGraph** node/edge tipleri:

| Nodes | Edges |
|-------|-------|
| Junction, Fixture, Source | Supply, Drainage, Vent |
| Tank, Pump, Drain | |

**HydraulicSolver** hesap adımları:

1. **Su Temini (TS EN 806-3)**:
   - LU (Load Unit) → debi dönüşümü
   - Darcy-Weisbach: `ΔP = f * (L/D) * (ρv²/2)`
   - Haaland friction faktörü (türbülanslı/laminar)
   - Kritik yol analizi → pompa kafası boyutlandırma

2. **Drenaj (EN 12056-2)**:
   - DU (Discharge Unit) toplaması
   - Manning denklemi: `Q = (1/n) * A * R_h^(2/3) * S^(1/2)`
   - K faktörü (Konut=0.5, Otel=0.7, Endüstri=1.0)

### `vkt::geom` — Geometri Motoru

- **Math.hpp**: `Vec3`, `Mat4`, `Ray`, `Camera`, `BoundingBox`; `Ray::IntersectsPlane()` + `Ray::IntersectsCylinder()` (gizmo hit-test)
- **Quaternion.hpp**: Quaternion rotasyon matematiği (3D gizmo için)
- **BRep**: Shoelace alan hesabı, nokta-poligon testi, santroid, çakışma tespiti

### `vkt::render` — Vulkan Renderer

- Vulkan instance, device, swapchain yönetimi
- G-Buffer render pass (gbuffer.vert/frag) + SSAO pass + blur pass + composite pass
- PBR shading: GGX Cook-Torrance BRDF, Reinhard tone mapping (composite.frag)
- Pipeline'lar: line (CAD/borular), triangle (node'lar/dolgu), gbuffer line/triangle, gizmo
- Max 2 frame in-flight (`MAX_FRAMES_IN_FLIGHT=2`)
- Görünüm modları: Plan (2D), İzometrik (3D aksonometrik), Perspektif
- **Gizmo**: Translate (silindir hit-test) + Rotate (çember ring hit-test) — `vkt/render/Gizmo.hpp`
- **CompositePushConstants**: lightDir, roughness, metalness, ambient (32 byte)
- **Viewport**: ortografik projeksiyon, Vulkan Y-flip (`proj[5] = -2/h`), zoom/pan/extents
- **GPU Clash Compute**: `shaders/clash_detection.comp` (16×16 local group), `RunClashDetectionGPU()` — SSBO pipe/arch/result, atomic counter, host readback
- **SSAO**: tuned params (radius=0.35, bias=0.015, power=1.8), standard rangeCheck, symmetric 4×4 bilateral blur, randomized kernel scale

---

## Kodlama Kuralları

```cpp
// Namespace: tüm kod vkt:: altında
namespace vkt::cad { ... }
namespace vkt::mep { ... }

// Header'larda "using namespace" kullanma

// Entity ID tipi
vkt::cad::EntityId id;  // uint64_t, static counter

// Ownership: unique_ptr
auto entity = std::make_unique<Line>(p1, p2);
document->AddEntity(std::move(entity));

// Command pattern ile değişiklik
auto cmd = std::make_unique<DrawPipeCommand>(nodeA, nodeB, params);
document->ExecuteCommand(std::move(cmd));

// CAD toleransı
constexpr double CAD_TOLERANCE = 0.001;  // 1mm
```

---

## Görev Referans Tablosu

| Görev | Değiştirilecek Dosya(lar) |
|-------|--------------------------|
| Yeni entity tipi ekle | `vkt/cad/Entity.hpp` → inherit + yeni `.hpp`/`.cpp` |
| MEP hesabı ekle | `vkt/mep/HydraulicSolver.hpp` + `src/mep/HydraulicSolver.cpp` |
| UI menü/buton | `src/ui/MainWindow.cpp` → `CreateMenus()` |
| Rendering hatası | `src/renderer/VulkanRenderer.cpp` → `Render()` |
| Malzeme tablosu | `src/mep/Database.cpp` → `InitializePipes()` |
| Geometri algoritması | `vkt/geom/BRep.hpp` + `src/geom/BRep.cpp` |
| Yeni dosya formatı | Yeni `Reader.hpp`/`.cpp` çifti oluştur |
| Test ekle | `tests/test_*.cpp` — Catch2 TEST_CASE |

---

## Test

```bash
# Tüm testler
ctest --output-on-failure -V

# Modül bazlı
./build/bin/test_hydraulic    # Darcy-Weisbach, LU/DU dönüşüm
./build/bin/test_network      # Graf topoloji
./build/bin/test_geometry     # BRep, ray-cast, alan
```

Mühendislik formülleri standartlara karşı doğrulanmadan commit edilmemeli.

---

## Tamamlanan / Devam Eden / Planlanan

### Tamamlanan
- Entity sistemi (serialization dahil)
- DXF import + DXF export (DXFWriter)
- DWG import (LibreDWG — temel entity'ler, MINSERT expand, arc/circle fit)
- **DWG renk doğruluğu**: UTF-16LE layer isimleri (`Utf16LeToUtf8`), method=0xC3 ACI index fix, INSERT/MINSERT layer mirası, DXFReader `SetLayerName` düzeltmesi
- Vulkan render — G-Buffer + SSAO + PBR composite (3 görünüm modu)
- PBR malzeme sistemi (GGX Cook-Torrance, MaterialProperties lookup tablosu)
- 3D transform gizmo (Translate + Rotate, silindir/ring hit-test, Quaternion)
- Viewport Y-flip düzeltmesi (Vulkan Y-down → CAD Y-up)
- Zoom-to-fit (percentile-based extents, otomatik DXF import sonrası)
- B-Rep mahal tespiti + otomatik import sonrası tespit
- Hidrolik çözücü (supply + drainage, Hardy-Cross kapalı döngü)
- Kritik yol + pompa boyutlandırma
- Undo/Redo
- Katman yönetimi
- Malzeme veritabanı
- Clash detection (ClashDetection modülü)
- Excel/PDF rapor export (XLSXWriter)
- Riser diyagramı (RiserDiagram modülü)
- Çok katlı bina desteği (FloorManager)
- Snap overlay (SnapOverlay UI)
- Komut satırı (CommandBar — PIPE/FIXTURE/JUNCTION/SOURCE/DRAIN/VALVE/PUMP)
- Armatür özellik paneli (FixturePropertiesPanel)
- Baskı düzeni (PrintLayout)
- Boyut/ölçü entity'leri (Dimension)
- Metin entity'leri (Text)
- Armatür sembol kütüphanesi (FixtureSymbolLibrary)
- Catch2 birim testleri (cad, dxf_import, hydraulic, persistence, riser, dwg_colors)
- SpaceManager segment birleştirme — LINE'lardan oda tespiti (planar half-edge face traversal)
- DXF `$INSUNITS` okuma + birim dönüşümü
- Akıllı snap sistemi — öncelik tabanlı (Endpoint > Center > Intersection > Midpoint > Perpendicular > Nearest > Grid)
- Clash detection GPU compute shader (SSBO, atomic counter)
- PBR Malzeme Editörü UI paneli
- MEP boru çizim tool — state machine, iki noktalı boru + zincir çizim, NetworkGraph entegrasyonu
- **Rubber-band önizleme** — çizerken ikinci tıklamadan önce canlı önizleme çizgisi
- **ESC iptal** — aktif çizim modunu tuş ile iptal etme
- **DrawPipeCommand (Command Pattern)** — boru/armatür/kavşak işlemleri undo/redo stack'e kaydediliyor
- **Armatür tipi seçici** — Lavabo/Duş/WC/Mutfak/Bataryolu combo ile fixture tip seçimi
- **DN otomatik boyutlandırma UI** — çizilen ağdan tek tıkla EN 806-3 hesabı, sonuçları boru label olarak render

### Devam Eden
- (tüm planlanan özellikler tamamlandı)

### Planlanan
- DWG xref zinciri (nested external reference tam desteği)
- Multi-threading: GPU Vulkan senkronizasyonu ayrı thread
- GPU Clash: device-local + staging buffer ile büyük proje performansı

---

## Bilinen Kısıtlar

1. **DWG xref**: `ExpandXref()` temel desteği var; karmaşık nested xref zincirleri ve döngüsel referans koruması eksik
2. **Mahal tespiti**: T-kesişim ve iç bölme duvarı geometrilerinde planar embedding bozulabilir
3. **Undo/Redo**: Disk'e yazılmıyor, session bazlı
4. **Shader**: SPIR-V `.spv` dosyaları runtime derleniyor, dinamik GLSL derleme yok
5. **Multi-threading**: UI ve Vulkan senkronizasyonu tek thread
6. **GPU Clash**: `RunClashDetectionGPU` host-coherent bellek kullanır; büyük projelerde device-local + staging buffer ile daha hızlı olur
