# VKT Kullanıcı Kitabı

**VKT — Mekanik Tesisat Draw v1.0**
Vulkan + Qt6 tabanlı profesyonel MEP (Su Tesisatı & Drenaj) CAD uygulaması.
Standartlar: TS EN 806-3 (su temini) · EN 12056-2 (drenaj) · TS 822 · EN 12566-1

---

## İçindekiler

1. [Başlarken — Proje Oluşturma](#1-başlarken)
2. [Mimari Altlık Yükleme](#2-mimari-altlık-yükleme)
3. [ST Cihazları — Sıhhi Tesisat](#3-st-cihazları)
4. [Temiz Su Borusu Çizimi](#4-temiz-su-borusu-çizimi)
5. [Pis Su Sistemi](#5-pis-su-sistemi)
6. [Tesisat Kopyalama (Çok Katlı)](#6-tesisat-kopyalama)
7. [Hidrolik Analiz](#7-hidrolik-analiz)
8. [Görsel Doğrulama Araçları](#8-görsel-doğrulama)
9. [Snap ve Ölçüm](#9-snap-ve-ölçüm)
10. [Proje Yönetimi (CC Klasörü)](#10-proje-yönetimi)
11. [Komut Referansı](#11-komut-referansı)
12. [Dosya Formatları](#12-dosya-formatları)
13. [Hesap Föyü ve Raporlama](#13-hesap-föyü)
14. [Fosseptik ve Atıksu Standartları](#14-fosseptik-standartları)

---

## 1. Başlarken

### Kök Dizin Ayarı (ilk kurulumda bir kez)

**Dosya → Proje Kök Klasörü Ayarla...**

Tüm projelerin tutulacağı ana dizini seçin. Ayar kalıcı olarak saklanır.

### Yeni Proje Oluşturma

**Dosya → Yeni Proje...** `Ctrl+Shift+N`

Proje adını girin — VKT otomatik olarak şu yapıyı oluşturur:

```
[ProjeKökü]/
└── [ProjeAdı]/
    ├── [ProjeAdı].vkt    ← ana proje dosyası
    ├── mimari/           ← DXF/DWG mimari altlıklar
    ├── cikti/            ← dışa aktarılan dosyalar
    └── rapor/            ← hesap raporları
```

### Mevcut Proje Açma

**Dosya → Aç** `Ctrl+O` — `.vkt` dosyasını seçin; proje klasörü otomatik aktif olur.

---

## 2. Mimari Altlık Yükleme

### Adım 1 — AutoCAD'de W-Block Hazırlama

Her kat, **aynı referans noktası** kullanılarak ayrı DXF dosyasına aktarılmalıdır.

```
Command: WBLOCK
Dosya adı: zemin_kat.dxf  (format: 2013 DXF)
Nesneleri Seç: Tümü (A + Enter)
Baz Nokta: Ortak referans noktasını tıkla (örn. A-1 kolon köşesi)
```

> **Kritik:** Tüm katlarda **aynı fiziksel nokta** (kolon köşesi, asansör perdesi) baz noktası olarak seçilmelidir. VKT bu noktayı tüm katlarda (0,0)'a eşleyerek 3D hizalama sağlar.

### Adım 2 — Mimari Belirle

**Mimari → Mimari Belirle...** `Ctrl+M` — veya CommandBar'a `MIMARI`

| Alan | Açıklama |
|------|----------|
| Global Referans X/Y | W-Block baz noktasının CAD koordinatı (tüm katlarda aynı) |
| Kat Numarası | Sıralı tanımlayıcı — 1'den başlar |
| Kot (m) | Gerçek döşeme kotu (bodrum için negatif) |
| İsim | "Bodrum Kat", "Zemin Kat", "1. Kat" |
| Dosya | İlgili katın DXF/DWG dosyası |

**Kat No ve Kot bağımsızdır.** Örnek:

| Kat No | Kot (m) | İsim |
|--------|---------|------|
| 1 | −3.00 | Bodrum Kat |
| 2 | 0.00 | Zemin Kat |
| 3 | +3.00 | 1. Kat |

**İş akışı:** Kat bilgilerini doldurun → **Yenile** → Tüm katları ekledikten sonra **Tamam**.

### Birim Otomatik Dönüşümü

VKT, DXF dosyasındaki `$INSUNITS` başlık değişkenini okur ve tüm koordinatları otomatik olarak mm'ye dönüştürür:

| $INSUNITS | Kaynak Birim | Dönüşüm |
|-----------|-------------|---------|
| 4 | mm | — (değişiklik yok) |
| 5 | cm | × 10 |
| 6 | m | × 1000 |
| 1 | inch | × 25.4 |

Import sonrası status çubuğunda uygulanan birim gösterilir.

---

## 3. ST Cihazları — Sıhhi Tesisat

### Panel Kullanımı

Sağ panel → **ST Cihazları** sekmesi

1. Listeden cihazı seçin → **çift tıklayın**
2. Status bar: `ST Cihaz [Lavabo]: Yerleştirme noktasını tıklayın`
3. Çizim alanında konuma tıklayın → cihaz yerleştirilir
4. AutoHydro tetiklenir; DN etiketleri güncellenir

### TS EN 806-3 Tablo 3 — Yük Birimleri

| Cihaz | LU | Min. Boru |
|-------|----|-----------|
| Lavabo | 0.5 | DN 15 |
| Duş | 1.0 | DN 15 |
| WC (rezervuar) | 2.0 | DN 15 |
| Küvet | 3.0 | DN 20 |
| Evye (mutfak) | 1.5 | DN 15 |
| Bulaşık Makinesi | 1.5 | DN 15 |
| Çamaşır Makinesi | 2.0 | DN 15 |
| Pisuar | 0.5 | DN 15 |
| Bide | 0.5 | DN 15 |

### CommandBar'dan Armatür Ekleme

```
FIXTURE   → diyalogdan armatür tipi seçilerek yerleştirme
```

---

## 4. Temiz Su Borusu Çizimi

### Araçlar

| Eylem | Menü / Toolbar | CommandBar |
|-------|---------------|------------|
| Boru çiz | Çizim → Boru Çiz | `PIPE` veya `LINE` |
| Bağlantı noktası | Çizim → Bağlantı Noktası | `JUNCTION` |
| Kaynak (şebeke girişi) | — | `SOURCE` |
| Seçim modu | Çizim → Seç | `ESC` |

### Çizim İş Akışı

1. `PIPE` komutu veya toolbar butonu
2. **1. nokta** tıkla → boru başlangıcı
3. **2. nokta** tıkla → boru eklenir, zincirleme devam eder
4. `ESC` ile çizimi bitirin

> Snap sistemi **Endpoint > Center > Intersection > Midpoint > Perpendicular > Nearest > Grid** öncelik sırasıyla çalışır. Kolondan yatay hat alırken **Perpendicular snap** otomatik önerilir.

### Rubber-band Önizleme

Çizerken imleç hareketi ile canlı önizleme çizgisi görünür. Boru henüz eklenmemiştir — ikinci tıklamada eklenir.

---

## 5. Pis Su Sistemi

### Pis Su Borusu Çizimi

**Çizim → Pis Su Borusu** — veya CommandBar: `PIS-SU`

- Borular **kahverengi** renkte gösterilir (temiz su: mavi)
- Eğim parametresi `Edge::slope` alanında saklanır (varsayılan %2)
- EN 12056-2 Manning denklemi ile akış kapasitesi hesaplanır

### Yer Süzgeci Yerleştirme

**Çizim → Yer Süzgeci** — veya `YER-SUZGECI`

Zemin döşemesinde su tahliyesi için yer süzgeci düğümü yerleştirilir. Pis su borusuyla bağlanır.

### Boşaltma Noktası (Rögar)

**Çizim → Rögar (Boşaltma)** — veya `ROGAR`

Pis su sisteminin binayı terk ettiği noktayı tanımlar. Her pis su sistemi en az bir boşaltma noktasına sahip olmalıdır.

### Pis Su İş Akışı (EN 12056-2)

```
Yer Süzgeci / WC / Duş
      ↓  (pis su borusu, eğimli)
Yatay Toplama Hattı
      ↓  (kolektör)
Düşey Kolon
      ↓
Rögar (Boşaltma Noktası)
```

> **Minimum eğim:** DN 50 → %2, DN 70 → %1.5, DN 100+ → %1 (EN 12056-2)

---

## 6. Tesisat Kopyalama

Çok katlı binalarda aynı plan tekrar eden katlarda tesisat tasarımını sıfırdan çizmek yerine kopyalayın.

### Kullanım

**Çizim → Tesisat Kopyala...** — veya `KOPYA-KAT`

1. **Kaynak kat** seçin (kopyalanacak)
2. **Hedef kat** seçin (kopyalanacağı yer)
3. VKT:
   - Kaynak kattaki tüm **yatay boru ve armatürleri** kopyalar
   - **Kolonları otomatik hariç tutar** (dikey bileşenler — her katta ayrı tanımlanmalı)
   - Kopyalama tam Undo/Redo desteklidir

> **Önemli:** Kolonlar (dikey, Z-ekseninde uzanan borular) kopyalamaya dahil edilmez. Pis suda özellikle dikkat — teras ve açık alan engellerinden geçen kolonlar manuel olarak her katta ayrı çizilmelidir.

### Ön Koşul

Kopyalama için en az 2 katın **Mimari → Mimari Belirle** ile tanımlı olması gerekir.

---

## 7. Hidrolik Analiz

### Gerçek Zamanlı Analiz (AutoHydro)

Her boru veya armatür ekleme/silme işleminden 600ms sonra otomatik olarak çalışır:

1. TS EN 806-3 LU tablosu → debi (`Q = K × √ΣLU`)
2. Darcy-Weisbach → sürtünme kaybı
3. Sonuç: her boru üzerinde **DN etiketi** (açık mavi) görünür

**K katsayısı:** Konut = 0.5 · Otel = 0.7 · Endüstri = 1.0

### Manuel Tetikleme

**Analiz → Hidrolik Analiz** — veya CommandBar: `HYDRAULICS`

### Boru Boyları ve Debiler

VKT boru boyunu çizimdeki iki uç nokta arasındaki Öklid mesafesinden otomatik hesaplar — manuel giriş hatası riski yoktur.

| Büyüklük | Formül | Standart |
|----------|--------|---------|
| Toplam LU | Σ(armatür LU) | EN 806-3 T.3 |
| Debi Q | `K × √(ΣLU)` L/s | EN 806-3 §9 |
| DN seçimi | v ≤ 2 m/s, ΔP ≤ 300 Pa/m | — |

### Kritik Devre

HydraulicSolver en dezavantajlı hattı (en uzun yol + en yüksek basınç kaybı) otomatik tespit eder. Pompa/hidrofor boyutlandırması bu hat üzerinden yapılır.

---

## 8. Görsel Doğrulama

### Açık Uç Uyarıları (Real-time)

Bağlantısız node'lar **kırmızı halka + ünlem işareti** ile işaretlenir:

- **Kırmızı halka**: node hiçbir boruya bağlı değil (isolated)
- **Kırmızı halka**: Junction node tek kenara bağlı (dead-end boru ucu)

Bu işaretler otomatik olarak kaybolur — boruyu bağladığınızda uyarı kaldırılır.

### MEP Overlay Etiketleri

| Etiket Rengi | Anlamı |
|-------------|--------|
| Açık mavi | DN boru çapı (borunun ortasında) |
| Yeşil | Armatür tipi (Lavabo, WC...) |
| Turuncu | Drain node (Yer Süzgeci, Rögar) |
| Sarı | Pompa |

### 3D Görünüm

**Görünüm → İzometrik** `Ctrl+5` — katlar Z ekseninde kotlarına göre konumlandırılmış olarak görünür. Kolon borularının dikey olarak bağlandığı doğrulanabilir.

### Clash Detection

**Analiz → Çakışma Tespiti** — GPU compute shader ile tüm boru kesişimleri taranır.

---

## 9. Snap ve Ölçüm

### Snap Öncelik Sırası

1. **Endpoint** — çizgi/boru ucu (sarı kare)
2. **Center** — daire/yay merkezi (cyan)
3. **Intersection** — iki çizginin kesişimi (magenta X)
4. **Midpoint** — orta nokta (yeşil üçgen)
5. **Perpendicular** — dik açılı nokta (mavi)
6. **Nearest** — en yakın nokta (açık mavi elmas)
7. **Grid** — ızgara (beyaz)

### Mesafe Ölçme

CommandBar: `UZAKLIK` veya `DISTANCE`

1. 1. noktayı tıklayın
2. 2. noktayı tıklayın
3. Sonuç: `Mesafe: XXX.XX mm (X.XXX m)` — status bar ve log panelinde

ESC ile iptal.

---

## 10. Proje Yönetimi (CC Klasörü)

### Proje Klasörü Yapısı

```
[ProjeKökü]/[ProjeAdı]/
├── [ProjeAdı].vkt    ← Dosya → Kaydet ile güncellenir
├── mimari/           ← Mimari Belirle'de "kopyala" seçilirse buraya
├── cikti/            ← DXF dışa aktarım için önerilen konum
└── rapor/            ← Rapor Dışa Aktar otomatik buraya yönlendirir
```

### Menü Komutları

| Komut | Kısayol |
|-------|---------|
| Yeni Proje | `Ctrl+Shift+N` |
| Aç | `Ctrl+O` |
| Kaydet | `Ctrl+S` |
| Farklı Kaydet | `Ctrl+Shift+S` |
| Proje Kök Klasörü Ayarla | — |
| Proje Klasörünü Aç | — (Dosya Gezgini'nde açar) |

---

## 11. Komut Referansı

### Çizim Komutları

| Komut | Açıklama |
|-------|----------|
| `PIPE` / `LINE` | Temiz su borusu çiz |
| `PIS-SU` | Pis su borusu çiz (kahverengi) |
| `FIXTURE` | Armatür yerleştir (diyalogdan tip seçilir) |
| `JUNCTION` | Bağlantı noktası ekle |
| `SOURCE` | Su kaynağı / şebeke girişi |
| `DRAIN` | Tahliye noktası |
| `YER-SUZGECI` | Yer süzgeci yerleştir |
| `ROGAR` / `BOSALTMA` | Boşaltma noktası (rögar) |
| `KOPYA-KAT` | Tesisat kopyalama diyaloğunu aç |

### Analiz ve Görünüm

| Komut | Açıklama |
|-------|----------|
| `HYDRAULICS` | Tam hidrolik çözüm |
| `UZAKLIK` / `DISTANCE` | Mesafe ölç |
| `ZOOM-EXTENTS` / `ZE` | Tümünü göster |
| `VIEW-PLAN` | Plan (2D) görünüm |
| `VIEW-ISO` | İzometrik 3D görünüm |
| `MIMARI` | Mimari Belirle diyaloğunu aç |
| `UNDO` / `REDO` | Geri al / İleri al |
| `HELP` | Tüm komutları listele |

---

## 12. Dosya Formatları

| Format | İçe Aktar | Dışa Aktar |
|--------|-----------|------------|
| `.vkt` | Dosya → Aç | Dosya → Kaydet |
| `.dxf` | Dosya → CAD Dosyası İçe Aktar | Analiz → Rapor Dışa Aktar |
| `.dwg` | Dosya → CAD Dosyası İçe Aktar | — |
| `.xls` | — | Analiz → Rapor Dışa Aktar |
| `.pdf` | — | Analiz → Rapor Dışa Aktar |

---

## 13. Hesap Föyü

### Otomatik Güncelleme

VKT'de hesap föyü **her çizim değişikliğinde** (600ms debounce) otomatik güncellenir:

- Boru boyu: çizimdeki iki uç nokta arası Öklid mesafesi (manuel giriş yoktur)
- Debi: LU → `Q = K × √(ΣLU)` (EN 806-3)
- Basınç kaybı: Darcy-Weisbach + Haaland sürtünme faktörü
- DN seçimi: v ≤ 2 m/s ve ΔP ≤ 300 Pa/m kısıtları altında en küçük standart çap

### Rapor Dışa Aktarma

**Analiz → Rapor Dışa Aktar** — aktif proje varsa `rapor/` klasörüne yönlendirilir.

Rapor içeriği:
- Her boru için: DN, boy (m), debi (L/s), hız (m/s), basınç kaybı (Pa/m)
- Kritik devre hattı
- Toplam LU ve hesap debisi

---

## 14. Fosseptik ve Atıksu Standartları

### Uygulanabilir Standartlar

| Konu | Standart |
|------|---------|
| Fosseptik boyutlandırma | **TS 822** |
| Sızdırmaz kapalı çukur | **TS EN 12566-1** |
| Emici kuyu (emdirme çukuru) | **TS EN 12566-2** |
| Pis su boru hesabı | **EN 12056-2** |
| Yağmur suyu hesabı | **EN 12056-3** |
| Arıtma tesisi deşarjı | **TS EN 12255** |

### Fosseptik Hacim Formülü

```
V (m³) = n_kişi × v_kişi × T_boşaltma
```

| Parametre | Tipik Değer |
|-----------|------------|
| `n_kişi` | Eşdeğer kullanıcı (PE) |
| `v_kişi` | 150–200 L/kişi/gün |
| `T_boşaltma` | Min. 90 gün (EN 12566-1) |

**Örnek:** 10 kişi × 0.15 m³/gün × 90 gün = **135 m³ kapasiteli fosseptik**

### Kapalı Çukur (Sızdırmaz Depo)

Aynı formül uygulanır; boşaltma periyodu yerel yönetim yönetmeliğine göre belirlenir. EN 12566-1 sızdırmazlık testini zorunlu kılar.

### Emdirme Çukuru

Toprak geçirgenliği (`k` katsayısı) sahada ölçülür. EN 12566-2 ile çukur boyutu:

```
A_emici (m²) = Q_atık (m³/gün) / k (m/gün)
```

---

## Hızlı Başlangıç — Örnek Proje

```
1. Dosya → Yeni Proje... → "OrnekBina" girin
2. Mimari → Mimari Belirle... → 2 kat tanımlayın (0.00 m, +3.00 m)
   Global Ref X/Y: AutoCAD'deki ortak köşe koordinatı
3. Her kat için DXF seçin → Tamam → DXF import
4. ST Cihazları panelinden Lavabo ve WC'yi çizime yerleştirin
5. PIPE komutu ile boruları çizin (snap'e dikkat)
6. SOURCE komutu ile şebeke giriş noktası ekleyin
7. HYDRAULICS → DN etiketleri boruların üzerinde görünür
8. PIS-SU komutu → pis su borularını çizin
9. YER-SUZGECI → yer süzgeçlerini ekleyin
10. ROGAR → boşaltma noktasını tanımlayın
11. 2. kat için KOPYA-KAT → kaynak: Zemin, hedef: 1. Kat
12. Analiz → Rapor Dışa Aktar → rapor/ klasörüne kaydedin
```

---

*VKT v1.0 — © 2026 — TS EN 806-3 · EN 12056-2 · TS 822 · EN 12566-1 uyumlu*
