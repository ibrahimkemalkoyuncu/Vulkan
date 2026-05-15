# VKT Eğitim Kılavuzu

## 1. Mimari Çizimin Programa Girilmesi

### Genel Bakış
Tesisat projesi başlamadan önce yapının mimari planı programa referans olarak yüklenir. Bu sayede boru çizimi mimari planın üzerine yapılır ve koordinat doğruluğu sağlanır.

### Adım 1: Yeni Proje Oluşturma
- Menü: **Dosya → Yeni** (Ctrl+N)
- VKT yeni bir boş proje açar; tüm katmanlar ve ağ sıfırlanır.

### Adım 2: Birim Kontrolü
- DXF/DWG dosyası import edilmeden önce doğru birim ayarı kritiktir.
- VKT, `$INSUNITS` başlık değişkenini okuyarak mm/cm/m/inch arasında otomatik dönüşüm yapar.
- Import sonrası otomatik zoom-to-extents ile mimari planın doğru boyutta geldiği doğrulanır.

### Adım 3: W-Block / Blok Hazırlama ve Referans Noktası (AutoCAD tarafında)

W-Block (WBLOCK) komutu ile kat planı tek bir DXF dosyasına aktarılır. **En kritik adım referans noktası seçimidir:**

> **Referans Noktası** — Tüm katlarda ortak olan, katlar üst üste getirildiğinde aynı hizada olmasını sağlayan bir noktadır. Kolon köşesi, asansör boşluğu köşesi veya yapının referans aksı başlangıcı gibi tüm planlarda net görülebilen bir nokta seçilmelidir.

**W-Block referans noktası neden önemlidir?**
- AutoCAD'de WBLOCK komutu çalıştırılırken istenilen "baz noktası" bu referans noktasıdır.
- Zemin kat planında A-1 kolon köşesi `(15250, 8300)` ise, 1. kat planında da aynı A-1 köşesi `(15250, 8300)` olmalıdır.
- VKT bu koordinatı alıp tüm entity'leri `(-15250, -8300)` kadar kaydırır; böylece her katta bu nokta `(0, 0)`'a eşlenir.
- 3D görünümde katlar yatayda mükemmel hizalanır; yalnızca Z ekseni (kot) farklıdır.

**AutoCAD'de doğru W-Block çıktısı:**
```
Command: WBLOCK
Dosya adı: zemin_kat.dxf
Kaydet formatı: 2013 DXF
Nesneleri Seç: Tümü (A + Enter)
Baz Nokta: A-1 kolon köşesini tıkla → 15250, 8300, 0
```

Önerilen katmanlar:
- `DUVAR` — bina dış/iç duvarları
- `KAT-PLANI` — kat planı ana çizgisi
- `PENCERE`, `KAPI` — doğrama
- Gereksiz katmanlar (hatch, kota vb.) dışarıda bırakılarak dosya boyutu küçültülür.

### Adım 4: Mimari Belirle — Kat Tanımları
**Menü: Mimari → Mimari Belirle...** (Ctrl+M)

Açılan pencerede:
| Alan | Açıklama |
|------|----------|
| Kat Numarası | Sıralı tanımlayıcı — her zaman 1'den başlar, gerçek kat adını etkilemez |
| Kot (m) | Gerçek döşeme kotu (metre) — istediğiniz değeri girebilirsiniz |
| İsim | "Bodrum Kat", "Zemin Kat", "1. Kat" ... |
| Dosya | İlgili katın DXF veya DWG dosyası |
| Referans X / Y | W-Block baz noktasının bu dosyadaki CAD koordinatı (örn. 15250 / 8300) |

**Kat Numarası ve Kot alanı bağımsızdır.** Program gereği kat numarası 1'den başlar; ancak bu, girilen kot değerini etkilemez. Aşağıdaki gibi bodrum katı da tanımlanabilir:

| Kat No | Kot (m) | İsim |
|--------|---------|------|
| 1 | −1.00 | Bodrum Kat |
| 2 | 0.00 | Zemin Kat |
| 3 | 3.00 | 1. Kat |

> **Referans noktası** tüm katlarda aynı fiziksel noktayı (kolon köşesi, asansör kenarı) göstermelidir. VKT, her katın entity'lerini bu koordinat kadar kaydırarak 3D'de hizalar. Referans noktası sıfır (0, 0) bırakılırsa VKT dosyayı olduğu gibi import eder — katlar hizalanamaz.

**İş akışı:**
1. Kat bilgilerini doldurun → **Yenile** ile listeye ekleyin.
2. Tüm katları listeye ekledikten sonra **Tamam**'a basın.
3. VKT her kat için dosya yolu kayıtlıdır; DXFImportDialog otomatik açılır.
4. Import sonrası her kat ayrı bir katman grubunda görünür.

> Bir satıra tıklayarak seçip **Sil** ile kat silinebilir. Seçili satıra tekrar tıklamak formu düzenleme moduna alır.

### Adım 5: 3D Hizalama
- Farklı kotlardaki planlar FloorManager sayesinde Z-ekseninde doğru konumlandırılır.
- **Görünüm → İzometrik** (Ctrl+5) ile bina 3D aksonometrik görünümde incelenebilir.
- Katlar arası geçiş boruları (kolon) Z-eksenine perpendicular olarak çizilir — VKT bu kısıtı otomatik önerir.

---

## 2. Temel Çizim Komutları

| Komut (CommandBar) | Kısayol / Menü | Açıklama |
|--------------------|---------------|----------|
| `PIPE` / `LINE` | Çizim toolbar | İki noktalı boru çiz; zincirleme devam eder |
| `FIXTURE` | Çizim toolbar | Armatür yerleştir (Lavabo, Duş, WC...) |
| `JUNCTION` | Çizim toolbar | Bağlantı kavşağı |
| `UZAKLIK` / `DISTANCE` | — | İki nokta arası mesafe ölç (mm cinsinden) |
| `MIMARI` | Mimari menü | Mimari Belirle dialog'unu aç |
| `ZE` / `ZOOM-EXTENTS` | — | Tüm çizimi ekrana sığdır |
| `VIEW-PLAN` | Görünüm | Plan (2D tepeden) görünüm |
| `VIEW-ISO` | Görünüm | İzometrik 3D görünüm |
| `HYDRAULICS` | Analiz | Tam hidrolik çözüm (TS EN 806-3) |
| `UNDO` / `REDO` | Ctrl+Z / Ctrl+Y | Geri al / İleri al |
| `HELP` | — | Tüm komutları listele |

---

## 3. Mesafe Ölçüm (UZAKLIK/DISTANCE)

1. CommandBar'a `UZAKLIK` veya `DISTANCE` yazın.
2. **1. nokta** — çizim alanında bir noktaya tıklayın.
3. **2. nokta** — ikinci noktaya tıklayın.
4. Sonuç: Status bar ve log panelinde `Mesafe: XXX.XX mm (X.XXX m)` gösterilir.
5. ESC ile ölçüm iptal edilebilir.

---

## 4. Gerçek Zamanlı Hidrolik (AutoHydro)

Boru veya armatür eklenir/silinir eklenmez 600ms debounce ile otomatik DN boyutlandırma çalışır:
- TS EN 806-3 LU tablosu → debi hesabı
- Darcy-Weisbach → sürtünme kaybı
- Sonuç: Her boru üzerinde **DN etiketi** (mavi) görünür

Manuel tetikleme: **Analiz → Hidrolik Analiz**

---

## 5. Snap Sistemi

VKT snap önceliği (yüksekten düşüğe):

1. **Endpoint** — çizgi/boru ucu
2. **Center** — daire/yay merkezi
3. **Intersection** — iki çizginin kesişimi
4. **Midpoint** — orta nokta
5. **Perpendicular** — dik açılı nokta
6. **Nearest** — en yakın nokta
7. **Grid** — ızgara noktası

Snap noktası sarı işaret ile gösterilir. Snap yakalandığında CommandBar'da snap tipi yazar.

---

## 6. Undo/Redo

- **Ctrl+Z** — son komutu geri al (Düzen → Geri Al)
- **Ctrl+Y** — ileri al (Düzen → İleri Al)
- Tüm boru/armatür/kavşak işlemleri Command Pattern ile kaydedilir; sınırsız geçmiş.
- Undo/Redo sonrası AutoHydro otomatik çalışır; DN etiketleri güncellenir.

---

## 7. Proje Çalışma Alanı (CC Klasörü Eşdeğeri)

VKT'de her proje, merkezi bir kök dizin altında kendi alt klasörünü alır. Bu yapı FineSANI'deki CC klasörü konseptinin tam karşılığıdır.

### Kök Dizini Ayarlama
**Dosya → Proje Kök Klasörü Ayarla...** — Tüm projelerin tutulacağı ana dizini belirleyin. Bu ayar kalıcı olarak saklanır.

### Yeni Proje Oluşturma
**Dosya → Yeni Proje...** (Ctrl+Shift+N)
- Proje adını girin → VKT otomatik olarak şu yapıyı oluşturur:

```
[ProjeKökü]/
└── [ProjeAdı]/
    ├── [ProjeAdı].vkt    ← ana proje dosyası (otomatik kaydedilir)
    ├── mimari/           ← DXF/DWG mimari altlıkların kopyaları
    ├── cikti/            ← dışa aktarılan DXF/blok dosyaları
    └── rapor/            ← hesap raporları (XLS/PDF)
```

### Mimari Altlıkların Aktarımı
**Mimari → Mimari Belirle...** (Ctrl+M) penceresinde dosya seçilirken **"Dosyayı proje mimari/ klasörüne kopyala"** seçeneği işaretlenirse, DXF/DWG dosyası otomatik olarak proje klasörüne kopyalanır ve yol güncellenir.

### Proje Klasörünü Açma
**Dosya → Proje Klasörünü Aç** — Aktif projenin klasörünü Dosya Gezgini'nde açar. Buradan rapor ve çıktı dosyalarına ulaşılabilir.

### Rapor ve Çıktı Yönetimi
- **Rapor kaydet** (Analiz → Rapor Dışa Aktar) — Aktif proje varsa otomatik olarak `rapor/` klasöründen başlar
- **DXF dışa aktarım** — `cikti/` klasörüne kaydedilmesi önerilir

---

## 8. ST Cihazları — Sıhhi Tesisat Kütüphanesi

VKT sağ panelinde **"ST Cihazları"** sekmesi, TS EN 806-3 Tablo 3'e göre tanımlı armatürleri listeler.

### Kullanım
1. Sağ panel → **ST Cihazları** sekmesini açın.
2. Listeden yerleştirilecek cihazı seçip **çift tıklayın**.
3. Status bar: `ST Cihaz [Lavabo]: Yerleştirme noktasını tıklayın`
4. Çizim alanında noktaya tıklayın → armatür yerleştirilir, ağa otomatik eklenir.
5. AutoHydro tetiklenir; DN etiketleri güncellenir.

### TS EN 806-3 Tablo 3 — Yük Birimleri (LU)

| Cihaz | LU | Boru çapı (min.) |
|-------|----|-----------------|
| Lavabo | 0.5 | DN 15 |
| Duş | 1.0 | DN 15 |
| WC (rezervuar) | 2.0 | DN 15 |
| Küvet | 3.0 | DN 20 |
| Evye (mutfak) | 1.5 | DN 15 |
| Bulaşık Makinesi | 1.5 | DN 15 |
| Çamaşır Makinesi | 2.0 | DN 15 |
| Pisuar | 0.5 | DN 15 |
| Bide | 0.5 | DN 15 |

---

## 9. Hesap Föyü — Boru Boyları ve Debiler

### Boru Boyları Nasıl Güncellenir?

VKT, her boruyu çizim üzerindeki iki uç nokta arasındaki Öklid mesafesiyle hesaplar. Güncelleme iş akışı:

1. **Çizimde boru boyunu değiştir** — boruyu silin, yeniden çizin; VKT uzunluğu otomatik hesaplar.
2. **Hidrolik analiz** — **Analiz → Hidrolik Analiz** (veya AutoHydro), hesaplanan boru boyunu Darcy-Weisbach formülünde kullanır.
3. **Rapor** — **Analiz → Rapor Dışa Aktar** ile XLS/PDF raporu üretilir; her boru satırında `L (m)`, `DN`, `v (m/s)`, `ΔP (Pa/m)` gösterilir.

> **FineSANI farkı:** VKT'de boru boyu "elle giriş" yoktur — çizimdeki gerçek uzunluk kullanılır; manuel giriş hatası riski yoktur.

### Debiler Nasıl Belirlenir?

| Büyüklük | Formül / Standart |
|----------|------------------|
| Toplam LU | Σ (armatür LU) — TS EN 806-3 T.3 |
| Debi Q (L/s) | `Q = K × √(Σ LU)` — EN 806-3 Bölüm 9 |
| K katsayısı | Konut: 0.5 / Otel: 0.7 / Endüstri: 1.0 |
| DN seçimi | Darcy-Weisbach → v ≤ 2 m/s, ΔP ≤ 300 Pa/m |

AutoHydro her boru ekleme/silme sonrası bu hesabı otomatik yapar; sonuçlar DN etiketi olarak boru üzerinde görünür.

---

## 10. Foseptik ve Kapalı Çukur Hesapları

### Geçerli Standartlar

| Konu | Standart |
|------|---------|
| Fosseptik boyutlandırma | **TS 822** — İçme ve Kullanma Suyu Kaynaklarının Korunması; İşlenmesi ve Dağıtımı |
| Sızdırmaz kapalı çukur (sızdırmaz depo) | **TS EN 12566-1** — Küçük atıksu arıtma tesisleri |
| Emici kuyu | **TS EN 12566-2** — Filtre yerleşimine uygunluk |
| Arıtma tesisi çıkışı / deşarj | **TS EN 12255** serisi |
| Koku bariyeri / çatı havalandırması | **EN 12056-2** — Bölüm 8 |

### Fosseptik Hacim Hesabı (TS 822 / TS EN 12566-1)

```
V_fosseptik (m³) = n_kişi × V_kişi × T_boşaltma
```

- `n_kişi` — eşdeğer kullanıcı sayısı (PE — Population Equivalent)
- `V_kişi` — kişi başına günlük atıksu hacmi (150–200 L/kişi/gün tipik)
- `T_boşaltma` — boşaltma periyodu (EN 12566-1: min. 90 gün)

**Örnek:** 10 kişi × 0.15 m³/kişi/gün × 90 gün = **135 m³ → 2 bölmeli, 3 m³/bölme minimum**

> Kapalı (sızdırmaz) çukurlar için aynı formül uygulanır; boşaltma sıklığı yerel yönetim kısıtlarına göre belirlenir.

### VKT'de Fosseptik Hesabı

Mevcut sürümde fosseptik hacim hesabı **Analiz → Hidrolik Analiz** raporu içinde "Sistem Özeti" bölümünde gösterilir: toplam DU'dan EN 12056-2 Manning denklemi ile pik debi hesaplanır. Kapasite boyutlandırma için sonuçlar `rapor/` klasörüne dışa aktarılır.

---

## 11. Dosya Formatları

| Format | İçe Aktar | Dışa Aktar |
|--------|-----------|------------|
| `.vkt` | Dosya → Aç | Dosya → Kaydet |
| `.dxf` | Dosya → CAD Dosyası İçe Aktar | Analiz → Rapor Dışa Aktar |
| `.dwg` | Dosya → CAD Dosyası İçe Aktar | — |
| `.xls` | — | Analiz → Rapor Dışa Aktar |
| `.pdf` | — | Analiz → Rapor Dışa Aktar |

---

*VKT v1.0 — © 2026 — TS EN 806-3 + EN 12056-2 uyumlu*
