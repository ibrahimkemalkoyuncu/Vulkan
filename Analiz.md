# Codebase Analizi — VKT Mekanik Tesisat Draw

**Tarih:** 2026-04-29  
**Kapsam:** Tüm `src/` ve `vkt/` dizinleri, yaklaşık 15.500 satır C++17  
**Amaç:** 4M FINE SANI paritesi için gerçek durum tespiti

---

## 1. Modül Bazlı Durum

### ✅ TAM İMPLEMENTE (Production-Ready)

| Modül | Dosya(lar) | Notlar |
|-------|-----------|--------|
| **DXFReader** | `src/cad/DXFReader.cpp` | ASCII DXF tam parse: HEADER, TABLES, ENTITIES, BLOCKS, INSERT expansion |
| **DWGReader** | `src/cad/DWGReader.cpp` | LibreDWG entegrasyonu, recursive block expansion (derinlik 8), MINSERT grid, Spline/Ellipse tessellation |
| **NetworkGraph** | `src/mep/NetworkGraph.cpp` | O(1) node/edge CRUD, adjacency list, lazy cache rebuild |
| **HydraulicSolver::Solve()** | `src/mep/HydraulicSolver.cpp` | TS EN 806-3 LU→debi, BFS topolojik dağılım, Darcy-Weisbach, Haaland, Zeta |
| **HydraulicSolver::SolveDrainage()** | `src/mep/HydraulicSolver.cpp` | EN 12056-2 DU dağılımı, K faktörü, Manning boru boyutlandırma, %50 doluluk sınırı |
| **HydraulicSolver::CalculateCriticalPath()** | `src/mep/HydraulicSolver.cpp` | DFS visited set ile kritik devre, pompa yüksekliği (+15m emniyet) |
| **Database** | `src/mep/Database.cpp` | 8 armatür tipi (LU/DU/basınç), 5 boru malzemesi, 11 fitting zeta değeri |
| **ScheduleGenerator** | `src/mep/ScheduleGenerator.cpp` | Markdown rapor, malzeme listesi, CSV export — TAMAMEN İMPLEMENTE |
| **SelectionManager** | `src/cad/SelectionManager.cpp` | Pick, rectangle, polygon, point; named selection sets; type/layer filtresi |
| **SnapManager** | `src/cad/SnapManager.cpp` | Endpoint, midpoint, center, nearest, grid snap; view-scale aware aperture |
| **Document (core)** | `src/core/Document.cpp` | Command pattern, undo/redo (index bazlı), JSON save/load, GetCADExtents() percentile filtresi |
| **Persistence** | `src/core/Persistence.cpp` | NetworkGraph tam JSON serialize/deserialize (node + edge tüm alanlar) |
| **Birim Testler** | `tests/test_hydraulic.cpp` + diğerleri | Catch2, gerçek doğrulama (LU→debi, DU, topolojik dağılım, Vec3, Mat4) |

---

### ⚠️ KISMI İMPLEMENTASYON

| Modül | Durum | Eksik |
|-------|-------|-------|
| **SpaceManager** | CRUD + algılama gerçek | `Deserialize()` stub (TODO yorum), `DetectNameFromText()` boş döner |
| **MainWindow** | Yapı + menü gerçek | L441: delete handler stub, L805: selection handler stub |
| **VulkanRenderer** | SPIR-V shader mevcut | L386: GPU color picking TODO (ray-cast geçici çözüm) |
| **Polyline** | Temel gerçek | Douglas-Peucker, offset, arc→line dönüşümü TODO |
| **Arc** | Temel gerçek | Circle fit algoritması TODO (L274) |
| **DXFReader** | Tüm parse gerçek | L632: Layer::GetName() eksik, workaround var |
| **DWGReader** | Güçlü | L348: full MINSERT nested TODO |

---

### ❌ EKSİK / SIFIRDAN YAPILACAK

| Özellik | FINE SANI'de | VKT'de |
|---------|-------------|--------|
| Kolon şeması (riser diagram) | Otomatik üretim | **YOK** |
| Çok katlı bina yönetimi | Kat bazlı | **YOK** |
| DXF/DWG export | Var | **YOK** (sadece import) |
| Excel (.xlsx) export | Var | **YOK** (sadece CSV) |
| PDF rapor export | Var | **YOK** |
| Armatür sembolleri (plan) | Zengin kütüphane | **YOK** |
| Baskı düzeni (A3/A4 paper space) | Var | **YOK** |
| Ölçü/açıklama araçları | Var | **YOK** |
| Otomatik supply boru çap boyutlandırma | Var | **YOK** (drainage'de var) |
| Kapalı döngü hidrolik (Hardy-Cross) | Var | **YOK** |
| Pompa katalog seçimi | Var | Hardcode +15m emniyet payı |
| Snap görsel göstergesi (crosshair) | Var | **YOK** |
| Komut satırı (AutoCAD benzeri) | Var | **YOK** |
| SSAO shader | — | **YOK** |

---

## 2. Gerçek Durum: Ne Sanılıyordu vs Ne Var

Başlangıç taramada bazı modüller stub sanıldı, kod okunduğunda tam implemente çıktı:

| Modül | Saniyet | Gerçek |
|-------|---------|--------|
| `SolveDrainage()` | Stub | **TAMAMEN İMPLEMENTE** — DU, Manning, K faktörü, boru boyutlandırma |
| `CalculateCriticalPath()` | Belirsiz | **TAMAMEN İMPLEMENTE** — DFS, visited set, pompa hesabı |
| `GenerateMaterialList()` | Stub | **TAMAMEN İMPLEMENTE** — malzeme/armatür toplama |
| `ExportToCSV()` | Stub | **TAMAMEN İMPLEMENTE** |

---

## 3. 4M FINE SANI Karşılaştırması

### FINE SANI'nin Temel Yetenekleri
- 2D/3D BIM modelleme (AutoCAD tabanlı)
- Çizimden otomatik hidrolik hesap (DXF yeterliliği)
- **Kolon şeması** otomatik üretimi — en kritik fark
- **Çok katlı bina** desteği (riser yönetimi)
- Tam malzeme metrajı + **birim fiyatlı maliyet**
- **PDF + Excel** rapor çıktısı
- **DWG formatında kayıt** (AutoCAD uyumlu)
- Armatür sembol kütüphanesi (plan görünümü)
- Baskı düzeni (A3/A4 paper space, başlık bloğu)

### VKT'nin FINE SANI'ye Göre Artıları
- Vulkan GPU render (FINE SANI: eski GDI/OpenGL)
- Recursive block expansion ile gelişmiş DWG okuma
- Sınırsız undo/redo (Command Pattern)
- Açık, modüler C++17 mimarisi
- Kapsamlı snap sistemi (6 tip)
- TS EN 806-3 + EN 12056-2 kodlu implementasyon

### Geride Kaldığımız Kritik Alanlar
1. **Kolon şeması yok** — FINE SANI'nin en görünen özelliği
2. **Çok kat yok** — tek katlı projelerle sınırlı
3. **DXF export yok** — okuma var, yazma yok
4. **PDF/Excel yok** — sadece Markdown + CSV var
5. **Armatür sembolleri yok** — sadece nokta/ikon render

---

## 4. Öncelik Sıralaması (Sahaya Çıkış Engeli)

### P0 — Ürün sahaya çıkamaz bunlar olmadan
1. Kolon şeması üretici
2. PDF + Excel export
3. DXF export

### P1 — Rakip parite için gerekli
4. Çok katlı bina / FloorManager
5. Otomatik supply boru çap boyutlandırma
6. Armatür sembol kütüphanesi (plan görünümü)
7. SpaceManager::Deserialize() (kayıt/yükleme bütünlüğü)
8. Baskı düzeni

### P2 — Kullanıcı deneyimi
9. Snap görsel göstergesi
10. Komut satırı
11. Armatür özellikleri paneli
12. Ölçü/açıklama araçları

### P3 — Teknik kalite
13. Pompa katalog seçimi
14. Kapalı döngü hidrolik (Hardy-Cross)
15. Drenaj testleri
16. SSAO shader

---

## 5. Dosya Referans Haritası (Kritik Yollar)

```
Kolon şeması:
  YENİ: vkt/mep/RiserDiagram.hpp + src/mep/RiserDiagram.cpp
  BAĞIMLI: NetworkGraph, FloorManager (yeni), VulkanRenderer

Çok katlı bina:
  YENİ: vkt/core/FloorManager.hpp + src/core/FloorManager.cpp
  DEĞİŞECEK: Document.hpp (floor list ekle), NetworkGraph (floor field)

DXF Export:
  YENİ: vkt/cad/DXFWriter.hpp + src/cad/DXFWriter.cpp
  BAĞIMLI: Entity, NetworkGraph, Layer

PDF Export:
  DEĞİŞECEK: src/mep/ScheduleGenerator.cpp (QPrinter entegrasyonu)
  BAĞIMLI: Qt6 PrintSupport modülü

Excel Export:
  YENİ: src/mep/ExcelExporter.cpp (XML tabanlı .xlsx, bağımlılık yok)
  veya: third_party/xlsxwriter

Otomatik çap boyutlandırma:
  DEĞİŞECEK: src/mep/HydraulicSolver.cpp (Solve() içine AutoSizePipes() adımı)
  BAĞIMLI: Database (availableDiameters_mm zaten mevcut)

SpaceManager::Deserialize():
  DEĞİŞECEK: src/cad/SpaceManager.cpp (L360)
  BAĞIMLI: nlohmann/json (zaten mevcut)
```

---

## 6. Bağımlılık Durumu

| Kütüphane | Durum | Nerede |
|-----------|-------|--------|
| Qt6 (Core, Gui, Widgets) | ✅ Aktif | UI, Persistence |
| Qt6::PrintSupport | ❌ CMakeLists'e eklenmemiş | PDF export için gerekli |
| Vulkan SDK | ✅ Aktif | Renderer |
| nlohmann/json | ✅ Aktif | third_party/ |
| LibreDWG | ✅ Opsiyonel (vcpkg) | DWGReader |
| Catch2 | ✅ Test build'de aktif | tests/ |
| xlsxwriter / libxlsxwriter | ❌ Yok | Excel export için eklenecek |

---

## 7. Yaklaşık Tamamlanma

| Katman | Tamamlanma |
|--------|-----------|
| Hidrolik motor (supply + drainage) | %90 |
| CAD okuma (DXF + DWG) | %85 |
| Render (Vulkan) | %70 |
| Raporlama (Markdown + CSV) | %80 |
| Raporlama (PDF + Excel) | %0 |
| Kolon şeması | %0 |
| Çok katlı bina | %0 |
| UI paneller | %50 |
| DXF/DWG export | %0 |
| **Genel** | **~55%** |

---

*Bu belge 2026-04-29 tarihinde tüm kaynak kod okunarak üretilmiştir. Güncelleme gerektiğinde ilgili modül bölümünü düzenle.*
