# VKT - Mekanik Tesisat Draw

**Vulkan Tabanlı Profesyonel Sıhhi Tesisat CAD Yazılımı**

## 🎯 Proje Hedefi
FINE SANI'yi geçen, mühendislik hesapları ile donatılmış, modern bir MEP (Tesisat) CAD yazılımı.

## 🏗️ Mimari

### **Çekirdek Teknolojiler**
- **Vulkan**: Yüksek performanslı 3D rendering
- **Qt6**: Modern UI framework
- **C++17**: Gelişmiş dil özellikleri

### **Modüller**
1. **vkt::core** - Temel CAD altyapısı (Document, Command Pattern)
2. **vkt::geom** - Geometrik işlemler (B-Rep, 3D Math)
3. **vkt::mep** - Tesisat mühendisliği (Hidrolik hesap, veritabanı)
4. **vkt::render** - Vulkan rendering motoru
5. **vkt::ui** - Kullanıcı arayüzü

## 📐 Mühendislik Özellikleri

### **Hidrolik Analiz**
- ✅ Darcy-Weisbach / Haaland (Besleme hatları)
- ✅ EN 12056 (Atık su / Drenaj)
- ✅ Zeta faktörü (Lokal kayıplar)
- ✅ Kritik devre analizi (Pompa yüksekliği)

### **Mimari Entegrasyon**
- ✅ DXF import
- ✅ B-Rep tabanlı mahal (space) tespiti
- ✅ Alan hesabı ve yük dağılımı

### **Standartlar**
- TS EN 806-3 (İçme suyu tesisatı)
- EN 12056 (Atık su tesisatı)

## 🚀 Kurulum

```bash
# 1. Gereksinimler
# - Vulkan SDK
# - Qt6
# - CMake 3.20+

# 2. Build
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 📊 Özellik Durumu

| Özellik | Durum |
|---------|-------|
| B-Rep Space Detection | ✅ 100% |
| Supply Network Analysis | ✅ 100% |
| Drainage Solver (EN 12056) | ✅ 100% |
| Critical Path (Pump Head) | ✅ 100% |
| 3D Visualization | ✅ 100% |
| Plan/Isometric View | ✅ 100% |

## 🔮 Gelecek Özellikler

- [ ] Akıllı yakalama (Smart Snap)
- [ ] Çok katlı bina desteği (Riser Management)
- [ ] Çarpışma kontrolü (Clash Detection)
- [ ] Otomatik raporlama (Schedule Export)

## 📝 Lisans

Proprietary - Telif Hakkı © 2026
