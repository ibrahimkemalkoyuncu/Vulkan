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
15. [Cihazları Tesisata Bağla (BAGLA)](#bagla)
16. [Hesap Normu Seçimi](#norm)
17. [Hidrofor Boyutlandırma](#hidrofor)
18. [Yağmur Suyu Modülü](#yagmur)
19. [Keşif Listesi (BOM)](#bom)
20. [3D Hizalama Kontrolü](#hizalama)
21. [Kolon Şeması (Riser Diyagramı)](#riser)
22. [DN Manuel Override — Hesap Föyü XLS](#dn-override)
23. [Kolon Bağlantı Asistanı (KOLON)](#kolon-asistan)
24. [PDF Pafta Düzeni (PAFTA)](#pafta)
25. [Sıcak Su Modülü](#sicak-su)
26. [Tesisatı Kabul Et](#tesisat-kabul)
27. [Çıktı Dosyasının Hazırlanması](#cikti)
28. [Pis Su Hesap Araçları](#pis-su-hesap)
29. [Özel Hesap Araçları](#ozel-hesap)
30. [Fare Kullanımı — AutoCAD Uyumlu](#fare-kullanimi)
31. [Seçim Araçları — Pencere / Kesişim](#secim-araclari)
32. [Snap Sistemi — Gelişmiş](#snap-sistemi)
33. [Katman Yöneticisi](#katman-yoneticisi)
34. [Firma Logosu & Pafta Export](#logo-pafta)
35. [Düzenleme Komutları — TRIM / OFFSET / MIRROR](#duzen-komutlari)
36. [Ortho Modu & Copy/Paste](#ortho-copy)

---

## 1. Başlarken

### Kök Dizin Ayarı (ilk kurulumda bir kez)

**Dosya → Proje Kök Klasörü Ayarla...**

Tüm projelerin tutulacağı ana dizini seçin. Ayar kalıcı olarak saklanır.

### Yeni Proje Oluşturma

**Dosya → Yeni Proje...** `Ctrl+Shift+N`

Proje oluşturma penceresi şu bilgileri toplar:

| Alan | Açıklama |
|------|----------|
| **Proje Adı** \* | Klasör adı (özel karakter yasak) |
| Müşteri | İşveren / müşteri adı |
| Mühendis | Proje sorumlusu |
| Bina Tipi | Konut, Ofis, Otel, Hastane... |
| **Hesap Normu** | EN 806-3 (Türkiye/AB) veya DIN 1988-300 |
| Proje Tarihi | Takvimden seçilir |

Proje oluşturulunca VKT:
- Seçilen norm otomatik uygulanır (sonradan Analiz → Hesap Normu ile değiştirilebilir)
- Müşteri ve mühendis bilgisi log paneline ve status bar'a yazılır
- Şu dizin yapısı oluşturulur:

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

### Boru Malzeme Seçimi

Çizmeden önce sağ paneldeki **"Özellikler (Mühendislik Modu)"** sekmesinde **Malzeme** açılır menüsünden seçim yapın:

| Malzeme | Tipik Kullanım |
|---------|---------------|
| PVC (varsayılan) | Soğuk su, drenaj |
| PP | Sıcak/soğuk su (PPR boru) |
| PE | Toprak altı |
| Bakır | Sıcak su, gömme tesisat |
| Çelik | Endüstriyel, yangın |

Seçilen malzemenin pürüzlülüğü Darcy-Weisbach hesabına otomatik girer.

### Çizim İş Akışı

1. Özellik panelinden malzeme seçin
2. `PIPE` komutu veya toolbar butonu
3. **1. nokta** tıkla → boru başlangıcı
4. **2. nokta** tıkla → boru eklenir, zincirleme devam eder
5. `ESC` ile çizimi bitirin

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
| `HIDROFOR` | Kritik devre + pompa boyutlandırma |
| `NORM` | Hesap normu seç (EN 806-3 / DIN 1988-300) |
| `YAGMUR` | Yağmur suyu tahliye boyutlandırması |
| `BOM` / `KESIF` | Keşif listesi (metraj + bağlantı sayımı) |
| `RISER` / `KOLON-SEMA` | Kolon şeması (SVG/PDF) |
| `DN-OVERRIDE` / `DN-DEGISTIR` | DN manuel override + XLS föy |
| `HIZALAMA` / `FLOOR-ALIGN` | 3D hizalama kontrolü |
| `KOLON` / `COLUMN` | Kolon bağlantı asistanı (dikey boru) |
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

## Bölüm 15 — Cihazları Tesisata Bağla (BAGLA)

FineSANI'nin en sık kullanılan "Cihazları Tesisata Bağla" özelliği VKT'de `BAGLA` komutu olarak implementedir.

### Çalışma Prensibi

Armatür (lavabo, WC, vb.) çizime yerleştirildikten sonra ana boru hattına dik bir dal boru ile bağlanır.

### Adım Adım Kullanım

```
1. BAGLA yazın (veya Çizim → Cihazı Tesisata Bağla ya da Ctrl+B)
2. Bağlanacak armatürü tıklayın   [1/2 — Armatür seç]
   → Status bar: "[Lavabo] seçildi — boru hattını tıklayın (2/2)"
3. Bağlanacak ana boru hattını tıklayın  [2/2]
   → VKT otomatik olarak:
      a) Borunun en yakın noktasına (dik ayak) Junction ekler
      b) Armatür → Junction arası dal boru çizer
      c) Dal borunun çapı ve malzemesi ana borudan kopyalanır
4. Komut devam eder — bir sonraki armatürü tıklayabilirsiniz
5. ESC ile çıkın
```

### Notlar

- Dal borunun uzunluğu otomatik hesaplanır (mm → m dönüşümü)
- Undo/Redo destekler (CompositeCommand)
- Boru bağlandıktan sonra Gerçek Zamanlı Hidrolik (AutoHydro) devreye girer ve DN etiketleri güncellenir

---

## Bölüm 16 — Hesap Normu Seçimi

VKT iki farklı hesap normu destekler:

| Norm | Formül | Kullanım |
|------|--------|---------|
| **TS EN 806-3** (varsayılan) | Q = 0.682 × LU^0.45 l/s | Türkiye / AB projeleri |
| **DIN 1988-300** | Q = φ × √ΣLU × 0.5 l/s | Alman norm gereksinimi |

DIN 1988-300'de eşzamanlılık faktörü: φ = 1 / (1 + √LU / 10)

DIN normu genellikle daha küçük boru çapları verir — büyük binalarda belirgin fark.

### Norm Seçimi

```
Analiz → Hesap Normu...
veya: NORM komutu
```

Seçim sonrası AutoHydro otomatik tetiklenir, DN etiketleri güncellenir.

---

## Bölüm 17 — Hidrofor Boyutlandırma

### Kullanım

```
Analiz → Hidrofor Boyutlandırma...
veya: HIDROFOR komutu
```

VKT kritik devre analizini çalıştırır ve şu bilgileri gösterir:

| Bilgi | Açıklama |
|-------|---------|
| **Kritik devre kaybı** | Kırmızı değer — minimum gereken pompa yüksekliği |
| **Gerekli pompa basma yüksekliği** | mSS (metre su sütunu) |
| **Gerekli debi** | m³/h |
| **Önerilen ekipman** | Model adı + max basınç + max debi + güç |

> Bu değer FineSANI'deki "kırmızı değer" ile eşdeğerdir.

---

## Bölüm 18 — Yağmur Suyu Modülü

EN 12056-3 standardına göre yağmur suyu tahliye borusu boyutlandırması.

### Kullanım

```
Analiz → Yağmur Suyu Modülü...
veya: YAGMUR komutu
```

### Girdi Parametreleri

| Parametre | Açıklama |
|-----------|---------|
| **Tahliye alanı (m²)** | Çatı / zemin alanı |
| **Yüzey tipi** | Çatı (C=1.0), Beton/Asfalt (C=0.9), Yeşil çatı (C=0.5), Çakıllı (C=0.6) |

### Hesap Yöntemi

```
Q = C × A × r_D
r_D = 0.03 l/(s·m²)  (Türkiye iklim bölgesi, 2 yıllık dönüş periyodu)
```

Çıktı: Önerilen boru sayısı ve DN boyutu (Manning denklemi, %2 eğim, PVC n=0.012)

---

## Bölüm 19 — Keşif Listesi (BOM)

### Kullanım

```
Analiz → Keşif Listesi (BOM)...
veya: BOM veya KESIF komutu  (Ctrl+K)
```

### Çıktı İçeriği

**Boru Metrajları:**
- DN'e ve malzemeye göre gruplanmış toplam boru boyu (m)
- Her DN'deki parça sayısı
- Genel toplam

**Bağlantı Elemanları (tahmini):**
- T-parça: üç veya daha fazla boru bağlantısı olan kavşak noktaları
- Dirsek: iki borulu kavşak noktaları
- Armatür bağlantısı: Fixture node sayısı
- Kaynak / Tahliye noktası sayıları

> Tüm sonuçlar Analiz Logu'na da yazılır.

---

## Bölüm 20 — 3D Hizalama Kontrolü {#hizalama}

Mimari altlıklar import edildikten sonra her katın Z kotunun doğru olduğunu ve tesisat node'larının kat kotlarına atandığını kontrol edin.

### Kullanım

```
Mimari → 3D Hizalama Kontrolü...
veya: Ctrl+Shift+H
veya: HIZALAMA komutu
```

### Kontrol Tablosu

| Sütun | Açıklama |
|-------|---------|
| **Kat No** | MimariBelirleDialog'daki kat numarası |
| **İsim** | Kat ismi (Bodrum, Zemin, 1. Kat...) |
| **Kot (m)** | Tanımlanan döşeme kotu |
| **Kat Yük. (m)** | Kat yüksekliği (düzenlenebilir — çift tıkla) |
| **Node Sayısı** | O kata atanan boru/armatür sayısı |
| **Durum** | OK / HATA: Kot çakışıyor / Boş (node yok) |

### Renk Kodları

| Renk | Anlam |
|------|-------|
| Kırmızı | İki kat aynı Z kotunda — **düzeltilmeli** |
| Sarı | Kat tanımlı ama hiç node yok — import edilmemiş olabilir |
| Normal | Her şey yolunda |

### Kat Yüksekliği Düzenleme

"Kat Yük. (m)" sütununu çift tıklayıp yeni değer girin. **Uygula ve Kapat** ile değişiklikler FloorManager'a yazılır.

> Node sayısı 0 olan kat sarı renkte görünür. Bu katın DXF'i henüz import edilmemiş demektir — Mimari → Mimari Belirle ile dosya ekleyin.

---

## Bölüm 21 — Kolon Şeması (Riser Diyagramı) {#riser}

Tesisat sisteminin dikey kesit görünümü — her kolon hat üzerindeki boruları, armatürleri ve DN değerlerini gösterir.

### Kullanım

```
Analiz → Kolon Şeması...
veya: Ctrl+R
veya: RISER veya KOLON-SEMA komutu
```

### Çıktı Seçenekleri

| Düğme | İşlev |
|-------|-------|
| **SVG Kaydet** | Vektörel SVG dosyası (rapor/ klasörüne) |
| **PDF Kaydet** | A3 Landscape PDF (QPrinter) |

Diyagram önizlemesi pencerede görüntülenir (QTextBrowser). SVG/PDF kaydedilmeden önce önizleme kontrol edilebilir.

---

## Bölüm 22 — DN Manuel Override ve Hesap Föyü XLS {#dn-override}

Otomatik boyutlandırmanın dışına çıkmak istediğiniz borulara manuel DN atayabilirsiniz.

### Kullanım

```
Analiz → Hesap Föyü — DN Override...
veya: DN-OVERRIDE veya DN-DEGISTIR komutu
```

### Override Tablosu

Her boru satırında:
- **ID** — boru kimliği
- **Tip** — Supply / Drainage
- **Malzeme** — çelik, bakır, PPR...
- **DN** — açılır liste (DN16 → DN200)
- **Uzunluk** — otomatik hesaplanan boru boyu
- **Debi / Hız / ΔH** — son hidrolik çözümden

Bir satırda DN değiştirilip **Tamam** basıldığında:
1. Seçilen DN anında uygulanır
2. Overlay etiketleri güncellenir
3. Hidrolik çözüm otomatik yeniden çalışır

### XLS Dışa Aktarım

Tablonun altındaki **XLS Olarak Kaydet** butonu üç sekme içeren Excel dosyası üretir:

| Sekme | İçerik |
|-------|--------|
| **Özet** | Proje bilgileri, toplam boru metrajı |
| **Boru Hesap Föyü** | ID, Tip, Malzeme, DN, L(m), Q(l/s), v(m/s), dH(mSS) |
| **Armatür Listesi** | Tip ve sayı |

---

## Bölüm 23 — Kolon Bağlantı Asistanı (KOLON) {#kolon-asistan}

Kolon, farklı katlardaki tesisatları birbirine bağlayan **dikey boru**dur. Temiz su kolonları ana şebekeden yukarı kata su taşır; pis su kolonları atıksuyu aşağıya iletir.

FineSANI'de kolon çizimi standart boru aracıyla yapılır ve kullanıcı Z kotunu elle girmek zorundadır. VKT'de **Kolon Bağlantı Asistanı** bu adımı otomatikleştirir:

1. Mevcut node'ları (kavşak, armatür, kaynak) bir listeden seçersiniz.
2. Hedef katı açılır menüden seçersiniz.
3. Program **aynı XY konumunda** hedef kat yüksekliğinde yeni bir node oluşturur (veya mevcut olanı bulur) ve dikey boruyu ekler.

### Nasıl Kullanılır

```
Yöntem 1: Çizim → Kolon Boru Ciz (Ctrl+Shift+K)
Yöntem 2: Komut satırına KOLON yazın
```

**Ön koşullar:**
- En az 2 kat tanımlanmış olmalı (Mimari → Mimari Belirle, Ctrl+M).
- En az 1 mevcut node (kavşak/armatür/kaynak) çizilmiş olmalı.

**Adımlar:**

| Adım | Açıklama |
|------|----------|
| 1 | "Kaynak Node" listesinden kolon başlangıcını seçin |
| 2 | "Hedef Kat" listesinden kolonun uzanacağı katı seçin |
| 3 | Tamam → kolon otomatik oluşur, DN hesabı güncellenir |

### Teknik Detaylar

- **Boru tipi:** Aktif pis su/temiz su modu korunur (son seçilen `PIS-SU` veya normal boru)
- **Boru çapı:** Otomatik Ø25mm ile başlar; `HYDRAULICS` veya `DN-OVERRIDE` ile güncelleyin
- **Uzunluk:** Kat elevasyonları arasındaki Z farkından hesaplanır (m)
- **Undo/Redo:** `Ctrl+Z` ile geri alınabilir
- **Mevcut node tespiti:** Hedef katta aynı XY konumunda ±50mm/±0.15m tolerans içinde node varsa yeni node oluşturulmaz, var olan kullanılır

> **İpucu:** Temiz su kolonlarından önce her katın yatay borularını çizin, sonra KOPYA-KAT ile diğer katlara kopyalayın, ardından KOLON ile katları bağlayın. Bu sıralama en az hata riski taşır.

---

## Bölüm 22b — Devre Seçenekleri {#devre-sec}

FineSANI'nin "Devre Seçenekleri" penceresine karşılık gelir — bina tipi, boru cinsi, pürüzlülük ve maksimum hız tek yerden ayarlanır.

```
Analiz → Devre Secenekleri...
Kısayol: Ctrl+Shift+D
Komut: DEVRE  veya  DEVRE-SEC
```

| Parametre | Açıklama | Varsayılan |
|-----------|---------|-----------|
| **Bina Tipi** | Konut / Otel / Endüstri | Konut |
| **Ana Boru Cinsi** | PPR / PVC / Bakır / Galvaniz / PE | PPR |
| **İkincil Boru Cinsi** | Dallar için boru malzemesi | PPR |
| **Boru Pürüzlülüğü** | Darcy-Weisbach için `ε` (mm) | 0.007 mm (PPR) |
| **Maks. Su Hızı** | Bu değeri aşan çaplar otomatik büyütülür | 2.0 m/s |
| **Hesap Normu** | TS EN 806-3 veya DIN 1988-300 | TS EN 806-3 |

**Not:** Ana boru cinsi değiştiğinde pürüzlülük otomatik güncellenir. Tamam'a basıldığında mevcut tüm borulara yeni malzeme/pürüzlülük uygulanır ve sistem yeniden hesaplar.

---

## Bölüm 22c — Baskı İçeriği (Çizim Etiket Seçici) {#baski-icerigi}

Çizim üzerindeki MEP etiketlerinde hangi değerlerin görüneceğini seçin.

```
Analiz → Baski Icerigi...
Komut: BASKI  veya  BASKI-ICERIGI
```

Checkbox'larla seçilebilen bileşenler:

| Seçenek | Örnek görünüm |
|---------|--------------|
| ☑ DN (çap) | `DN32` |
| ☐ Debi Q | `Q=1.40L/s` |
| ☐ Uzunluk L | `L=4.5m` |
| ☐ Hız v | `v=1.75` |
| ☐ Basınç kaybı ΔH | `dH=0.3966` |

Birden fazla seçilirse etiketler birleştirilir: `DN32 Q=1.40L/s L=4.5m`

---

## Bölüm 22d — Parçaların Basınç Kaybı {#basinc-kaybi}

Tüm boru devrelerinin basınç kaybı ayrıntılı tablo olarak gösterilir. Kritik devre sarı ile vurgulanır.

```
Analiz → Parcalarin Basinc Kaybi...
Komut: BASINC  veya  PARCALAR
```

Tablo sütunları: **Devre No / Tip / Malzeme / DN / L (m) / Q (L/s) / v (m/s) / ΔH (m) / Durum**

- Sarı satırlar = kritik devre (en yüksek toplam kayıp)
- "**PDF Kaydet**" butonu ile A4 Yatay PDF olarak çıktı alınabilir

---

## Bölüm 22e — Word RTF Rapor {#word-rapor}

FineSANI'nin "Word Dosyası Oluştur" özelliğine karşılık gelir.

```
Analiz → Word/HTML Rapor Olustur...
Komut: WORD  veya  HTML-RAPOR
```

Oluşturulan rapor içeriği:
1. **Devre Parametreleri** — bina tipi, malzeme, norm, hız limiti
2. **Boru Hesap Föyü** — tüm devreler (DN / L / Q / v / ΔH), kritik devre vurgulu
3. **Kritik Devre ve Hidrofor** — toplam kayıp + gerekli pompa yüksekliği
4. **Armatür Listesi** — tip, LU, debi

Dosya **`.rtf`** (Rich Text Format) olarak kaydedilir — **Microsoft Word** ve WordPad doğrudan açar, harici kütüphane gerekmez. Türkçe karakterler RTF `\uXXXX?` escape ile tam korunur. `rapor/` klasörüne otomatik yol önerisi gelir.

---

## Bölüm 24 — PDF Pafta Düzeni (PAFTA) {#pafta}

**VKT**, tesisat çizimini A3/A4 boyutlu bir sayfa düzenine (pafta) yerleştirip ISO 7200 başlık bloğuyla birlikte **PDF** veya **SVG** olarak dışa aktarır.

```
Analiz → Pafta Duzenle ve Yazdir...  (Ctrl+P)
Komut: PAFTA  veya  PRINT
```

### Pafta Düzeni Penceresi

**Sayfa Ayarları:**

| Alan | Seçenekler |
|------|-----------|
| Kağıt Boyutu | A3 Yatay (420×297 mm), A3 Dikey, A4 Yatay, A4 Dikey |
| Ölçek | Otomatik (sığdır) · 1:20 · 1:50 · 1:100 · 1:200 · 1:500 |

**ISO 7200 Başlık Bloğu:**

| Alan | Açıklama |
|------|----------|
| Proje Adı * | Zorunlu — ProjectManager'dan otomatik doldurulur |
| Pafta Adı | ör. "Zemin Kat Temiz Su Tesisatı" |
| Pafta No | ör. P-001 |
| Revizyon | A, B, C... |
| Firma | Mühendislik bürosu adı |
| Çizen | Baş harfler |
| Tarih | Otomatik — bugünün tarihi |
| **Firma Logosu** | **PNG / JPG / BMP / SVG — opsiyonel** |

**Firma Logosu Yükleme:**

1. "Yükle..." butonuna tıklayın.
2. Logo dosyasını seçin (PNG/JPG/BMP/SVG).
3. 120×40 px önizleme alanda logo görünür.
4. PDF çıktısında başlık bloğunun firma alanında logo render edilir.
5. Logoyu kaldırmak için "Temizle" butonuna tıklayın.

**Çıktı:**
- **PDF Kaydet** → standart A3/A4 PDF; `rapor/` klasörüne önerilen yol
- **SVG Kaydet** → vektörel SVG; web/sunum için

### Pafta İçeriği

Pafta üç katmandan oluşur:
1. **Çerçeve** — sayfa kenar çizgisi (0.5mm)
2. **Viewport** — tesisat çizimi (entity'ler + MEP ağı, ölçeklenmiş)
   - Temiz su borular: **mavi** (0.4mm)
   - Pis su borular: **kahverengi** (0.4mm)
   - CAD entity'leri (DXF/DWG): katman rengi
3. **Başlık Bloğu** — sayfanın alt kısmında, ISO 7200 formatında

> **Not:** Pafta çizimi, anlık çizim durumunu yansıtır. DN etiketleri ve riser diyagramı ayrı dosyalarda saklanır — bunları paftaya dahil etmek için `RISER` + `PDF Kaydet` kullanın.

---

## Bölüm 25 — Sıcak Su Modülü {#hotwater}

VKT, soğuk su şebekesiyle paralel, renk kodlu ayrı bir sıcak su ağı çizmeyi destekler.

### Sıcak Su Kaynağı Yerleştirme

```
Çizim → Sıcak Su Kaynağı Yerleştir
Komut: SOFBEN  veya  KAZAN
```

Şofben veya kazan konumuna tıklayın → kırmızı `HotSource` node eklenir.

### Sıcak Su Borusu Çizme

```
Çizim → Sıcak Su Borusu
Komut: SICAK-SU
```

Boru modu kırmızıya döner. İki nokta tıklama — soğuk su ile aynı çizim iş akışı. ESC ile çıkılır.

### Renk Kodlaması

| Boru Tipi | Renk |
|-----------|------|
| Soğuk su yatay | Açık mavi |
| Soğuk su kolon | Camgöbeği (cyan) |
| **Sıcak su yatay** | **Kırmızı** |
| **Sıcak su kolon** | **Turuncu** |
| Pis su | Kahverengi |

### Hidrolik Hesap

Sıcak su boruları TS EN 806-3 ile hesaplanır (soğuk suyla aynı motor). DN etiketleri overlay'de kırmızı ile gösterilir. **Tesisatı Kabul Et** adımında `SK-001`, `SK-002`... numaralandırması uygulanır.

---

## Bölüm 26 — Tesisatı Kabul Et {#accept}

Proje çizimi tamamlandığında doğrulama ve numaralandırma:

```
Analiz → Tesisatı Kabul Et
Kısayol: Ctrl+Enter
Komut: KABUL  veya  ACCEPT
```

### Kontrol Listesi

| Kontrol | Beklenen Durum |
|---------|---------------|
| Kaynak varlığı | En az 1 `Source` veya `HotSource` |
| Açık uçlar | Tümü bağlı olmalı (kırmızı halkalar kaybolmalı) |
| Boru sayısı | En az 1 boru |

Hatalar varsa, iletişim kutusunda listelenir — düzeltin ve tekrar `KABUL`.

### Otomatik Numaralandırma

| Prefix | Boru Tipi |
|--------|----------|
| `P-001`... | Soğuk su (temiz su) |
| `SK-001`... | Sıcak su |
| `PS-001`... | Pis su / drenaj |

### Kabul Sonrası İş Akışı

1. `KABUL` → hata listesi yoksa numaralandırma uygulanır.
2. `AutoHydro` otomatik tetiklenir — DN etiketleri güncellenir.
3. `RISER` → kolon şemasında `P-/SK-/PS-` prefix'li borular.
4. `BOM` → keşif listesinde numaralı metrajlar.
5. `DN-OVERRIDE` → Hesap Föyü XLS'de numaralı satırlar.

---

## Hızlı Başlangıç — Tam İş Akışı

```
1.  Dosya → Yeni Proje...          → ad, müşteri, norm seçimi, Tamam
2.  DEVRE (Ctrl+Shift+D)           → boru cinsi, pürüzlülük, max hız, norm ayarla
3.  Mimari → Mimari Belirle...     → kat tanımla + global ref X/Y + DXF dosyaları
4.  Mimari → 3D Hizalama Kontrolü... → kot/yükseklik doğrula
5.  ST Cihazları panelinden Lavabo, WC, Duş yerleştir
6.  PIPE     → soğuk su ana boru hatları (snap aktif)
7.  SOURCE   → şebeke giriş noktası
8.  SICAK-SU → sıcak su boru hatları (kırmızı)
9.  SOFBEN   → şofben / kazan konumu
10. BAGLA    → armatürleri boru hattına bağla (toplu seçim destekli)
11. HYDRAULICS → DN etiketleri görünür
12. BASKI    → hangi değerlerin görüneceğini seç (DN/debi/uzunluk...)
13. HIDROFOR → pompa boyutlandırma
14. BASINC   → parçaların basınç kaybı tablosu — kritik devre kontrolü
15. KATMAN   → Pis Su katmanını görünür yap (Temiz Su'yu gizle)
16. PIS-SU   → pis su borularını çiz
17. AKILLI   → ST panelinden Akıllı Bağlantı Noktası — sadece sembol
18. YER-SUZGECI + ROGAR → drenaj bağlantısı
19. BOSALTMA → ana tahliye noktasını (en alt Drain) işaretle
20. KOPYA-KAT → tekrar eden katları kopyala (kolonlar otomatik dışarıda)
20b. KOLON   → katlar arası dikey pis su borusu
21. YAGMUR   → yağmur suyu boyutlandırması (EN 12056-3)
22. KABUL    → doğrulama + numaralandırma (P-/SK-/PS-)
23. RISER    → kolon şeması önizle, PDF/SVG kaydet
24. DN-OVERRIDE → gerekirse manuel DN düzelt, XLS hesap föyü
25. BOM      → keşif listesi (metraj + bağlantı)
26. WORD     → Word RTF rapor oluştur (Word / WordPad doğrudan açar)
27. Analiz → Rapor Dışa Aktar → rapor/ klasörüne
```

---

## Bölüm 27 — Pis Su Katman Yönetimi

### Hangi Katman Görünsün?

`Görünüm → Katman Görünürlüğü` (Ctrl+Shift+L) ile her sistemi bağımsız açıp kapatın:

| Seçim | Kullanım |
|-------|----------|
| Sadece Pis Su | Drenaj çizimi sırasında temiz su hatlarını gizle |
| Temiz Su + Pis Su | İki sistemi birlikte gör, bağlantıları doğrula |
| Tüm Katmanlar | Pafta almadan önce tam görünüm |

**Komut:** `KATMAN`  
**Kısayol:** Ctrl+Shift+L

---

## Bölüm 28 — Akıllı Bağlantı Noktaları

### Mimari Planda Cihaz Zaten Varsa

Banyoda WC, lavabo veya küvet mimaride zaten çiziliyse tekrar sembol eklemek gereksizdir.
Bunun yerine **sadece pis su bağlantı noktasını** (magenta × sembolü) yerleştirin:

**ST Cihazları Paneli Yöntemi (Önerilen):**
1. Sağ panelde **ST Cihazları** sekmesini açın.
2. Alt kısımdaki **mor "Akıllı Bağlantı Noktası"** checkbox'ını işaretleyin.
3. Listeden cihaz tipini (WC, Lavabo vb.) seçip çift tıklayın.
4. Cihazın pis su çıkış noktasına tıklayın.
5. Ekrana **magenta × sembolü** gelir — "Baglanti" etiketiyle.

**Komut:** `AKILLI` veya `AKILLI-BAGLANTI`

| Cihaz Durumu | Yöntem |
|-------------|--------|
| Mimaride zaten çizili (WC, Lavabo) | Akıllı Bağlantı Noktası |
| Mimaride çizimi yok (Yer Süzgeci) | Normal `YER-SUZGECI` |
| Boşaltma noktası / Rögar | `ROGAR` komutu |

---

## Bölüm 29 — Ana Tahliye Noktası (Boşaltma Noktası)

### Ana Kanalizasyon Bağlantısını İşaretleme

Zemin katta tüm pis su kolonu birleştikten sonra ana tahliye noktasını sisteme tanıtın:

1. `BOSALTMA` veya `Çizim → Ana Tahliye Noktasını İşaretle` komutunu çalıştırın.
2. VKT, **en düşük Z kotundaki Drain node'u** otomatik bulur.
3. Bu nokta overlay'de **turuncu "[ANA TAHLİYE]"** etiketi ile vurgulanır.
4. Raporlarda (BOM, Word raporu) bina kanalizasyon bağlantısı olarak gösterilir.

**Komut:** `BOSALTMA`

> **İpucu:** Birden fazla Drain node'u varsa sistematik çalışın: önce rögarları yerleştirin
> (`ROGAR`), ardından `BOSALTMA` ile en alttakini işaretleyin.

---

## Bölüm 30 — Pis Su Tesisat Hesapları {#pis-su-hesap}

Bu bölüm, pis su tesisatının tamamlandıktan sonra VKT'de nasıl hesaplanacağını ve raporlanacağını adım adım anlatır.

---

### Adım 1 — Tesisatı Kabul Et

Hesaplara geçmeden önce çizimin hatasız olduğunu programa onaylatın.

```
KABUL   veya   Çizim → Tesisatı Kabul Et   (Ctrl+Enter)
```

- VKT açık uç kontrolü + kaynak varlık kontrolü yapar.
- Tüm borular **PS-** (pis su), **P-** (temiz su), **SK-** (sıcak su) prefix ile numaralandırılır.
- Hata varsa dialog listesi çıkar — önce hataları düzeltin.

> **İpucu:** VKT, `KABUL` komutunu çalıştırmadan da otomatik hesap (`AutoHydro`) yapar; ancak numaralandırma için KABUL zorunludur.

---

### Adım 2 — Pis Su Hesap Föyünü Açma

Tüm drenaj borularının hesap tablosunu görüntüleyin:

```
PIS-HESAP   veya   Analiz → Pis Su Hesap Foyu
```

**Tablo sütunları:**

| Sütun | Açıklama |
|-------|---------|
| Boru No | KABUL'de atanan numara (örn. PS-03) |
| **Boru Cinsi** | Yatay boru / Kolon borusu |
| Malzeme | PVC, PP, PE, Beton |
| DN (mm) | Manning ile hesaplanan standart çap |
| L (m) | Çizimden otomatik algılanan uzunluk |
| DU | Kümülatif Deşarj Birimi |
| Q (L/s) | EN 12056-2: Q = K × √ΣDU |
| Eğim i (%) | Boru eğimi |
| **Doluluk h/d (%)** | Gerçek doluluk — EN 12056 max %50 |

**Renk uyarıları:**
- 🔴 Kırmızı: h/d > %50 — çap büyütülmeli
- 🟡 Sarı: h/d %40–50 — dikkat bölgesi

---

### Adım 3 — Devre Seçeneklerini Belirleme

Hesap yöntemini ve boru özelliklerini ayarlayın:

```
DEVRE   veya   Analiz → Devre Secenekleri   (Ctrl+Shift+D)
```

**Ayarlanabilir parametreler:**

| Parametre | Açıklama |
|-----------|---------|
| Bina tipi | Konut / Otel / Endüstri (K faktörü) |
| Boru cinsi | PVC / PP / PE / Beton (pürüzlülük) |
| Pürüzlülük (mm) | Manning n katsayısı için |
| Ana eğim | Yatay borular için varsayılan eğim (örn. %2) |
| Norm | **EN 806-3** (sarfiyat birimi) veya **DIN 1988-300** |

"Tamam"a tıklayınca tüm borular seçilen norma göre anında yeniden hesaplanır.

---

### Adım 4 — Ek Hesaplar (Fosseptik / Çukur / Pompa)

Kanalizasyon bağlantısı olmayan projelerde ek hesaplar gerekebilir:

#### 4a. Kapalı Çukur / Foseptik

```
FOSEPTIK   veya   Analiz → Kapali Cukur / Foseptik Hesabi
```

- Girin: **kişi sayısı** + **kişi başı günlük su tüketimi** (m³/kişi/gün)
- Kırmızı değerler = VKT'nin hesapladığı sonuçlar (TS 822 + EN 12566-1)
- Çift odalı seçenek: 1. oda %67, 2. oda %33 (EN 12566-1 Md. 5.3)

#### 4b. Emdirme Çukuru

```
EMDIRME   veya   Analiz → Emdirme Cukuru Hesabi
```

- Toprak tipi seçimi (kum-çakıl / kumlu-kil / killi / ağır kil)
- Perkolasyon testi süresi girişi → gerekli yüzey alanı m² hesabı

#### 4c. Pis Su Çukuru (Geçirimsiz Depolama)

```
PIS-CUKUR   veya   Analiz → Pis Su Cukuru Hesabi
```

- Foseptikten farkı: arıtma yok — tanker ile periyodik tahliye
- Tanker aralığı + emniyet katsayısı → toplam depolama hacmi m³

#### 4d. Pis Su Pompası

```
PIS-POMPA   veya   Analiz → Pis Su Pompasi Boyutlandirma
```

- Statik yükseklik + boru kayıpları → manometrik yükseklik
- Hesap gücü → standart motor serisi (0.37–7.5 kW)

---

### Adım 5 — Keşif Listesi

```
BOM   veya   KESIF   (Ctrl+K)
```

- Tüm malzemelerin boru tipi + çap + boy bazında listesi
- DN gruplarına göre toplam metraj

---

### Adım 6 — Baskı İçeriği Seçimi

Raporda ve çizimde hangi değerlerin görüneceğini seçin:

```
BASKI   veya   Analiz → Baskı İçeriği
```

**Temiz su boru etiketleri:** DN / Q (debi) / L (uzunluk) / v (hız) / ΔH (kayıp)

**Pis su ek etiketleri:**
- ☑ Eğim i (%) — yatay boru eğimi
- ☑ Doluluk h/d (%) — Manning doluluk oranı

---

### Adım 7 — Word/HTML Rapor Oluşturma

```
WORD   veya   Analiz → Word/HTML Rapor
```

Rapor içeriği:
- Devre seçenekleri (norm, bina tipi, boru cinsi)
- Boru hesap föyü (baskı içeriğinde seçilenler)
- Kritik devre özeti
- Armatür / keşif listesi

Rapor **`rapor/`** klasörüne `.rtf` uzantısıyla kaydedilir — Word ve WordPad doğrudan açar.

> **Not:** VKT'de proje klasörü `ProjectManager` ile yönetilir. Proje kök klasörünü ayarlarsanız (`Dosya → Proje Kök Klasörü Ayarla`) rapor otomatik doğru yola kaydedilir.

---

### Adım 8 — Hesapları Çizime Yaz (Çizimi Güncelle)

Hesap sonuçlarını boru etiketleri olarak çizime işleyin:

```
GUNCELLE   veya   Analiz → Cizimi Guncelle   (Ctrl+Shift+U)
```

**Seçenekler:**
- ☑ Boru çapı (DN / mm)
- ☑ Boru boyu (L)
- ☑ Eğim i (%) — Pis Su
- ☑ Doluluk h/d (%) — Pis Su

"Tamam" → AutoHydro yeniden çalışır → seçilen değerler overlay etiketleri olarak çizimde görünür.

> **FineSANI karşılığı:** "Otomatik Yerleştirme" butonu — aynı işlev, aynı sonuç.

---

### Adım 9 — Kolon Şeması Oluşturma

```
RISER   veya   Analiz → Kolon Semasi   (Ctrl+R)
```

VKT kolon şemasını üç formatta export eder:

| Format | Açıklama |
|--------|---------|
| **SVG** | Tarayıcıda açılır, ölçeklenebilir vektör |
| **PDF** | A3 Yatay, baskıya hazır |
| **DXF** | R12 format, CAD programlarında açılır — FineSANI .dwg eşdeğeri |

DXF dosyası üzerinde AutoCAD / LibreCAD'de istenilen manuel düzenlemeler yapılabilir.

> **Not:** DXF dosyası `rapor/` klasörüne kaydedilir. Sonradan erişmek için `Dosya → Proje Klasörünü Aç` ile klasörü açın.

---

### Pis Su Hesapları — Hızlı Komut Referansı

| Adım | Komut | Ctrl Kısayolu | Açıklama |
|------|-------|--------------|---------|
| 1 | `KABUL` | Ctrl+Enter | Tesisatı doğrula + numaralandır |
| 2 | `PIS-HESAP` | — | Pis su hesap föyü tablosu |
| 3 | `DEVRE` | Ctrl+Shift+D | Hesap normu, eğim, boru cinsi |
| 4a | `FOSEPTIK` | — | Kapalı çukur / foseptik hacmi |
| 4b | `EMDIRME` | — | Emdirme çukuru yüzey alanı |
| 4c | `PIS-CUKUR` | — | Geçirimsiz depolama tankı |
| 4d | `PIS-POMPA` | — | Pis su pompası gücü |
| 5 | `BOM` | Ctrl+K | Keşif listesi + malzeme metrajı |
| 6 | `BASKI` | — | Çizim etiketi seçici |
| 7 | `WORD` | — | HTML/Word rapor çıktısı |
| 8 | `GUNCELLE` | Ctrl+Shift+U | Hesapları çizime yaz |
| 9 | `RISER` | Ctrl+R | Kolon şeması (SVG/PDF/DXF) |

---

## Bölüm 31 — Çıktı Dosyasının Hazırlanması {#cikti}

Bu bölüm, projenin tamamlanmasının ardından çizim dosyalarının nasıl hazırlanacağını açıklar. VKT'nin çıktı iş akışı FineSANI'nin xref/blok tabanlı yaklaşımına eşdeğer sonuçlar üretir — daha az adımla.

---

### Adım 1 — Katman Görünürlüğünü Ayarla

Çıktı dosyasında temiz su ve pis su tesisatını ayrı ya da birlikte gösterebilirsiniz:

```
KATMAN   veya   Görünüm → Katman Görünürlüğü   (Ctrl+Shift+L)
```

- ☑ Temiz Su (mavi borular)
- ☑ Sıcak Su (kırmızı borular)
- ☑ Pis Su (kahverengi borular)

Sadece pis su çıktısı almak istiyorsanız diğer ikisini kapatın; karışık çıktı için üçü de açık bırakın.

> **FineSANI karşılığı:** "Otonet → Uygulama Katmanlarını Seç" + katman yöneticisinde xref kontrolü

---

### Adım 2 — Tam Proje DXF Export (Tüm Katlar)

Projenin tamamını (mimari arka plan + MEP şebekesi) tek DXF dosyasına aktarın:

```
EXPORT-DXF   veya   Dosya → Tam Proje DXF Olarak Kaydet   (Ctrl+Shift+E)
```

- DXF R2000 formatı — AutoCAD 2000+ ve tüm CAD yazılımları açar
- Tüm katlar, tüm boru tipleri, node etiketleri dahil
- Rapor klasörüne (`rapor/`) kaydedilir

---

### Adım 3 — Kat Bazlı DXF Export (FineSANI "Ekran Çizimi" Eşdeğeri)

Her katı ayrı DXF dosyası olarak çıkarmak için:

```
KAT-DXF   veya   Dosya → Aktif Kat DXF Olarak Kaydet
```

**İş akışı:**
1. Komut çalıştırılınca kat seçim listesi açılır
2. Export edilecek katı seçin (örn. "Zemin Kat — kot=0.00m")
3. Dosya adı otomatik önerilir (örn. `Proje_ZeminKat.dxf`)
4. VKT o kattaki MEP node + edge'leri + CAD entity'lerini filtreler
5. DXF dosyası kaydedilir

**Tüm katlar için tekrarlayın** → her kat ayrı DXF dosyasına çıkar.

> **FineSANI karşılığı:** Otob → Ekran Çizimi → isim ver → xref bağla → kaydet süreci  
> VKT'de xref/bind adımı yoktur — tek komutla aynı sonuç.

---

### Adım 4 — Kolon Şeması DXF/PDF

Temiz su ve pis su kolon şemalarını çıktıya ekleyin:

```
RISER   (Ctrl+R)
```

Açılan diyalogdan:
- **DXF Kaydet** → CAD ortamında düzenlenebilir kolon şeması
- **SVG Kaydet** → vektör grafik, tarayıcıda açılır
- **PDF Kaydet** → A3 Yatay, baskıya hazır

> **FineSANI karşılığı:** Kolon şeması → oluştur → .dwg dosyası  
> VKT DXF formatında üretir; AutoCAD/LibreCAD'de açıp manuel düzenleme yapabilirsiniz.

---

### Adım 5 — Pafta Düzeni ve PDF Çıktı

Tüm katları tek paftaya toplamak ve ISO 7200 başlık bloğuyla PDF almak için:

```
PAFTA   (Ctrl+P)
```

**Pafta ayarları:**
- Sayfa: A3 / A4, Yatay / Dikey
- Ölçek: Otomatik ya da manuel (1:50, 1:100 vb.)
- Başlık Bloğu (ISO 7200): Proje adı / Pafta no / Firma / Çizen / Tarih
  - ProjectManager'dan otomatik doldurulur
- Çıktı: **PDF** veya **SVG**

> **FineSANI karşılığı:** Boş sayfaya blok ekleme + antet şablonu  
> VKT'de tek diyalogda tüm ayarlar yapılır; "patlat" adımı gerekmez.

---

### Adım 6 — Raporları Dışa Aktar

```
WORD        →  HTML rapor (Word + tarayıcı açar)
BOM         →  Keşif listesi + malzeme metrajı
EXPORT-PDF  →  XLS/CSV rapor
```

Tüm raporlar `rapor/` klasörüne kaydedilir. Proje klasörünü açmak için:

```
Dosya → Proje Klasörünü Aç
```

---

### Çıktı İş Akışı — Komut Referansı

| Adım | Komut | Kısayol | Açıklama |
|------|-------|---------|---------|
| 1 | `KATMAN` | Ctrl+Shift+L | Görünürlük filtresi (temiz/pis/sıcak su) |
| 2 | `EXPORT-DXF` | Ctrl+Shift+E | Tam proje → DXF (tüm katlar) |
| 3 | `KAT-DXF` | — | Tek kat → DXF (FineSANI ekran çizimi) |
| 4 | `RISER` | Ctrl+R | Kolon şeması → DXF / SVG / PDF |
| 5 | `PAFTA` | Ctrl+P | Pafta düzeni + PDF çıktı |
| 6 | `WORD` | — | Hesap raporu (HTML/Word) |
| 6 | `BOM` | Ctrl+K | Keşif listesi |

---

### VKT vs. FineSANI Çıktı Karşılaştırması

| FineSANI Adımı | VKT Karşılığı | Avantaj |
|---------------|-------------|---------|
| Uygulama katmanlarını seç | `KATMAN` (Ctrl+Shift+L) | Tek diyalog |
| Xref açık mı kontrol et | Gerekmez — VKT otomatik | Adım yok |
| Otob → Ekran çizimi → isim ver | `KAT-DXF` komutuna | Tek komut |
| Yeni çizim → xref bağla → kaydet | Gerekmez — dahili | Adım yok |
| Tüm katları boş sayfaya blok ekle | `PAFTA` → PDF | 1 diyalog |
| Kolon şeması → .dwg | `RISER` → DXF Kaydet | SVG/PDF da var |
| Blokları patlat → antet şablonu | Gerekmez | Yıkıcı işlem yok |

> **Kritik Not:** VKT'de "patlat" (explode) komutu yoktur çünkü gerekmez —  
> tüm geometri her zaman düzenlenebilir haldedir. FineSANI'de patlatma yalnızca  
> bir kez yapılabildiği ve geri alınamaz olduğu için büyük bir risk faktörüdür.

---

---

## Bölüm 30 — Fare Kullanımı — AutoCAD Uyumlu {#fare-kullanimi}

VKT fare davranışı AutoCAD klavuzlarıyla örtüşür. Daha önce AutoCAD veya FineSANI kullanan teknikerlerin alışkanlıkları doğrudan çalışır.

### Temel Fare Eylemleri

| Eylem | Sonuç |
|-------|-------|
| **Sol tık** | Komut sırasında nokta girişi / seçim modunda entity seç |
| **Sol tık + sürükle** (boş alan) | Seçim kutusu çiz (bkz. Bölüm 33) |
| **Sağ tık** | Bağlamsal menü aç |
| **Orta tuş basılı + sürükle** | Pan (ekranı kaydır) — imleç el/yumruk |
| **Orta tuşa çift tıkla** | **Zoom Extents** — tüm çizim ekrana sığar |
| **Tekerlek yukarı** | Zoom In — imleç merkezli |
| **Tekerlek aşağı** | Zoom Out — imleç merkezli |
| **Shift + Tekerlek** | Yatay kaydırma |

### Pan İmleci Davranışı

- Orta tuşa **basılır**: imleç açık el (`⭯`) olur.
- 2 piksel hareket edilince: imleç kapalı yumruk (`✊`) olur — gerçekten pan yapıldığını gösterir.
- Tuş bırakılınca: imleç normale döner.

> **Not:** AutoCAD'de Spacebar veya Enter ile komut tekrarlama VKT'de desteklenmez; komut tekrarlama için CommandBar'a tekrar yazın veya menüden seçin.

### Sağ Tık Bağlamsal Menüsü

Sağ tıklama konumuna ve aktif moda göre farklı seçenekler sunar:

| Durum | Menü İçeriği |
|-------|-------------|
| Aktif çizim komutu | İptal Et |
| MEP node seçili | Node Sil · Özellikler |
| CAD entity seçili | Entity Sil · Seçimi Kaldır |
| Boş alan | Zoom Extents · Boru Çiz · Geri Al · Yinele |

---

## Bölüm 31 — Seçim Araçları — Pencere / Kesişim {#secim-araclari}

VKT, AutoCAD'in Window (Pencere) ve Crossing (Kesişim) seçim yöntemlerini eksiksiz destekler.

### Seçim Kutusu Başlatma

Seçim modunda (`ESC` ile tüm komutlar iptal edilmeli) **boş alana sol tıklayıp sürükleyin**.

### Window Seçimi (Soldan Sağa)

- Fareyi **soldan sağa** sürükleyin.
- **Mavi düz çerçeve** + açık mavi dolgu belirir.
- Kutu içine **tam olarak giren** entity'ler seçilir.
- Etiket: **"Pencere"** (sol üst köşe)

```
Kullanım: Yalnızca belirli bir oda veya bölgede çalışmak istediğinizde.
Örnek: Mutfak lavabosunu çizimden izole etmek için sadece o alanı box-seç.
```

### Crossing Seçimi (Sağdan Sola)

- Fareyi **sağdan sola** sürükleyin.
- **Yeşil kesikli çerçeve** + açık yeşil dolgu belirir.
- Kutu ile **kesişen tüm** entity'ler seçilir (tam içinde olmasa da).
- Etiket: **"Kesişim"** (sol üst köşe)

```
Kullanım: Bir duvarı geçen boru segmentlerini toplu silmek istediğinizde.
Örnek: Yeniden çizilecek bir boru güzergahını crossing ile seçip Delete.
```

### Seçim Sonrası İşlemler

| Tuş / Eylem | Sonuç |
|-------------|-------|
| **Delete** | Seçili tüm entity'leri sil (undo'lanabilir) |
| **ESC** | Aktif seçim kutusunu iptal et; sonra tekrar ESC → seçimi temizle |
| Başka entity'ye sol tık | Seçimi temizler, yeni entity'yi seçer |

### Çoklu MEP Node Seçimi

MEP node'ları (armatür, kavşak) da Crossing seçimi ile toplu silinebilir:

```
Sağdan sola seçim kutusu çiz → Delete → CompositeCommand ile tek Undo
```

---

## Bölüm 32 — Snap Sistemi — Gelişmiş {#snap-sistemi}

### Snap Öncelik Sırası

Birden fazla snap noktası aynı anda etkili olabilir. VKT aşağıdaki öncelik sırasına göre en uygun olanı seçer:

| Öncelik | Snap Tipi | Sembol | Renk |
|---------|----------|--------|------|
| 1 (en yüksek) | **Endpoint** — uç nokta | Kare □ | Sarı |
| 2 | **Center** — daire/yay merkezi | Daire ○ + artı | Cyan |
| 3 | **Intersection** — çizgi kesişimi | × | Magenta |
| 4 | **Midpoint** — orta nokta | Üçgen △ | Yeşil |
| 5 | **Perpendicular** — dik açı | ⌐ | Mavi |
| 6 | **Nearest** — en yakın nokta | Elmas ◇ | Açık mavi |
| 7 (en düşük) | **Grid** — ızgara noktası | Artı + nokta | Beyaz |

### Snap Toggle — F3

**F3** tuşu Kesişim (Intersection) snap'ini açıp kapatır.

- Büyük DXF projelerde (15.000+ entity) kesişim snap, AABB ön filtresi sayesinde otomatik olarak yalnızca imleç civarındaki entity'leri kontrol eder — yavaşlama olmaz.
- Snap toleransı zoom seviyesine göre otomatik ayarlanır.

### Snap Görsel Göstergeler

Snap noktası yakalanınca:
1. Sembol (üstteki tabloya göre) snap noktasında çizer.
2. Crosshair ortasında küçük bir boşluk açılır (snap aktif olduğunu gösterir).
3. Sağ üst köşede etiket (UÇ / ORTA / MERKEZ / KESİŞİM / DİK / YAKIN / GRİD) belirir.

### Snap ile Boru Çizimi

```
1. PIPE komutunu başlat
2. Fare ile ucuna yaklaş → sarı kare (Endpoint snap)
3. Sol tık → boru başlangıcı tam uca sabitlendi
4. İkinci noktaya yürü → rubber-band önizleme
5. Sol tık → boru segmenti tamamlandı
```

> **İpucu:** Snap sembolü görünmeden tıklamayın — boru boşlukta kalır ve açık uç (kırmızı halka) uyarısı verir.

---

## Bölüm 33 — Katman Yöneticisi {#katman-yoneticisi}

### Katman Paneli

Ana pencerenin sağında **"Katmanlar"** dock paneli bulunur. DXF/DWG dosyasındaki tüm katmanlar burada listelenir.

### Seçimle Otomatik Vurgulama

Bir CAD entity'si seçildiğinde (sol tık ile):
- Entity'nin ait olduğu katman panelde **kalın + altın sarısı arka planla** vurgulanır.
- Panel başlığı **"Katmanlar — [katman adı]"** olarak güncellenir.
- Liste, vurgulanan satıra kaydırılır.

Bu davranış AutoCAD'in "Properties → Layer" kutusunun anlık güncellenmesiyle aynı amacı taşır — hangi katmanda çalıştığınızı her an görürsünüz.

### Katman Görünürlüğü

MEP sistemleri (Temiz Su / Sıcak Su / Pis Su) katman bazlı kapatılabilir:

```
Görünüm → Katman Görünürlüğü   (Ctrl+Shift+L)
Komut: KATMAN
```

| Seçenek | Tipik Kullanım |
|---------|---------------|
| ☑ Temiz Su | Temiz su çizimi + hidrolik analiz |
| ☑ Sıcak Su | Sıcak su devresini ayrı kontrol et |
| ☑ Pis Su | Drenaj sistemi çizimi |

Kapalı sistemlerin boru çizgileri ve DN etiketleri gizlenir — AutoHydro hâlâ tüm sistemi hesaplar.

### Katman Renklerini DXF'den Alma

DXF import sırasında her katmanın rengi orijinal dosyadan alınır. Katman rengi değiştirmek için DXF dosyasını kaynak programında (AutoCAD, LibreCAD) düzenleyin ve yeniden import edin.

> **Not:** VKT'de şu an için katman renk düzenleme dialog'u yoktur. Bu özellik gelecek sürümlerde planlanmaktadır.

---

---

## Bölüm 34 — Firma Logosu & Pafta Export {#logo-pafta}

Bu bölüm, mühendislik bürolarının pafta çıktılarına logo eklemesini ve VKT'nin PDF/SVG export özelliklerini kapsamlı şekilde açıklar.

---

### Firma Logosu Yükleme

Logo, ISO 7200 başlık bloğunun **FİRMA** hücresine gömülür. PDF ve SVG çıktılarının her ikisinde de görünür.

```
Analiz → Pafta Düzenle ve Yaz...   (Ctrl+P)
```

Açılan diyalogda Başlık Bloğu bölümünde:

| Adım | İşlem |
|------|-------|
| 1 | **"Yükle..."** butonuna tıkla |
| 2 | PNG / JPG / BMP / SVG dosyasını seç |
| 3 | 120×40 piksel önizleme alanda logo görünür |
| 4 | **PDF Kaydet** veya **SVG Kaydet** ile çıktı al |
| 5 | Logoyu kaldırmak için **"Temizle"** butonu |

**Desteklenen formatlar:**

| Format | PDF | SVG | Notlar |
|--------|-----|-----|--------|
| PNG | ✅ | ✅ | Şeffaf arka plan desteklenir |
| JPG / JPEG | ✅ | ✅ | Beyaz arka plan önerilir |
| BMP | ✅ | ✅ | |
| SVG | ✅ | ✅ | SVG'de base64 olarak gömülür |

---

### PDF Çıktısında Logo Konumu

Logo, **FİRMA** hücresinin sağ yarısına 1 mm iç boşlukla yerleştirilir. Aspect ratio (en/boy oranı) otomatik korunur; logo hücreye sığacak şekilde ölçeklenir ve ortalanır.

```
┌──────────────────────┬──────────────┬────────────┐
│     PROJE ADI        │  FİRMA adı   │  LOGO      │
│     ————————————     │  ──────────  │  [Resim]   │
│     PAFTA ADI        │  STANDART    │  ölçek/tar │
└──────────────────────┴──────────────┴────────────┘
```

> **İpucu:** Logo dosyası taşınırsa bir sonraki açılışta VKT logoyu otomatik olarak yeniden yükler (`SetInitialTitleBlock` logo restore). Logo dosyası silinmişse hücre boş kalır.

---

### SVG Çıktısında Logo

SVG formatında logo, dosya içine **Base64 olarak gömülür** — ayrı bir logo dosyasına gerek kalmaz. Gönderilen SVG dosyası bağımsız çalışır.

```html
<!-- SVG içindeki logo örneği -->
<image x="..." y="..." width="..." height="..."
       preserveAspectRatio="xMidYMid meet"
       href="data:image/png;base64,iVBORw0KGgo..."/>
```

---

### Pafta Export Karşılaştırması

| Özellik | PDF | SVG |
|---------|-----|-----|
| Format | İkili (baskı kalitesi) | Vektörel XML |
| Kağıt boyutu | A3 / A4 Yatay/Dikey | Aynı |
| Ölçek | Otomatik veya 1:20 → 1:500 | Aynı |
| Logo | QPainter ile render | Base64 gömülü |
| Açılış | Adobe Acrobat, tarayıcı | Tarayıcı, Inkscape, CAD |
| Düzenleme | — | SVG editöründe düzenlenebilir |
| Önerilen | Baskı / Onay | Web paylaşımı / Teknik sunum |

---

### Pafta Çıktı İş Akışı — Özet

```
1. KABUL        → tesisatı onayla + numaralandır
2. HYDRAULICS   → DN etiketleri güncelle
3. BASKI        → hangi etiketler görünsün seç
4. PAFTA        → diyalog aç (Ctrl+P)
5.   Kağıt: A3 Yatay
6.   Ölçek: Otomatik veya 1:50
7.   Başlık bloğunu doldur (proje adı, pafta no, çizen)
8.   Logo: Yükle... → firma logosu seç
9.   PDF Kaydet → rapor/ klasörüne
10. RISER       → kolon şeması PDF (A3 Yatay) ayrıca kaydet
```

> **FineSANI farkı:** FineSANI'de pafta antetine logo eklemek mümkün değildir (statik şablon). VKT her projede farklı logo ve başlık bloğu kullanabilir.

---

---

## Bölüm 35 — Düzenleme Komutları: TRIM / OFFSET / MIRROR {#duzen-komutlari}

VKT, AutoCAD'in en sık kullanılan düzenleme komutlarını destekler. Hepsi **Ctrl+Z ile geri alınabilir**.

---

### TRIM — Kısalt

İki çizginin kesişim noktasına göre bir çizgiyi kısaltır.

```
Komut: TRIM  veya  KISALT  veya  TR
Menü: Düzen → Trim (Kısalt)...
Kısayol: T
```

**Adımlar:**
1. Sınır olarak kullanılacak çizgiyi **tek tıkla** seçin.
2. `TRIM` komutunu çalıştırın.
3. Kısaltılacak entity'nin ID'sini girin.
4. Hangi tarafın kesileceğini seçin (başlangıç mı, son mu?).

> **Not:** Şu an Line → Line kesişimi destekleniyor. Her iki entity de `Line` tipinde olmalıdır.

---

### OFFSET — Paralel Kopya

Seçili entity'nin paralel (eşit mesafeli) kopyasını oluşturur.

```
Komut: OFFSET  veya  PARALEL  veya  OFSET
Menü: Düzen → Offset (Paralel Kopya)...
Kısayol: O
```

**Adımlar:**
1. Entity'i seçin (tek tık veya Shift+tık çoklu seçim).
2. `OFFSET` komutunu çalıştırın.
3. Offset mesafesini (mm) girin.
4. Yönü seçin: Sağa / Sola / Her iki yöne.

**Line entity için:** Doğru geometrik dik mesafe (perp-normal) uygulanır.

| Seçenek | Sonuç |
|---------|-------|
| Sağa / Yukarıya | +normal yönde kopya |
| Sola / Aşağıya | −normal yönde kopya |
| Her iki yöne | İki kopya birden |

---

### MIRROR — Ayna (Yansıtma)

Seçili entity'leri bir eksen etrafında yansıtır.

```
Komut: MIRROR  veya  AYNA  veya  YANSI
Menü: Düzen → Ayna (Mirror)...
Kısayol: M
```

**Adımlar:**
1. Entity'leri seçin.
2. `MIRROR` komutunu çalıştırın.
3. Yansıtma eksenini seçin:
   - **Yatay eksen** — Y koordinatları ters çevrilir
   - **Dikey eksen** — X koordinatları ters çevrilir
   - **180° döndür** — Seçimin merkezine göre tam döndürme

> **FineSANI farkı:** FineSANI'de MIRROR komutu yoktur. Simetrik plan tipleri (koridorlar, daire çiftleri) VKT'de tek komutla kopyalanabilir.

---

### SELECT ALL — Tümünü Seç

Çizimdeki tüm görünür entity'leri seçer.

```
Komut: TUMU  veya  SELECTALL
Menü: Düzen → Tümünü Seç
Kısayol: Ctrl+A
```

Seçim sonrası:
- Tüm entity'ler sarı vurgulanır
- Delete → hepsini sil
- MIRROR / OFFSET → hepsine uygula
- ESC → seçimi temizle

---

## Bölüm 36 — Ortho Modu & Copy/Paste {#ortho-copy}

### Ortho Modu (F8)

Boru çizerken fareyi **yalnızca yatay veya dikey** eksende kilitler.

```
Komut: ORTHO  veya  F8
Kısayol: F8 (toggle)
```

| Durum | Status Bar Mesajı |
|-------|------------------|
| Açık | "Ortho AÇIK (F8) — sadece yatay/dikey çizim" |
| Kapalı | "Ortho KAPALI (F8)" |

**Kullanım:** İkinci noktayı seçerken fare önce yatay ya da dikey eksenin hangisinde daha fazla hareket ettiğine bakar ve o ekseni kilitler. Dik boru ve dirsek geometrisi garantilenir.

> **İpucu:** Ortho açıkken bir noktadan yatay çizgi çizmek için fareyi yatay hareket ettirip tıklayın — VKT otomatik olarak aynı Y koordinatını kullanır.

---

### Copy / Paste (Kopyala / Yapıştır)

Entity'leri panoya kopyalayıp farklı konuma yapıştırır.

```
Kopyala: Ctrl+C  /  COPY  /  KOPYALA  /  CP
Yapıştır: Ctrl+V  /  PASTE  /  YAPISTIR
Menü: Düzen → Kopyala / Yapıştır
```

**Kopyalama:**
1. Entity'leri seçin (tek veya Shift+tık çoklu).
2. `Ctrl+C` — panoya kopyalandı mesajı çıkar.
3. Seçim sayısı status bar'da gösterilir.

**Yapıştırma:**
1. `Ctrl+V` — offset dialog açılır.
2. X ve Y öteleme değeri girin (mm, 0 = aynı konum).
3. Yapıştırılan entity'ler otomatik olarak seçili gelir → Delete, Mirror vb. hemen uygulanabilir.

**Örnek kullanım:**
```
Zemin katta lavabo grubu çiz →
Ctrl+A ile tümünü seç →
Ctrl+C ile kopyala →
Ctrl+V → Y=3000 mm (1 kat yüksekliği) →
1. kattaki lavabolar hazır
```

> **Not:** Bu kopyalama yalnızca CAD entity'leri (çizgi, daire, metin) için geçerlidir.  
> MEP ağını (borular, armatürler) katlar arası kopyalamak için **KOPYA-KAT** komutunu kullanın.

---

*VKT v1.0 — © 2026 — TS EN 806-3 · EN 12056-2 · EN 12056-3 · DIN 1988-300 · TS 822 · EN 12566-1 uyumlu*
