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

## Hızlı Başlangıç — Tam İş Akışı

```
1.  Dosya → Yeni Proje...     → ad, müşteri, norm seçimi, Tamam
2.  Mimari → Mimari Belirle... → kat tanımla + global ref X/Y + DXF dosyaları
3.  Mimari → 3D Hizalama Kontrolü... → kot/yükseklik doğrula, sarı/kırmızı satır yok mu?
4.  ST Cihazları panelinden Lavabo, WC, Duş yerleştir
5.  PIPE → ana boru hatları (snap aktif)
6.  SOURCE → şebeke giriş noktası
7.  BAGLA → armatürleri boru hattına bağla
8.  HYDRAULICS → DN etiketleri görünür
9.  HIDROFOR → pompa boyutlandırma
10. NORM → gerekirse DIN 1988-300'e geç, otomatik yeniden hesaplar
11. PIS-SU → pis su borularını çiz
12. YER-SUZGECI + ROGAR → drenaj bağlantısı
13. KOPYA-KAT → tekrar eden katları kopyala (kolonlar otomatik dışarıda)
13b. KOLON → katlar arası dikey boru bağlantısı (aynı XY, farklı Z)
14. YAGMUR → yağmur suyu boyutlandırması
15. RISER → kolon şeması önizle, PDF/SVG kaydet
16. DN-OVERRIDE → gerekirse manuel DN düzelt, XLS hesap föyü al
17. BOM → keşif listesi (metraj + bağlantı)
18. Analiz → Rapor Dışa Aktar → rapor/ klasörüne
```

---

---

*VKT v1.0 — © 2026 — TS EN 806-3 · EN 12056-2 · EN 12056-3 · DIN 1988-300 · TS 822 · EN 12566-1 uyumlu*
