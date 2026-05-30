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

## 12. Boru Malzeme Seçimi

Özellikler panelinde (sağ taraf — **"Özellikler (Mühendislik Modu)"**) **Malzeme** açılır menüsü bulunur. Boru çizmeden önce bu alandan malzemeyi seçin — seçim, bundan sonra çizilecek tüm borulara uygulanır.

| Malzeme | Pürüzlülük (mm) | Tipik Kullanım |
|---------|----------------|----------------|
| PVC | 0.0015 | Soğuk su, drenaj |
| PP (Polipropilen) | 0.0015 | Sıcak/soğuk su (PPR borular) |
| PE | 0.0015 | Toprak altı, bahçe |
| Bakır | 0.0015 | Sıcak su, gömme tesisat |
| Çelik | 0.045 | Endüstriyel, yangın |

> **Not:** Seçilen malzemenin pürüzlülük değeri otomatik olarak Darcy-Weisbach hesabına girer; DN boyutlandırma sonuçlarını etkiler.

---

## 13. Pis Su Sistemi

Pis su boruları, yer süzgeçleri ve rögarlar ayrı bir çizim moduyla eklenir.

### Pis Su Borusu
```
Çizim → Pis Su Borusu (kahverengi boru)
Komut: PIS-SU
```
Pis su modu aktifken çizilen borular `Drainage` tipi olarak kaydedilir ve kahverengi renkte görünür. ESC ile temiz su moduna geri dönülür.

### Yer Süzgeci ve Rögar
```
Komut: YER-SUZGECI  → Drain node yerleştirir (yer süzgeci)
Komut: ROGAR        → Drain node yerleştirir (rögar/bosaltma)
```
Her iki komut da tıklanan noktaya bir drenaj girişi ekler. AutoHydro sonrası Manning denklemiyle pik debi hesaplanır.

### EN 12056-2 Discharge Unit (DU)
| Cihaz | DU |
|-------|----|
| Lavabo | 0.5 |
| Duş | 0.6 |
| WC | 2.0 |
| Banyo/Küvet | 1.5 |
| Mutfak Evyesi | 0.8 |

---

## 14. Cihazları Tesisata Bağla (BAGLA)

FineSANI'nin en sık kullanılan özelliği: armatür ile ana boru hattı arasına otomatik dal boru ekler.

```
Çizim → Cihazı Tesisata Bağla  (Ctrl+B)
Komut: BAGLA  veya  CONNECT
```

**İş akışı:**
1. `BAGLA` yazın veya Ctrl+B tuşuna basın.
2. **Adım 1/2:** Armatür node'una tıklayın — seçildiğinde status bar'da `BAGLA: [Lavabo] seçildi` görünür.
3. **Adım 2/2:** Bağlanacak ana boru hattına tıklayın.
4. VKT dik ayak noktasını hesaplar → Junction oluşturur → dal boruyu ekler.
5. Undo/Redo desteklidir.

> **İpucu:** Ana hattın tam üstüne tıklamanız yeterli — VKT en yakın noktayı ve dik bağlantıyı otomatik bulur.

---

## 15. Tesisat Kopyalama — Çok Katlı Projeler

Aynı plan tekrar eden katlarda tesisat bir kere çizilir, diğer katlara kopyalanır.

```
Çizim → Tesisat Kopyala...
Komut: KOPYA-KAT
```

**Dikkat:** Kolonlar (dikey borular, farklı Z'ye uzanan) kopyalamaya **dahil edilmez** — bu kasıtlı bir davranıştır. Kolonları katlar arası bağlamak için **KOLON** komutunu kullanın (bkz. Bölüm 16).

**İş akışı:**
1. `KOPYA-KAT` yazın.
2. Kaynak katı seçin (örn. Zemin Kat).
3. Hedef katı seçin (örn. 1. Kat).
4. VKT kaynak kattaki tüm yatay boru ve armatürleri hedef kata kopyalar.

---

## 16. Kolon Bağlantı Asistanı (KOLON)

Farklı katlardaki tesisatı birbirine bağlayan dikey boruları (kolonlar) eklemek için:

```
Çizim → Kolon Boru Ciz  (Ctrl+Shift+K)
Komut: KOLON  veya  COLUMN
```

**Ön koşul:** En az 2 kat tanımlı olmalı (bkz. Adım 4 — Mimari Belirle).

**İş akışı:**
1. `KOLON` yazın veya Ctrl+Shift+K tuşuna basın.
2. **Kaynak Node:** Açılan listeden kolonun başladığı node'u seçin.
3. **Hedef Kat:** Kolonun uzanacağı katı seçin.
4. VKT aynı XY konumunda hedef katta node arar (±50mm/±0.15m tolerans). Yoksa otomatik oluşturur.
5. Dikey boru eklenir; uzunluk Z farkından hesaplanır; AutoHydro çalışır.

---

## 17. Hesap Normu Seçimi (EN 806-3 / DIN 1988-300)

```
Analiz → Hesap Normu...
Komut: NORM
```

| Norm | Açıklama | φ (eşzamanlılık) |
|------|----------|-----------------|
| **TS EN 806-3** (varsayılan) | Avrupa / Türkiye standardı | √(ΣLU) |
| **DIN 1988-300** | Alman standardı | 1/(1+√LU/10) |

Norm seçiminden sonra sistem **otomatik yeniden hesaplar** — DN etiketleri güncellenir.

---

## 18. Hidrofor Boyutlandırma

```
Analiz → Hidrofor Boyutlandırma...
Komut: HIDROFOR
```

VKT kritik devreyi analiz eder ve:
- Kritik basınç kaybı (mSS)
- Gerekli pompa basma yüksekliği
- Gerekli debi (m³/h)
- Önerilen pompa modeli ve gücü (kW)

bilgilerini gösterir.

---

## 19. Yağmur Suyu Modülü (EN 12056-3)

```
Analiz → Yağmur Suyu Modülü...
Komut: YAGMUR
```

Gerekli girişler:
- **Çatı alanı** (m²)
- **Yüzey tipi** (C katsayısı: beton/kiremit/çakıl vb.)
- Yağış yoğunluğu r_D = 0.03 l/(s·m²) (standart değer)

Sonuç: `Q = C × A × r_D` formülüyle hesaplanan debi, DN tablosu, boru adedi.

---

## 20. Keşif Listesi (BOM)

```
Analiz → Keşif Listesi (BOM)...
Komut: BOM  veya  KESIF  (Ctrl+K)
```

BOM raporu:
- DN'e göre boru metrajları (m)
- T-parça, dirsek, armatür bağlantı sayımı
- Dialog'dan kopyalanabilir veya `rapor/` klasörüne kaydedilebilir

---

## 21. Kolon Şeması (Riser Diyagramı)

```
Analiz → Kolon Şeması (Riser)...
Komut: RISER  (Ctrl+R)
```

VKT, mevcut NetworkGraph ve kat tanımlarından otomatik riser diyagramı üretir:
- SVG önizleme — kat etiketleri, DN ve debi değerleri, lejant
- **"SVG Olarak Kaydet"** → `rapor/` klasörüne
- **"PDF Olarak Kaydet"** → A3 Landscape PDF çıktısı

---

## 22. DN Manuel Override — Hesap Föyü XLS

```
Komut: DN-OVERRIDE  veya  DN-DEGISTIR
```

Otomatik DN boyutlandırma sonuçlarını görmek ve gerekirse manuel değiştirmek için:
1. Tüm borular tablo halinde listelenir.
2. Her boru için QComboBox'tan DN seçilir (DN16 → DN200).
3. **Tamam** → değişiklikler anında uygulanır, overlay güncellenir.
4. **"XLS Olarak Kaydet"** → Özet + Boru Hesap Föyü + Armatür sekmeli .xls dosyası.

---

## 23. 3D Hizalama Kontrolü

```
Mimari → 3D Hizalama Kontrolü...  (Ctrl+Shift+H)
Komut: HIZALAMA
```

Her kat için kontrol tablosu:
| Sütun | Açıklama |
|-------|----------|
| Kat No | İndeks |
| İsim | Kat adı |
| Kot (m) | Döşeme kotu |
| Kat Yük. (m) | Düzenlenebilir — çift tıkla |
| Node Sayısı | Bu kattaki node adedi |
| Durum | OK / Boş / HATA: Kot çakışıyor |

- **Kırmızı satır** → iki kat aynı kota sahip (çakışma)
- **Sarı satır** → katta hiç node yok
- **Tamam** → değişiklikler FloorManager'a uygulanır

---

## 23. PDF Pafta Düzeni

Çizimi A3/A4 kağıt düzeninde, başlık bloğuyla birlikte PDF veya SVG olarak kaydedin:

```
Analiz → Pafta Duzenle ve Yazdir... (Ctrl+P)
Komut: PAFTA
```

1. **Kağıt Boyutu** seçin — A3 Yatay önerilir.
2. **Ölçek**: "Otomatik" bırakırsanız VKT tüm çizimi sayfaya sığdırır.
3. **Başlık Bloğu** alanlarını doldurun (Proje Adı otomatik gelir).
4. **PDF Kaydet** veya **SVG Kaydet** → `rapor/` klasörüne.

---

## 24. Tam Proje İş Akışı — Özet

```
1.  Yeni Proje...     (Ctrl+Shift+N) — ad, müşteri, norm seçimi
2.  Mimari Belirle... (Ctrl+M)       — kat tanımla, DXF yükle, ref noktası
3.  3D Hizalama Kontrolü (Ctrl+Shift+H) — kot doğrula
4.  Malzeme seç       Özellik paneli → PVC/PPR/Bakır...
5.  ST Cihazları      sağ panel → armatür yerleştir
6.  PIPE              soğuk su ana boru hatları
7.  SOURCE            şebeke girişi
8.  SICAK-SU          sıcak su boruları (kırmızı)
9.  SOFBEN/KAZAN      sıcak su kaynağı
10. BAGLA (Ctrl+B)   armatürleri hatta bağla (toplu seçim destekli)
11. HYDRAULICS        DN etiketleri görünür
12. HIDROFOR          pompa boyutlandır
13. NORM              gerekirse DIN 1988-300
14. PIS-SU            pis su borular
15. YER-SUZGECI/ROGAR drenaj bağlantısı
16. KOPYA-KAT         tekrar eden katlar
17. KOLON (Ctrl+Shift+K) dikey boru bağlantıları
18. YAGMUR            yağmur suyu boyutlandırma
19. KABUL (Ctrl+Enter) tesisatı kabul et — doğrulama + numaralandırma
20. RISER (Ctrl+R)    kolon şeması → PDF/SVG
21. DN-OVERRIDE       manuel düzeltme + XLS
22. BOM (Ctrl+K)      keşif listesi
23. Rapor Dışa Aktar  rapor/ klasörüne
```

---

## 25. Sıcak Su Modülü (SICAK-SU / SOFBEN / KAZAN)

VKT, soğuk su şebekesiyle paralel olarak ayrı sıcak su ağı çizmeyi destekler. Sıcak su boruları **kırmızı** renkte render edilir ve kolon şemasına `SK-` prefix ile dahil edilir.

### Sıcak Su Kaynağı Yerleştirme

```
Çizim → Sıcak Su Kaynağı Yerleştir
Komut: SOFBEN  veya  KAZAN
```

- Şofben veya kazan konumuna tıklayın.
- VKT kırmızı bir `HotSource` node ekler.
- Bir projede birden fazla sıcak su kaynağı olabilir (ör. daire başına şofben).

### Sıcak Su Borusu Çizme

```
Çizim → Sıcak Su Borusu
Komut: SICAK-SU
```

- `SICAK-SU` yazın → imleç kırmızı boru moduna geçer.
- İlk nokta: kaynak veya dağıtım borusu başlangıcı.
- İkinci nokta (ve devamı): sıcak su boru hattı zinciri.
- ESC ile soğuk su moduna geri dönülür.

### Renk Kodlaması

| Boru Tipi | Renk | Kolon Etiketi |
|-----------|------|--------------|
| Soğuk su yatay | Açık mavi | — |
| Soğuk su kolon | Camgöbeği (cyan) | `[K]` prefix |
| Sıcak su yatay | Kırmızı | — |
| Sıcak su kolon | Turuncu | `[K]` prefix |
| Pis su | Kahverengi | — |

### Hidrolik Hesap

- Sıcak su boruları TS EN 806-3 **aynı formülle** hesaplanır (LU tabanlı debi, Darcy-Weisbach).
- DN etiketleri soğuk suyla aynı şekilde overlay'de gösterilir.
- `SK-001`, `SK-002` ... numaralandırması **Tesisatı Kabul Et** adımında yapılır.

---

## 26. Tesisatı Kabul Et (KABUL)

Proje tamamlandığında boru sistemi bir doğrulama ve numaralandırma adımından geçirilir.

```
Analiz → Tesisatı Kabul Et
Kısayol: Ctrl+Enter
Komut: KABUL  veya  ACCEPT
```

### Yapılan Kontroller

| Kontrol | Açıklama |
|---------|----------|
| Kaynak varlığı | En az bir `Source` veya `HotSource` node olmalı |
| Açık uç | Bağlantısı olmayan node'lar tespit edilir (kırmızı halka overlay'de zaten gösterilir) |
| Hata / Uyarı | Hatalar varsa düzeltme önerileriyle listelenir |

### Numaralandırma

Doğrulama geçilirse tüm borular otomatik numaralandırılır:

| Prefix | Boru Tipi |
|--------|-----------|
| `P-001`, `P-002`... | Soğuk su temiz su boruları |
| `SK-001`, `SK-002`... | Sıcak su boruları |
| `PS-001`, `PS-002`... | Pis su (drenaj) boruları |

### Kabul Sonrası

- `RunAutoHydro` tetiklenir — tüm DN boyutları güncellenir.
- Özet iletişim kutusu: kaç boru kabul edildi, kaç uyarı var.
- Raporlar bu numaralandırmayı kullanır (Hesap Föyü, BOM, Riser).

**İş akışı:**
1. Tüm borular çizildikten ve BAGLA tamamlandıktan sonra `KABUL` çalıştırın.
2. Hata listesini inceleyin; eksik bağlantıları tamamlayın.
3. Tekrar `KABUL` — sorun kalmadığında numaralandırma uygulanır.
4. Ardından `RISER`, `BOM` ve `DN-OVERRIDE` yapın.

---

## Bölüm 27 — Pis Su Tesisat Tasarımı: Modül ve Katman Yönetimi

### Pis Su Modülüne Geçiş ve Katman Kontrolü

VKT'de birden fazla sistem katmanı (Temiz Su / Sıcak Su / Pis Su) aynı projede bulunur.
Çizimi sadeleştirmek için **Görünüm → Katman Görünürlüğü** (Ctrl+Shift+L) kullanın:

| Eylem | Sonuç |
|-------|-------|
| Sadece Pis Su açık | Ekranda yalnızca kahverengi drenaj boruları ve Drain node'ları görünür |
| Temiz Su + Pis Su açık | Tüm sistemi birlikte görerek tesisat bağlantılarını doğrulayabilirsiniz |
| Sadece Temiz Su açık | Sıcak + soğuk su hatları, Fixture ve Source node'ları |

**Komut:** `KATMAN` veya `KATMAN-VIS`

### İş Akışı

1. `Görünüm → Katman Görünürlüğü` açın.
2. İncelemek istediğiniz sistemi seçin (veya hepsini açık bırakın).
3. Çizim sırasında diğer katmanları geçici kapatarak çalışın.
4. Pafta almadan önce tüm katmanları açık bırakın.

---

## Bölüm 28 — Pis Su Tesisat Tasarımı: Cihaz Yerleştirme ve Akıllı Bağlantı Noktaları

### FineSANI'deki "Akıllı Bağlantı Noktaları" Eşdeğeri

Mimari planda banyo/mutfak cihazları (WC, lavabo, küvet) zaten çizilmişse, bu cihazları tekrar
sembol olarak eklemek görsel kargaşa yaratır. FineSANI'nin "akıllı bağlantı noktaları" özelliği
bu sorunu çözer — VKT'de iki yöntemle erişilir:

#### Yöntem 1: ST Cihazları Paneli (Önerilen)

1. Sağ panelde **ST Cihazları** sekmesini açın.
2. Alt kısımdaki **"Akıllı Bağlantı Noktası"** checkbox'ını işaretleyin (mor renk).
3. Listeden cihaz tipini seçin (WC, Lavabo, Duş vb.).
4. Cihazı çift tıklayın → yerleştirme moduna girin.
5. Cihazın pis su bağlantı çıkışına tıklayın → ekrana yalnızca **magenta artı/yıldız sembolü** gelir.

#### Yöntem 2: Menü veya Komut

- `Çizim → Akıllı Bağlantı Noktası` menü komutu
- CommandBar: `AKILLI` veya `AKILLI-BAGLANTI` veya `SMART-POINT`

#### Ne Zaman Hangisi?

| Durum | Tercih |
|-------|--------|
| Mimari planda cihaz zaten çizili | **Akıllı Bağlantı Noktası** — sadece sembol |
| Yer süzgeci / mimaride çizimi olmayan eleman | **Normal Fixture / Yer Süzgeci** — tam sembol |
| Rögar, boşaltma çukuru | `ROGAR` komutu — Drain node yerleştirir |

#### Overlay Gösterimi

Akıllı Bağlantı Noktası (SmartPoint) node'ları:
- GPU renderda: magenta renkli **× (çarpı + artı)** sembolü
- Overlay label: `+ Baglanti` (mor renk)
- LU değeri: 0 (hesaba dahil edilmez — sadece bağlantı geometrisi)

---

## Bölüm 29 — Pis Su Tesisat Tasarımı: Boşaltma Noktası ve Kolon Bağlantıları

### Ana Tahliye Noktasını İşaretleme

Zemin katta tüm kolon boruları tek bir noktada birleştirilerek kanalizasyona bağlanır.
Bu noktayı sisteme tanıtmak için:

1. Rögar veya yer süzgeci node'u zaten eklenmiş olmalı (`ROGAR` komutu).
2. `Çizim → Ana Tahliye Noktasını İşaretle` veya `BOSALTMA` komutunu çalıştırın.
3. VKT, tüm Drain node'ları arasında **en düşük Z'ye** (zemin kotuna) sahip olanı otomatik seçer ve **"ANA TAHLİYE"** etiketi ile turuncu renkte vurgular.

**Komut:** `BOSALTMA` veya `ANA-TAHLIYE`

### Pis Su Kolon Boruları

Katlar arası pis su kolonları için aynı yöntemi kullanın:

1. `KOLON` komutunu çalıştırın.
2. Alt kattaki pis su node'unu seçin.
3. Hedef katı seçin → dikey boru otomatik oluşturulur.
4. Snap olarak **Perpendicular** (dik nokta) kullanın — kolon yatay boruya dik birleşir.

**Not:** Kolon tespit algoritması (`IsColumnEdge`): aynı XY (<50mm), Z farkı >0.3m olan borular
otomatik kolon olarak tanınır ve izometrik görünümde cyan/turuncu renk ile ayrıştırılır.

### Kademeli Kolon Durumu

Alt ve üst katlarda kolon farklı XY'de ise (örn. dış cephe + iç duvar):

1. **Alt kolon:** 0–3m → `KOLON` ile çizin.
2. **Üst kolon:** 3–6m → `KOLON` ile ayrı çizin.
3. İki kolonu `BORU` (normal boru komutu) + Perpendicular snap ile yatayda birleştirin.
4. İzometrik görünümde (`VIEW-ISO`) 3D bağlantıyı doğrulayın.

### Tesisat Kopyalama — Kolonları Dışarıda Bırakma

`Çizim → Tesisat Kopyala` (`KOPYA-KAT`) ile kat kopyalanırken kolonlar **otomatik hariç tutulur**.
Bu kural hem temiz su hem pis su sistemi için geçerlidir — yeniden bağlamak gerekmez.

### Komut Özeti — Pis Su İş Akışı

| Adım | Komut / Menü | Açıklama |
|------|-------------|----------|
| 1 | `KATMAN` | Pis Su katmanını görünür yap |
| 2 | `PIS-SU` | Drenaj borusu çiz modu |
| 3 | `AKILLI` veya ST panel checkbox | SmartPoint bağlantı noktası yerleştir |
| 4 | `YER-SUZGECI` | Yer süzgeci (Drain node) |
| 5 | `ROGAR` | Rögar / boşaltma noktası (Drain node) |
| 6 | `KOLON` | Dikey pis su kolon borusu |
| 7 | `KOPYA-KAT` | Aynı plan katı kopyala (kolonlar hariç) |
| 8 | `BOSALTMA` | Ana tahliye noktasını işaretle |
| 9 | `YAGMUR` | Yağmur suyu boyutlandırma (EN 12056-3) |
| 10 | `KABUL` | Tesisatı doğrula + numaralandır |

---

## Bölüm 30 — Pis Su Tesisat Hesapları

### 30.1 Pis Su Hesap Föyü

Drenaj devrelerinin tüm hesap sonuçlarını tablolu olarak görmek için:

**Menü:** `Analiz → Pis Su Hesap Foyu` veya komut: `PIS-HESAP`

Tablo sütunları:
| Sütun | Açıklama |
|-------|---------|
| Boru No | Etiket veya otomatik ID |
| Malzeme | PVC / PP / PE / Beton |
| DN (mm) | Manning ile hesaplanan çap |
| L (m) | Boru uzunluğu |
| DU | Kümülatif Deşarj Birimi |
| Q (L/s) | EN 12056-2: Q = K × √ΣDU |
| Eğim i (%) | Boru eğimi (varsayılan %2) |
| Doluluk h/d (%) | Gerçek doluluk oranı |

**Renk uyarısı:**
- **Kırmızı:** h/d > %50 — EN 12056 sınırı aşılmış, çap büyütülmeli
- **Sarı:** h/d %40–50 — dikkat bölgesi

### 30.2 Doluluk Derecesi (h/d) ve Eğim Etiketleri

Boru üzerinde doluluk ve eğim değerlerini overlay'de göstermek için:

**Menü:** `Analiz → Baskı İçeriği` veya komut: `BASKI`

"Pis Su ek etiketleri" bölümünden seçin:
- **Eğim i (%)** — Pis su borularında eğim oranı
- **Doluluk h/d (%)** — Manning ile hesaplanan gerçek doluluk

Örnek overlay etiketi: `DN110  i=2.0%  h/d=38%`

### 30.3 Çizimi Güncelle

Hesap sonuçlarını (çap, uzunluk, eğim, doluluk) çizime işlemek için:

**Menü:** `Analiz → Cizimi Guncelle` veya komut: `GUNCELLE` (Ctrl+Shift+U)

Dialog'da güncellenecek değerleri seçin:
- Boru çapı (DN / mm)
- Boru boyu (L)
- Eğim i (%) — Pis Su
- Doluluk h/d (%) — Pis Su

"Tamam" sonrası AutoHydro yeniden çalışır, seçilen etiket overlay'de görünür hale gelir.

**FineSANI karşılığı:** "Hesap Föyü → Çizimden Güncelle" + "Çizimi Güncelle" birleşimi

### 30.4 Kapalı Çukur / Foseptik Hesabı

Kanalizasyon bağlantısı olmayan projeler için TS 822 ve EN 12566-1 uyumlu hacim hesabı:

**Menü:** `Analiz → Kapali Cukur / Foseptik Hesabi` veya komut: `FOSEPTIK`

Giriş parametreleri:
| Parametre | Varsayılan | Açıklama |
|-----------|-----------|---------|
| Kişi sayısı | 10 | Bina toplam nüfus yükü |
| Günlük tüketim | 0.15 m³/kişi/gün | TS 822: konut için 150 L/kişi/gün |
| Bekleme süresi | 3 gün | Arıtma/çökelme süresi |
| Çift odalı | seçimsiz | EN 12566-1 Md. 5.3: 1. oda %67 / 2. oda %33 |

**Hesap formülü (TS 822):**
```
V_bekleme = kişi × günlük × bekleme  (min 1.5 m³)
V_çamur   = V_bekleme × 0.30
V_TOPLAM  = V_bekleme + V_çamur
```

Sonuçlar anlık olarak güncellenir; tahliye sıklığı da otomatik hesaplanır.

### 30.5 Pis Su Hesap İş Akışı Özeti

| Adım | Komut | Açıklama |
|------|-------|---------|
| 1 | `DEVRE` | Eğim, boru cinsi, pürüzlülük ayarla |
| 2 | `PIS-HESAP` | Hesap föyü tablosunu kontrol et |
| 3 | `BASKI` | Doluluk + eğim etiketlerini aç |
| 4 | `GUNCELLE` | Çizimi hesap sonuçlarıyla güncelle |
| 5 | `FOSEPTIK` | Foseptik hacim hesabı (gerekirse) |
| 6 | `WORD` veya `EXPORT-REPORT` | Rapor çıktısı al |

---

---

## 31. İnteraktif Örnek — 3 Katlı Konut Projesi

Bu bölüm, VKT'de sıfırdan tamamlanmış bir 3 katlı konut tesisat projesinin adım adım yürütülmesini gösterir. Her adımda kullanılacak komut ve menü yolu belirtilmektedir. Kendi projenizde adımları sırayla izleyebilirsiniz.

### Proje Tanımı

| Alan | Değer |
|------|-------|
| Bina tipi | 3 katlı konut (Zemin + 1. Kat + 2. Kat) |
| Her katta | 2 daire — 1 banyo (Lavabo + WC + Duş) + 1 mutfak (Evye) |
| Toplam armatür | 8 adet/kat × 3 kat = 24 armatür |
| Su kaynağı | Zemin katta tek şebeke girişi |
| Sıcak su | Her katta 1 kombi (3 adet) — ayrı sıcak su devresi |
| Drenaj | Pis su + zemin kat sofben tahliyesi |
| Standart | EN 806-3 (temiz su) · EN 12056-2 (drenaj) |

---

### ADIM 1 — Yeni Proje Oluşturma

```
Dosya → Yeni Proje...   (Ctrl+Shift+N)
```

Proje oluşturma penceresinde doldurun:
- **Proje Adı:** `3Katli_Konut_Ornegi`
- **Bina Tipi:** Konut
- **Hesap Normu:** EN 806-3

"Tamam" → VKT `[KökDizin]/3Katli_Konut_Ornegi/` klasörünü oluşturur.

---

### ADIM 2 — Devre Parametrelerini Ayarlama

```
DEVRE   veya   Analiz → Devre Secenekleri   (Ctrl+Shift+D)
```

| Parametre | Değer |
|-----------|-------|
| Bina Tipi | Konut |
| Ana Boru Cinsi | PPR (polipropilen) |
| İkincil Boru Cinsi | PPR |
| Pürüzlülük | 0.0070 mm (PPR) |
| Maks. Su Hızı | 2.0 m/s |
| Hesap Normu | EN 806-3 |

"Tamam" — ayarlar tüm hesaplara uygulandı.

---

### ADIM 3 — Mimari Altlıkları Yükleme

```
Mimari → Mimari Belirle...   (Ctrl+M)
```

**Global Referans Noktası:** Yapının A-1 kolon köşesi → `X: 5000  Y: 3000`
(Tüm kat DXF dosyalarında bu nokta aynı koordinatta olmalıdır.)

**Kat tablosunu doldurun:**

| Kat No | Kot (m) | İsim | Dosya |
|--------|---------|------|-------|
| 1 | 0.00 | Zemin Kat | `mimari/zemin_kat.dxf` |
| 2 | 3.00 | 1. Kat | `mimari/kat_1.dxf` |
| 3 | 6.00 | 2. Kat | `mimari/kat_2.dxf` |

"Tamam" → her kat için DXFImportDialog açılır → import edin.

> **DXF yoksa:** Boş proje üzerinde çizim yapılabilir — kat tanımı yapılır ama DXF dosyası eklenmez. ADIM 3'ü atlayın.

**3D Hizalama Kontrolü:**

```
Mimari → 3D Hizalama Kontrolü...   (Ctrl+Shift+H)
```

Tablo açılır: 3 kat, 3 farklı kot (0.00 / 3.00 / 6.00 m), çakışma yok → yeşil durum.

---

### ADIM 4 — Şebeke Giriş Noktasını (Kaynak) Yerleştirme

Zemin katta su giriş noktasına sayaç armatür konumuna tıklayın:

```
SOURCE   veya   Çizim → Kaynak Ekle
```

- Zemin kat bodrum duvarına yakın bir noktayı tıklayın → mavi kaynak node eklenir.
- Overlay'de **"Kaynak"** etiketi görünür.

> **Kural:** Bir binada yalnızca **tek kaynak** olmalıdır. VKT bunu `KABUL` adımında denetler.

---

### ADIM 5 — Zemin Katta Armatür Yerleştirme

**Banyo 1 (Daire A):** Banyoya girin, ST Cihazları panelini kullanın.

```
ST Cihazları Paneli (sağ taraf) → çift tıkla → yerleştir
```

Her armatür için 2-tıklama: **1. tık = konum, 2. tık = yön.**

| Armatür | Komut | LU |
|---------|-------|-----|
| Lavabo | ST Panel → Lavabo | 1.0 |
| WC | ST Panel → WC | 2.0 |
| Duş | ST Panel → Duş | 1.5 |

**Mutfak 1 (Daire A):**

| Armatür | Komut | LU |
|---------|-------|-----|
| Evye | ST Panel → Evye | 1.5 |

**Aynısını Daire B için tekrarlayın** → zemin katta toplam 8 armatür.

**Sofben (Sıcak Su Kaynağı):**

```
SOFBEN   veya   Çizim → Sıcak Su Kaynağı Yerleştir
```

Kaynak tipi: **Kombi** seçin → kırmızı HotSource node eklenir.

---

### ADIM 6 — Zemin Katta Soğuk Su Boruları

```
PIPE   veya   Çizim → Temiz Su Borusu Çiz
```

**Ana hat:** Kaynaktan başlayarak tüm armatürlere dağıtım:

```
SOURCE → (yatay ana) → Daire A koridoru → T-kavşak → Lavabo
                                         → T-kavşak → WC
                                         → T-kavşak → Duş
                                         → T-kavşak → Evye
                   → (devam) → Daire B → aynı dağıtım
```

**Snap kullanımı:** Her armatür node'una yaklaşınca sarı kare (Endpoint snap) belirsin → tıklayın.

**Armatürleri Ana Hatta Bağlama:**

```
BAGLA   veya   Ctrl+B
```

1. Armatür node'larını toplu seçin (sağdan sola Crossing seçimi).
2. Ana boru hattını tıklayın → VKT her armatür için dal boru + T-kavşak otomatik ekler.

---

### ADIM 7 — Zemin Katta Sıcak Su Boruları

```
SICAK-SU   veya   Çizim → Sıcak Su Borusu
```

Boruların kırmızıya döndüğünü doğrulayın. Kombiden:

```
Kombi → (kırmızı yatay hat) → Lavabo (sıcak giriş)
                              → Duş   (sıcak giriş)
                              → Evye  (sıcak giriş)
```

> **WC'ye sıcak su bağlanmaz** — EN 806-3 standardı.

---

### ADIM 8 — Zemin Katı Üst Katlara Kopyalama

```
KOPYA-KAT   veya   Çizim → Tesisat Kopyala
```

- **Kaynak kat:** Zemin Kat (Z ≈ 0.00 m)
- **Hedef kat:** 1. Kat (Z = 3.00 m)

"Tamam" → Zemin kattaki tüm yatay borular ve armatürler Z=3.00'a kopyalanır. Kolonlar (dikey borular) **otomatik hariç tutulur.**

**Aynısını 2. Kat için tekrarlayın** (Hedef: Z = 6.00 m).

Şimdi 3 katta 24 armatür + 24 armatürün boru bağlantısı hazır.

---

### ADIM 9 — Kolon Boruları (Katlar Arası Dikey Borular)

```
KOLON   veya   Ctrl+Shift+K
```

Zemin kattaki ana boru hattının bir kavşağından:

1. **Kaynak node seçin** (zemin kattaki T-kavşak).
2. **Hedef kat:** 1. Kat.
3. VKT, aynı XY konumunda hedef katta node bulur; yoksa Junction oluşturur.
4. Z farkından boru uzunluğu otomatik = 3.00 m.

**Aynısını şu bağlantılar için tekrarlayın:**
- Zemin → 1. Kat (soğuk su ana hat)
- 1. Kat → 2. Kat (soğuk su ana hat)
- Zemin → 1. Kat (sıcak su hattı — kombi 1. katta da olacak)
- 1. Kat → 2. Kat (sıcak su hattı)

> **İpucu:** 1. ve 2. katta da birer Kombi (SOFBEN) ekleyin — her kat bağımsız sıcak su kaynağı.

---

### ADIM 10 — Gerçek Zamanlı Hidrolik Kontrolü

Boru eklendikçe VKT **600 ms sonra otomatik** hesap yapar (AutoHydro):

- Overlay'de DN etiketleri belirir: DN15, DN20, DN25 vb.
- Kaynak yakınındaki ana hat → büyük çap (DN32 veya DN40)
- Son armatüre giden dal → küçük çap (DN15)

**Açık uç uyarılarını kontrol edin:** Kırmızı halkalar varsa snap ile düzgün bağlantı yapılmamış — bağlanmamış noktayı düzeltin.

---

### ADIM 11 — Pis Su Sistemi

```
PIS-SU   veya   Çizim → Pis Su Borusu Çiz
```

Zemin katta:

```
WC  → (kahverengi DN110) → Rögar
Duş → (kahverengi DN50) → Rögar
Lavabo → (kahverengi DN40) → Rögar
Evye → (kahverengi DN50) → Rögar
```

```
ROGAR   veya   Çizim → Rögar
```

Banyonun zemin döşemesindeki rögar konumuna tıklayın.

**KOPYA-KAT ile pis su sistemi de kopyalayın** (1. ve 2. kat için).

**Dikey pis su kolonları:**

```
KOLON   → WC çıkışını bir alt kata bağla (DN110 kolon)
```

**Ana Tahliye Noktası:**

```
BOSALTMA   veya   Çizim → Ana Tahliye Noktasını İşaretle
```

VKT en alttaki Drain node'u turuncu `[ANA TAHLİYE]` etiketi ile işaretler.

---

### ADIM 12 — Tesisatı Kabul Ettirme

```
KABUL   veya   Ctrl+Enter
```

VKT kontrol listesi:
- ✓ En az 1 kaynak (Source) mevcut
- ✓ Tüm armatürler bağlı (kırmızı halka kalmadı)
- ✓ En az 1 boru

Hata yoksa otomatik numaralandırma:
- `P-001...` → Soğuk su boruları
- `SK-001...` → Sıcak su boruları
- `PS-001...` → Pis su boruları

---

### ADIM 13 — Hidrolik Analiz ve Doğrulama

**Tam analiz çalıştır:**

```
HYDRAULICS   veya   Analiz → Hidrolik Hesap Yap
```

**Basınç kaybı tablosu:**

```
BASINC   veya   Analiz → Parcalarin Basinc Kaybi
```

Kritik devre sarı vurguyla belirgin. EN 806-3'e göre son armatürde min 0.10 bar basınç kalmalı.

**Hidrofor gereksinimi:**

```
HIDROFOR   veya   Analiz → Hidrofor Boyutlandirma
```

Örnek çıktı (3 katlı konut, 24 armatür):
```
Toplam dinamik yük: 18.4 m
Şebeke bağlantı bası.: 4.0 bar
Kritik hat kayıp: 3.8 m
Bina yüksekliği: 9.5 m
Gerekli pompa yükü: 8.3 m → Pompa GEREKSİZ (şebeke basıncı yeterli)
```

**Pis su hesabı:**

```
PIS-HESAP   veya   Analiz → Pis Su Hesap Foyu
```

h/d < %50 olduğunu doğrulayın. Kırmızı satır yoksa boyutlandırma doğru.

---

### ADIM 14 — Norm Karşılaştırması (Opsiyonel)

EN 806-3 ile DIN 1988-300 arasındaki farkı görmek için:

```
NORM-KARSILASTIR   veya   Analiz → Norm Karsilastirma
```

Örnek: DIN 1988-300, simultanite faktörü nedeniyle bazı hatlarda DN15 yerine DN20 önerebilir. Sarı satırlar = DIN'in büyük önerdiği çaplar.

---

### ADIM 15 — Çıktı Üretme

**Kolon Şeması:**

```
RISER   (Ctrl+R)
```

→ SVG / PDF / DXF olarak export edin. DXF dosyası `rapor/` klasörüne kaydedilir.

**Keşif Listesi:**

```
BOM   (Ctrl+K)
```

DN gruplarına göre PPR boru metrajı + T-parça + dirsek sayımı.

**Hesap Föyü (XLS):**

```
DN-OVERRIDE   veya   Analiz → DN Manuel Override
```

→ "XLS Kaydet" ile Excel'e aktarın.

**Word Raporu:**

```
WORD   veya   Analiz → Word RTF Rapor
```

→ `rapor/3Katli_Konut_Ornegi_hesap_foyu.rtf` dosyasına yazar. Word veya WordPad açar.

**PDF Pafta:**

```
PAFTA   (Ctrl+P)
```

A3 yatay, ölçek 1:50 → ISO başlık bloğu otomatik doldurulur.

---

### Proje Özeti — 3 Katlı Konut

| Kalem | Değer |
|-------|-------|
| Toplam armatür | 24 adet (8/kat × 3 kat) |
| Soğuk su ana hat | DN32 → DN25 → DN20 → DN15 |
| Sıcak su hattı | DN20 → DN15 |
| Pis su kolon | DN110 (WC) + DN75 (genel) |
| Hesap normu | EN 806-3 |
| Şebeke basıncı | 4 bar → hidrofor GEREKMEZ |
| Riser PDF | A3 yatay, SVG + DXF |
| Rapor | .rtf (Word) + .xlsx (hesap föyü) |

### Sık Yapılan Hatalar ve Çözümleri

| Hata | Belirtisi | Çözüm |
|------|-----------|-------|
| Snap kullanılmadan boru bağlandı | Kırmızı açık uç halkası | ESC → boru sil → snap ile yeniden çiz |
| Kolon yanlış katta bitti | DN hesabı mantıksız | KOLON komutunu doğru kaynak/hedef kat ile tekrar çalıştır |
| Kaynak 2 kez eklendi | KABUL "çoklu kaynak" hatası | Extra SOURCE node sil |
| Armatür bağlanmadı | LU 0 görünüyor | BAGLA komutu ile ana hata bağla |
| h/d > %50 pis suda | PIS-HESAP kırmızı satır | Eğimi artır (DEVRE) veya DN büyüt (DN-OVERRIDE) |

---

*VKT v1.0 — © 2026 — TS EN 806-3 · EN 12056-2 · EN 12056-3 · DIN 1988-300 · TS 822 · EN 12566-1 uyumlu*
