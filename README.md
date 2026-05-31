# VKT — Mekanik Tesisat Draw

**Vulkan + Qt6 tabanlı profesyonel MEP (Su Tesisatı & Drenaj) CAD yazılımı**

Türkiye/AB standartlarına uygun tesisat tasarımı, hidrolik hesap ve pafta üretimi.  
Hedef: 4M FINE SANI'yi her açıdan geride bırakmak.

---

## Özellikler

### CAD Motoru
- DXF import (ASCII, tüm entity türleri, `$INSUNITS` birim dönüşümü, BLOCK/INSERT genişletme → BlockRegistry)
- DWG import (LibreDWG; INSERT/MINSERT genişletme, xref zincirleri A→B→C, TEXT/MTEXT/HATCH xref transform)
- DXF export (R2000 tam proje + kat bazlı)
- Snap sistemi: Endpoint › Center › Intersection › Midpoint › Perpendicular › Nearest › Grid (F3 toggle)
- AutoCAD uyumlu seçim: Window (soldan sağa, mavi) / Crossing (sağdan sola, yeşil), Shift+click multi-select
- Grip editing: Line/Arc/Circle/Polyline vertex grip noktaları — sürükle+bırak, Undo destekli
- Ortho modu (F8): yatay/dikey kısıt, boru çizimi hassasiyeti
- Katman yöneticisi: seçimle otomatik vurgulama, çift tık ile renk düzenleme
- Boyutlandırma (Dimension), Metin (Text/MText, word-wrap), Hatch, Spline, Ellipse
- Block/Insert altyapısı (BlockDef, BlockRegistry, Insert entity render)
- Fixture oto-tanıma: DXF/DWG import sonrası Text içeriğinden lavabo/WC/duş otomatik tespiti

### CAD Düzenleme Komutları (AutoCAD Uyumlu)

| Komut | Kısayol | Açıklama |
|-------|---------|---------|
| TRIM / KISALT | T | İki çizginin kesişimine kısalt (undo destekli) |
| OFFSET / PARALEL | O | Seçili entity'nin paralel kopyası |
| MIRROR / AYNA | M | Yatay/dikey eksen veya 180° yansıtma |
| ORTHO | F8 | Yatay/dikey kısıt toggle |
| SELECT ALL | Ctrl+A | Tüm görünür entity'leri seç |
| COPY | Ctrl+C | Seçili entity'leri kopyala |
| PASTE | Ctrl+V | Kopyalananları offset ile yapıştır |

### Performans
- Entity pick: Quad-tree (EntityGrid) — O(1) pick, 15K+ entity'de anlık
- Async CAD vertex build: >2000 entity'de arka planda, UI donmaz
- Network dirty flag: MEP vertex rebuild yalnızca değişimde
- Reactive validation: açık uç ve kaynak eksikliği anlık status bar

### MEP Mühendislik Motoru
- Temiz su (TS EN 806-3): LU→debi, Darcy-Weisbach, Haaland, kritik devre, pompa boyutlandırma
- Pis su (EN 12056-2): DU toplama, Manning, h/d doluluk (%50 sınır)
- Sıcak su: şofben/kazan, ayrı renk kodlu devre
- Yağmur suyu (EN 12056-3): çatı alanı × yüzey katsayısı × yoğunluk
- DIN 1988-300 alternatif norm; norm karşılaştırma tablosu
- Gerçek zamanlı AutoHydro (600 ms debounce, boru/armatür değişince otomatik hesap)
- Kritik devre 3D vurgulama (turuncu)

### Hesap Araçları
- Foseptik / Kapalı Çukur (TS 822 + EN 12566-1)
- Emdirme Çukuru, Pis Su Çukuru, Pis Su Pompası (DIN EN 12050-1)
- Membranlı Genleşme Tankı (EN 12828)
- Hidrofor boyutlandırma
- Norm Karşılaştırma (EN 806-3 vs DIN 1988-300)
- Hesap Kararı — "Neden Bu Çap?" analizi

### Çizim Araçları
- Boru çizim tool (state machine, rubber-band önizleme, zincir çizim)
- Armatür yerleştirme (2-tıklama: konum + yön)
- BAGLA — armatürü ana hatta dal boru + T-kavşak ile bağla (toplu seçim)
- Tesisat Kopyalama (KOPYA-KAT): katlar arası, kolonlar otomatik hariç
- Kolon Bağlantı Asistanı (KOLON): dikey boru, Z farkından otomatik uzunluk
- Tesisatı Kabul Et (KABUL): açık uç / kaynak kontrolü + P-/SK-/PS- numaralandırma
- DN Manuel Override + Baskı İçeriği seçici

### Görselleştirme
- Vulkan GPU render: G-Buffer + SSAO + PBR (GGX Cook-Torrance, Reinhard tone mapping)
- Plan (2D) / İzometrik / Perspektif görünüm
- 3D transform gizmo (Translate + Rotate, Quaternion)
- GPU Clash Detection (compute shader, SSBO, atomic counter)
- Renk kodlu boru tipleri: soğuk su (mavi/cyan), sıcak su (kırmızı/turuncu), pis su (kahverengi)

### Raporlama & Çıktı
- Kolon Şeması / Riser Diyagramı (SVG + PDF A3 + DXF R12)
- Keşif Listesi / BOM (DN × malzeme × metraj + **maliyet tahmini TL**)
- Hesap Föyü XLS (boru ID / Q / v / ΔH / DN)
- Word RTF Rapor (native RTF, Unicode escape, Türkçe tam destek)
- Pis Su Hesap Föyü (DN / DU / Q / eğim / h/d, kırmızı uyarı)
- PDF Pafta (A3/A4, ISO 7200 başlık bloğu, firma logosu, ölçek)
- SVG Pafta (base64 gömülü logo)

### Proje Yönetimi
- ProjectManager (CC Klasörü eşdeğeri): mimari/ cikti/ rapor/ alt dizinleri
- FloorManager: çok katlı, kot/yükseklik tanımı, 3D hizalama kontrolü
- NewProjectDialog: proje adı, norm, bina tipi

---

## Standartlar

| Standart | Kapsam |
|----------|--------|
| TS EN 806-3 | Su temini — LU, debi, Darcy-Weisbach |
| EN 12056-2 | Drenaj — DU, Manning, h/d |
| EN 12056-3 | Yağmur suyu — Q hesabı |
| DIN 1988-300 | Alman normu — simultanite faktörü |
| EN 12828 | Genleşme tankı |
| DIN EN 12050-1 | Pis su pompası |
| TS 822 + EN 12566-1 | Foseptik / kapalı çukur |

---

## Tech Stack

| Katman | Teknoloji |
|--------|-----------|
| Grafik | Vulkan 1.3 |
| UI | Qt 6.10 (Core, Widgets, Gui, PrintSupport, Svg) |
| Build | CMake 3.20+ + Ninja |
| Paket | vcpkg (LibreDWG) |
| Serialization | nlohmann/json |
| Test | Catch2 v3 — 118/118 geçiyor |
| Sürüm | v1.0.0 (configure_file → Version.hpp) |
| Installer | CPack NSIS (Windows, windeployqt ile Qt DLL paketi) |
| Shader | GLSL → SPIR-V (glslc) |

---

## Build

```bash
# Gereksinimler: Vulkan SDK, Qt 6.10, CMake 3.20+, MSVC 2022 veya MinGW 13

# Debug (MinGW — Qt Creator)
cmake --preset default   # veya Qt Creator ile aç
cmake --build --preset default-build

# Release
cmake --preset release
cmake --build --preset release-build

# Testler
ctest --output-on-failure -V
```

---

## Dizin Yapısı

```
vulkan/
├── src/                  # Implementasyon (.cpp)
│   ├── cad/              # DXFReader, DWGReader, Entity'ler, SnapManager
│   ├── core/             # Document, CommandManager, ProjectManager
│   ├── mep/              # HydraulicSolver, NetworkGraph, Database, RiserDiagram
│   ├── renderer/         # VulkanRenderer, VulkanWindow
│   └── ui/               # MainWindow, PrintLayout, tüm dialog'lar
├── vkt/                  # Public header'lar (.hpp)
├── shaders/              # GLSL — vertex/fragment/compute
├── tests/                # Catch2 birim testleri (118 test)
└── third_party/          # nlohmann/json
```

---

## Lisans

© 2026 İbrahim Kemal Koyuncu — Tüm hakları saklıdır.
