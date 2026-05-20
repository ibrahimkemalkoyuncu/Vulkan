# CLAUDE.md — VKT Mekanik Tesisat Draw

Vulkan + Qt6 tabanlı, Türkiye/AB standartlarına uygun profesyonel MEP (Mekanik-Elektrik-Tesisat) CAD uygulaması. **Birincil hedef: 4M FINE SANI'yi her açıdan geride bırakmak** — daha hızlı render, daha doğru hidrolik hesaplar, daha modern arayüz ve açık genişletilebilir mimari.

---

## Persistent Context (Notion)

Detaylı bilgi Notion'da. CLAUDE.md değişmeyen kuralları taşır; iş anına özgü detay Notion'dan çekilir. Notion MCP üzerinden erişim açık.

- **VKT Hub**: https://www.notion.so/35b0d0232488811d9638ebbb449c41bd
- **Project Bible**: https://www.notion.so/35b0d02324888117ac03e0db4f22b45c
- **Modül Durumu**: https://www.notion.so/35b0d0232488814c9835ce8065aafc22
- **Karar Günlüğü**: https://www.notion.so/35b0d02324888177a743dac9ebe7da10
- **Aktif Session**: https://www.notion.so/35b0d0232488814c807df1fb78e67843
- **FineSANI Referansı** (rakip analizi + domain bilgisi): https://www.notion.so/3600d0232488818f8913cc3ce64fd853
  - Sistem Mantığı & İş Akışı: https://www.notion.so/3600d023248881eba83ae6e05f815bd9
  - Hesaplama Motoru: https://www.notion.so/3600d02324888164ac13e46288c1b7dc
  - Çıktı & Pafta Yönetimi: https://www.notion.so/3600d023248881e4b4c4c223b3abc404
  - Domain Sözlüğü: https://www.notion.so/3600d0232488815cb900f1ebf24aae52

### Context Loading Protocol

**Session başında (her zaman):**
1. Project Bible'ı oku
2. Aktif Session'ı oku
3. Durumu özetle, sıradaki adımı söyle

**Görev FineSANI karşılaştırması / UX / domain içeriyorsa:**
- Sadece ilgili FineSANI Referansı alt sayfasını çek (tümünü yükleme — token israfı)

**Session sonunda:**
- Aktif Session sayfasını güncelle (tamamlanan / yarım kalan / sonraki adım)
- Mimari karar verildiyse Karar Günlüğü'ne entry ekle

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
- **MEP overlay label'ları** — Edge (DN boyutu, açık mavi) + Node (armatür tipi, renge göre) SnapOverlay'de gösteriliyor
- **Junction undo/redo** — AddNodeCommand ile Ctrl+Z desteği
- **MTEXT word-wrap** — `Text::m_rectWidth` (DXF code 41 / DWG rect_width), `SnapOverlay` word-wrap, `maxWidthPx` zoom-aware dönüşümü
- **DWG MTEXT iyileştirmesi** — `x_axis_dir`'den rotation parse, `attachment` → hAlign/vAlign
- **Pan sırasında overlay sync** — `VulkanWindow::mouseMoveEvent()` pan'da `m_onViewportChange` çağrıyor
- **MEP node label collision avoidance** — greedy placement, çakışan label'lar yukarı kaydırılıyor
- **Gerçek zamanlı hidrolik (FineSANI differentiator)** — boru/armatür eklenince/silinince 600ms debounce ile `RunAutoHydro()` tetikleniyor; `HydraulicSolver` + EN 806-3 DN seçimi sessizce çalışır, edge label güncellenir, overlay yenilenir
- **RunAutoHydro topoloji tabanlı LU** — DFS ile Source'dan downstream fixture LU hesabı; her edge gerçek taşıma kapasitesine göre DN alıyor (eşit dağılım fallback ile)
- **DWG xref circular ref koruması** — `m_visitedXrefs` (canonical path) + `m_missingXrefs` kaydı; nested xref'lerde de koruma; DXFImportDialog'da kayıp xref uyarısı
- **DWG xref arama dizini** — `DWGReader::AddXrefSearchPath()` + `GetMissingXrefs()`; DXFImportDialog'da QMessageBox ile kullanıcıdan dizin isteme + re-read
- **Undo/Redo → AutoHydro** — `OnUndo()` / `OnRedo()` sonrası `ScheduleAutoHydro()` çağrısı; MEP ağı geri alınca DN label'lar otomatik güncelleniyor
- **Birim testleri: text rotation + MTEXT word-wrap** — `x_axis_dir`→derece dönüşümü (6 test), greedy word-wrap algoritması (8 test); `tests/test_geometry.cpp`
- **CTest Windows encoding düzeltmesi** — TEST_CASE isimlerindeki em dash (`—`) → ASCII `-`; 79/79 test ctest ile geçiyor
- **Ellipse entity class** — `vkt/cad/Ellipse.hpp` + `src/cad/Ellipse.cpp`; parametrik depolama (center, semiMajor, axisRatio, rotAngle, startParam/endParam); DWGReader artık `Polyline` yerine `Ellipse*` döndürüyor; DXFReader'a `ReadEllipse()` eklendi (group codes 10–42); VulkanRenderer'da adaptive tessellation (ppu-tabanlı, Circle/Arc ile aynı yaklaşım); 6 birim testi — 85/85 geçiyor
- **Spline entity class** — `vkt/cad/Spline.hpp` + `src/cad/Spline.cpp`; fit noktaları veya kontrol noktası+knot+derece depolama; Tessellate() De Boor B-spline; DWGReader artık `Spline*` döndürüyor; DXFReader'a `ReadSpline()` eklendi; VulkanRenderer adaptive render; 5 birim testi — 90/90 geçiyor
- **DXFWriter Arc + Ellipse export** — `WriteEntityArc()` (ARC group 50/51) + `WriteEntityEllipse()` (ELLIPSE major-axis vektörü + axis-ratio); WriteEntitiesSection() switch'e eklendi
- **MimariBelirleDialog** — `vkt/ui/MimariBelirleDialog.hpp` + `src/ui/MimariBelirleDialog.cpp`; QFormLayout + QTableWidget; kat no/kot/isim/DXF-DWG dosyası tanımlama; ApplyToFloorManager(); Tamam'da DXFImportDialog ile otomatik import zinciri; Ctrl+M kısayolu
- **Mimari menüsü** — MainWindow menü çubuğuna "Mimari" menüsü eklendi; "Mimari Belirle..." eylemi
- **UZAKLIK/DISTANCE komutu** — CommandBar'dan `UZAKLIK`, `DISTANCE` veya `DIST` komutu; iki nokta tıklama ile mm + m mesafe hesabı; ESC ile iptal; mesafe log paneline ve status bar'a yazılır
- **FloorManager MainWindow entegrasyonu** — `m_floorManager` üyesi; kat tanımları session boyunca korunur; 90/90 test geçiyor
- **W-Block referans noktası hizalama** — `Floor::refX/refY` alanları; `MimariBelirleDialog`'da "Referans Noktası" X/Y spinbox; `DXFReader::SetInsertionOffset()` tüm entity'leri `(-refX, -refY)` kadar kaydırır; `DXFImportDialog::SetInsertionOffset()` ile pipeline'a aktarım; `Eğitim.md`'de W-Block baz noktası detaylı anlatım
- **ProjectManager (CC klasörü eşdeğeri)** — `vkt/core/ProjectManager.hpp` + `src/core/ProjectManager.cpp`; Singleton; kök dizin → proje alt klasörü → `mimari/`, `cikti/`, `rapor/` alt dizinleri; `CreateProject()` std::filesystem ile; `QSettings` ile kalıcı kök dizin ayarı; MainWindow: Yeni Proje (Ctrl+Shift+N), Proje Kök Klasörü Ayarla, Proje Klasörünü Aç; `OnOpen/SaveAs/ExportReport` proje klasörüne duyarlı; `MimariBelirleDialog`: "mimari/ klasörüne kopyala" checkbox; `Eğitim.md`'de CC klasörü iş akışı 90/90 test korunuyor
- **Global W-Block referans noktası** — `MimariBelirleDialog`'da tek "Global Referans Noktası" X/Y spinbox (per-floor yerine); `FloorDef`'den refX/Y kaldırıldı; `ApplyToFloorManager()` tüm katlara aynı refX/Y atar; `OnMimariBelirle()` `dlg.GetGlobalRefX/Y()` ile tüm `DXFImportDialog`'a aynı offset geçer; tablo 4 sütuna indirildi (Kat No/Kot/İsim/Dosya)
- **Otomatik birim dönüşümü ($INSUNITS → mm)** — `DXFReader::Read()` sonunda `header.GetScaleToMM()` ≠ 1.0 ise tüm entity'lere `Scale(scale, scale, scale)` + insertionOffset ölçekleme; `DXFImportDialog` import sonrası status etiketine birim bilgisi ekler (örn. "Birim: metre → ×1000")
- **ST Cihazları paneli** — `vkt/ui/STFixturePanel.hpp` + `src/ui/STFixturePanel.cpp`; QDockWidget "ST Cihazları" sekmesi; TS EN 806-3 T.3 fixture listesi (Lavabo/Duş/WC/Küvet/Evye/Bulaşık+Çamaşır Makinesi/Pisuar/Bide) LU değerleriyle; çift tıklayınca `FixtureSelected()` sinyali → `MainWindow::OnSTFixtureSelected()` → `PlaceFixture` modu; `Database::InitializeFixtures()`'a Küvet (LU=3.0) eklendi; `Eğitim.md` bölüm 8–10 güncellendi (ST cihazları, boru boyları/debiler, fosseptik standartları TS 822 + EN 12566-1)
- **Açık uç real-time uyarısı** — `SnapOverlay::SetOpenEndMarkers()`; `RefreshTextOverlay()` içinde tüm node'lar taranır: 0 kenarlı her node ve Junction ile 1 kenarlı dead-end → kırmızı halka + ünlem işareti overlay'de render; boru bağlandığında otomatik kaybolur
- **Pis su modülü UI** — `ToolMode::PlaceDrain` eklendi; `m_currentPipeType` (EdgeType::Supply/Drainage) ile boru tipi seçimi; `OnDrawDrainPipe()` → kahverengi boru; `OnPlaceYerSuzgeci()` + `OnPlaceRogar()` → Drain node yerleştirme; Çizim menüsüne "Pis Su Borusu / Yer Süzgeci / Rögar" eklendi; CommandBar: `PIS-SU`, `YER-SUZGECI`, `ROGAR`; ESC ile m_currentPipeType Supply'a sıfırlanır; 90/90 test geçiyor
- **Tesisat Kopyalama (FloorCopy)** — `OnCopyFloor()`: QInputDialog ile kaynak+hedef kat seçimi; kaynak kattaki tüm node'lar (z ≈ srcZ, tolerans 0.1m) yeni z'ye kopyalanır; her iki ucu kaynak katta olan edge'ler (yatay borular) kopyalanır — kolonlar (uçları farklı z'de) otomatik hariç tutulur; `CompositeCommand` ile tam Undo/Redo; Çizim menüsü + `KOPYA-KAT` CommandBar komutu
- **Kullanıcı_kitabı.md** — Kapsamlı kullanıcı kılavuzu oluşturuldu (14 bölüm): proje oluşturma, mimari altlık, ST cihazları, temiz su çizimi, pis su sistemi, tesisat kopyalama, hidrolik analiz, görsel doğrulama, snap/ölçüm, proje yönetimi, komut referansı, dosya formatları, hesap föyü, fosseptik standartları
- **Cihazları Tesisata Bağla (BAGLA)** — `ToolMode::ConnectFixture`; 2-adım: (1) armatür node seç, (2) boru hattını tıkla → dik ayak noktasında Junction eklenir + dal boru armatür→junction; `CompositeCommand` ile undo/redo; Ctrl+B + `BAGLA`/`CONNECT` komut; FineSANI'nin en sık kullanılan özelliği artık VKT'de
- **DIN 1988-300 norm seçimi** — `HydroNorm` enum (EN806_3/DIN1988); `HydraulicSolver::GlobalNorm()` static singleton; DIN modunda eşzamanlılık faktörü φ = 1/(1+√LU/10); Analiz → Hesap Normu + `NORM` komutu; seçim sonrası otomatik yeniden hesaplama
- **Hidrofor Boyutlandırma** — `OnHidrofor()`: `CalculateCriticalPath()` sonuçlarını rich-text dialog ile gösterir; kritik basınç kaybı kırmızı, önerilen pompa modeli + güç; Analiz menüsü + `HIDROFOR` komutu
- **Yağmur Suyu Modülü (EN 12056-3)** — `OnYagmurSuyu()`: alan + yüzey tipi (C katsayısı) + r_D=0.03 l/(s·m²) ile Q hesabı; DN seçim tablosu; kaç boru gerektiğini hesaplar; Analiz menüsü + `YAGMUR` komutu
- **Keşif Listesi / BOM** — `OnBOM()`: NetworkGraph'tan DN'e göre boru metraj toplamı; T-parça + dirsek + armatür bağlantı sayımı; rich-text dialog + log kaydı; Ctrl+K + `BOM`/`KESIF` komutu
- **DWGReader SetInsertionOffset** — `DWGReader::m_insertionOffsetX/Y`; `Read()` sonrası tüm entity'lere `Move(-x, -y)` uygulanır; `DXFImportDialog` DWG dalında da `SetInsertionOffset` çağrısı eklendi; DXFReader ile aynı davranış
- **Kolon Şeması (Riser Diagram) UI** — `OnRiserDiagram()`: `RiserDiagram::Generate() + ToSVG()` çağrısı; QTextBrowser'da SVG önizleme + metin özeti; "SVG Olarak Kaydet" butonu (rapor/ klasörüne); Ctrl+R + `RISER` komutu
- **Hesap Föyü DN Manuel Override** — `OnDNOverride()`: QTableWidget ile tüm edge'lerin DN tablosu; satır başına QComboBox (16→200 DN serisi); Tamam → anında uygulama + overlay refresh; `DN-OVERRIDE`/`DN-DEGISTIR` komutu
- **Riser PDF Export** — `OnRiserDiagram()` "PDF Kaydet" butonu: `QPrinter(PdfFormat)` + `QSvgRenderer` → A3 Landscape PDF; "SVG Kaydet" butonu da eklendi
- **Hesap Föyü XLS Export** — `XLSXWriter::ExportCalculationSheet()`: Özet + Boru Hesap Föyü (ID/Tip/Malzeme/DN/L/Q/v/dH) + Armatür sekmeli .xls; `OnDNOverride()` "XLS Olarak Kaydet" butonundan tetiklenir; solver önce çalıştırılır
- **NewProjectDialog** — `vkt/ui/NewProjectDialog.hpp` + `src/ui/NewProjectDialog.cpp`; Proje Bilgileri (ad/müşteri/mühendis, klasör önizleme) + Teknik Parametreler (bina tipi/norm/tarih); norm seçimi `HydroNorm::GlobalNorm()`'a uygulanır; Ctrl+Shift+N
- **FloorAlignmentDialog** — `vkt/ui/FloorAlignmentDialog.hpp` + `src/ui/FloorAlignmentDialog.cpp`; 3D hizalama kontrolü; 6 sütunlu tablo (Kat/İsim/Kot/Yükseklik/Node sayısı/Durum); kırmızı=kot çakışma, sarı=boş kat; `ApplyChanges()` FloorManager'ı günceller; Ctrl+Shift+H / `HIZALAMA`
- **RiserDiagram SVG iyileştirmesi** — `ToSVG()` yeniden yazıldı: viewBox, beyaz arka plan border, başlık/alt çizgi, çift sıra mavi alternating (#f5f8ff), stroke-linecap=round, kat etiketleri koyu/bold, DN etiketleri mavi (#2255aa), kolon başlıkları siyah/bold, sağ alt lejant kutusu (Temiz Su + Pis Su)
- **Kolon Bağlantı Asistanı** — `OnDrawColumn()`: 2-adım dialog (kaynak node listesi → hedef kat seçimi); aynı XY'de hedef katta node bulma (50mm/0.15m tolerans); yoksa Junction oluştur; dikey boru uzunluğu Z farkından; CompositeCommand ile undo/redo; Ctrl+Shift+K + `KOLON`/`COLUMN`/`DIKEY-BORU` komutu
- **Test kapsamı genişletme** — `test_riser.cpp`'ye 8 yeni test: SVG viewBox/legend/alternating/linecap, kolon dz hesabı, node-at-elevation arama algoritması, FloorManager null dönüşü, elevation aralik disi
- **Boru tipi görsel ayrımı** — `VulkanRenderer::GetEdgeColor(type, isColumn)`: kolon temiz su=cyan, kolon pis su=turuncu; yatay Supply=açık mavi, yatay Drainage=kahverengi; `RefreshTextOverlay()` edge label renkleri boru tipine göre (cyan/turuncu/mavi/kahverengi) + kolon label'lara `[K]` prefix
- **Sıcak Su Modülü** — `EdgeType::HotWater` (kırmızı boru); `NodeType::HotSource` (şofben/kazan, kırmızı düğüm); `OnDrawHotWaterPipe()` + `OnPlaceHotSource()`; HydraulicSolver HotWater'ı Supply gibi çözer; PrintLayout/SVG'de kırmızı çizgi; `SICAK-SU`, `SOFBEN`, `KAZAN` komutları
- **Tesisatı Kabul Et** — `OnTesistatKabul()`: açık uç + kaynak kontrolü, hata/uyarı listesi; tüm borular P-/SK-/PS- prefix ile numaralandırma; RunAutoHydro tetikleme; özet dialog; `Ctrl+Enter` + `KABUL`/`ACCEPT` komutu
- **Batch BAGLA** — `ConnectFixture` modu çoklu armatür seçimi destekler; `m_batchFixtureIds` vektörü; fixture tıklamalarında listeye ekleme, pipe tıklamasında tümünü tek CompositeCommand ile bağlama; status bar seçili sayıyı gösterir
- **HotWater/HotSource unit testleri** — `test_hydraulic.cpp`'ye 6 test eklendi (18 assertion): NodeType::HotSource roundtrip, EdgeType::HotWater roundtrip, pozitif debi, pozitif head loss, AutoSize standart DN, karışık ağ; 108/108 test geçiyor; test_riser.cpp legend etiketi "Soguk Su" uyumlu hale getirildi
- **Eğitim.md Bölüm 25-26** — Sıcak Su Modülü + Tesisatı Kabul Et; tam iş akışı özeti SICAK-SU/SOFBEN/KABUL adımları dahil güncellendi
- **Kullanıcı_kitabı.md Bölüm 25-26** — Bölüm 25 Sıcak Su + Bölüm 26 Tesisatı Kabul Et; hızlı başlangıç iş akışı 21 adıma güncellendi
- **Devre Seçenekleri dialog** — `vkt/ui/DevreSecenekleriDialog.hpp` + `src/ui/DevreSecenekleriDialog.cpp`; bina tipi/boru cinsi/pürüzlülük/max hız/norm tek dialog; değişince tüm borulara uygulanır + AutoHydro tetiklenir; Ctrl+Shift+D + `DEVRE` komutu
- **Baskı İçeriği** — `OnBaskiIcerigi()`: çizimde hangi etiketler görünsün checkbox seçici (DN/debi/uzunluk/hız/basınç kaybı); `m_labelShow*` bool flag'leri; `RefreshTextOverlay()` güncellendi — etiket bileşenleri birleştirilir; `BASKI` komutu
- **Parçaların Basınç Kaybı** — `OnBaskiKaybi()`: tüm Supply/HotWater edgelerin HTML tablosu; kritik devre sarı vurgulu; toplam + kritik kayıp özeti; PDF kaydet; `BASINC`/`PARCALAR` komutu
- **Word/HTML Rapor Export** — `OnWordRapor()`: devre parametreleri + boru hesap föyü + kritik devre + armatür listesi; `.htm` dosya (Word + tarayıcı açar); `rapor/` klasörüne otomatik yol; `WORD`/`HTML-RAPOR` komutu
- **MinGW Ninja build fix** — `catch_discover_tests DISCOVERY_MODE PRE_TEST` + `vcpkg bin PATH`; 0xc0000135 DLL hatası giderildi
- **Otomatik kolon tespiti** — `NetworkGraph::IsColumnEdge(edgeId)`: dx<50mm, dy<50mm, dz>0.3m → dikey boru; `GetColumnEdges()`: tüm kolon edge ID'lerini döndürür; 4 birim testi eklendi
- **Çok katlı izometrik önizleme** — `VulkanRenderer::UpdateNetworkVertexData()`: `m_viewMode != Plan` iken Z metre → mm dönüşümü (`×1000`); hem edge vertex'leri hem node vertex'leri Z-skalı; izometrik görünümde bağlantılar gerçek 3D yüksekliklerinde render edilir
- **Boru malzeme seçici düzeltmesi** — DrawPipe + OnDrawColumn: sabit "PVC" → `m_propMaterial->currentText()` + `Database::GetPipe().roughness_mm`; özellik paneli artık gerçekten etkili
- **PrintLayoutDialog** — `vkt/ui/PrintLayoutDialog.hpp` + `src/ui/PrintLayoutDialog.cpp`; A3/A4/Yatay/Dikey sayfa seçimi; otomatik/manuel ölçek; ISO 7200 başlık bloğu (proje adı/pafta/firma/çizen/tarih); ProjectManager'dan otomatik doldurma; PDF + SVG çıktı; Ctrl+P + `PAFTA`/`PRINT` komutu
- **Akıllı Bağlantı Noktası (SmartPoint)** — FineSANI "akıllı bağlantı noktaları" eşdeğeri; `fixtureType="SmartPoint"` Fixture node; GPU renderda magenta × sembolü (lineVertices); overlay'de `+ Baglanti` etiketi; STFixturePanel'e `m_cbSmartPoint` checkbox + `SmartPointModeChanged` sinyali; `OnItemActivated` SmartPoint mode'da "SmartPoint" gönderir; `AKILLI`/`AKILLI-BAGLANTI`/`SMART-POINT` komutları
- **Uygulama Katman Görünürlüğü** — FineSANI "uygulama katmanlarını seç" eşdeğeri; `m_showTemizSu/m_showSicakSu/m_showPisSu` bool flag'leri; `OnLayerVisibility()` dialog (3 checkbox); `VulkanRenderer::SetLayerVisibility()` + `m_showTemizSu/Su/PisSu` member'lar; `UpdateNetworkVertexData()` edge tipi filtrelemesi; `RefreshTextOverlay()` edge label filtrelemesi; `VulkanWindow::SetLayerVisibility()` passthrough; Ctrl+Shift+L + `KATMAN`/`LAYER-VIS` komutu; Görünüm menüsüne eklendi
- **Boşaltma Noktası (Ana Tahliye)** — `OnBosaltmaNoktasi()`: en düşük Z'li Drain node'u otomatik seçer → `m_mainDrainNodeId`; overlay'de turuncu "[ANA TAHLİYE]" etiketi; `BOSALTMA`/`ANA-TAHLIYE` komutu; Çizim menüsüne eklendi
- **MinGW PATH fix (ctest)** — `catch_discover_tests PROPERTIES ENVIRONMENT` hardcoded PATH (MinGW + vcpkg + System32); `$ENV{PATH}` configure-time genişlemesi yerine sabit path; 108/108 test geçiyor
- **Pis Su Hesap Föyü** — `OnPisSuHesapFoyu()`: drenaj edge'leri DN/L/DU/Q/eğim/doluluk QTableWidget; h/d>%50 kırmızı, h/d %40-50 sarı; `Analiz → Pis Su Hesap Foyu` + `PIS-HESAP`/`PIS-SU-HESAP` komutu
- **Çizimi Güncelle** — `OnCizimiGuncelle()`: hangi değerlerin çizime yazılacağını seçim dialog; seçilen label flag'lerini açar + `ScheduleAutoHydro()` tetikler; Ctrl+Shift+U + `GUNCELLE`/`CIZIMI-GUNCELLE` komutu; FineSANI "Çizimi Güncelle" eşdeğeri
- **Kapalı Çukur / Foseptik Hesabı** — `OnFoseptik()`: TS 822 + EN 12566-1; kişi sayısı × günlük tüketim × bekleme süresi; çamur hacmi (+%30); çift odalı seçenek (%67/%33); anlık sonuç + tahliye sıklığı; `Analiz → Kapali Cukur` + `FOSEPTIK`/`KAPALI-CUKUR`/`SEPTIK` komutu
- **Manning fillRate hesabı** — `SolveDrainage()`: Manning kapasitesi ters çözümü (binary search 40 iter) → `edge.fillRate = h/d`; `%50` EN 12056 sınırı kontrolü Hesap Föyü'nde vurgulanıyor
- **Baskı İçeriği — Pis Su etiketleri** — `m_labelShowSlope` + `m_labelShowFillRate`; `OnBaskiIcerigi()` dialog'a "Pis Su ek etiketleri" bölümü eklendi; `RefreshTextOverlay()` Drainage edge'lerde i(%) ve h/d(%) etiketi
- **Eğitim.md Bölüm 30** — Pis Su Tesisat Hesapları: PIS-HESAP, BASKI, GUNCELLE, FOSEPTIK; tam iş akışı tablosu
- **Pis Su Hesap Föyü "Boru Cinsi" sütunu** — `IsColumnEdge()` kullanarak yatay/kolon ayrımı; 9 sütunlu tablo
- **Emdirme Çukuru** — `OnEmdirmeCukuru()`: toprak emme kapasitesi hesabı; perkolasyon testi düzeltmesi; `EMDIRME`/`EMDIRME-CUKURU` komutu
- **Pis Su Çukuru** — `OnPisSuCukuru()`: geçirimsiz depolama tankı; tanker aralığı + emniyet katsayısı; silindir tank boyutu; `PIS-CUKUR` komutu
- **Pis Su Pompası** — `OnPisSuPompasi()`: DIN EN 12050-1; Q/v/manometrik yükseklik/güç; standart motor seçimi; `PIS-POMPA` komutu
- **Kolon Şeması DXF Export** — `OnRiserDiagram()` "DXF Kaydet" butonu; R12 format LINE + TEXT entity; riser diyagramı artık SVG/PDF/DXF olarak export edilebiliyor

### Devam Eden

(tüm planlanan özellikler tamamlandı — yeni hedefler için Aktif Session sayfasına bak)

### Planlanan

- Multi-threading: GPU Vulkan senkronizasyonu ayrı thread

---

## Bilinen Kısıtlar

1. **DWG xref**: `ExpandXref()` temel desteği var; karmaşık nested xref zincirleri hâlâ tam desteklenmiyor
2. **Mahal tespiti**: T-kesişim ve iç bölme duvarı geometrilerinde planar embedding bozulabilir
3. **Undo/Redo**: Disk'e yazılmıyor, session bazlı
4. **Shader**: SPIR-V `.spv` dosyaları runtime derleniyor, dinamik GLSL derleme yok
5. **Multi-threading**: UI ve Vulkan senkronizasyonu tek thread
6. **GPU Clash**: `RunClashDetectionGPU` host-coherent bellek kullanır; büyük projelerde device-local + staging buffer ile daha hızlı olur

---

## Domain Disiplini (FineSANI'den Miras)

FineSANI'nin pain point'lerinden çıkarılan 5 altın kural — VKT'nin **asla** tekrarlamayacağı hatalar:

1. **Birim daima metre** — cm/mm karışıklığı yok, runtime'da otomatik algılama + dönüşüm
2. **Z-ekseni bağlantılarında perpendicular zorunlu** — kolon ↔ yatay hat snap'i otomatik öneri
3. **Tek başlangıç noktası enforcement** — bir binada tek su kaynağı, sistem seviyesinde kural
4. **Kolonlar kopyalama dışı** — `CopyFloor` komutu kolonları otomatik filtreler
5. **Non-destructive editing** — "explode" tarzı geri dönüşsüz işlem yok, her zaman editable

Detay, FineSANI iş akışı ve VKT fırsatları için: **FineSANI Referansı** (Notion — yukarıda link).