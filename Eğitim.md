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

### Adım 3: W-Block / Blok Hazırlama (AutoCAD tarafında)
- FineSANI ile uyumlu proje dosyaları AutoCAD W-Block (WBLOCK) komutu ile tek dosyaya aktarılır.
- Önerilen katmanlar:
  - `DUVAR` — bina dış/iç duvarları
  - `KAT-PLANI` — kat planı ana çizgisi
  - `PENCERE`, `KAPI` — doğrama
- Gereksiz katmanlar dışarıda bırakılarak dosya boyutu küçültülür.

### Adım 4: Mimari Belirle — Kat Tanımları
**Menü: Mimari → Mimari Belirle...** (Ctrl+M)

Açılan pencerede:
| Alan | Açıklama |
|------|----------|
| Kat Numarası | 1'den başlayan sıralı numara |
| Kot (m) | Döşeme yüksekliği (bodrum = negatif, örn. -3.00) |
| İsim | "Bodrum Kat", "Zemin Kat", "1. Kat" ... |
| Dosya | İlgili katın DXF veya DWG dosyası |

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

## 7. Dosya Formatları

| Format | İçe Aktar | Dışa Aktar |
|--------|-----------|------------|
| `.vkt` | Dosya → Aç | Dosya → Kaydet |
| `.dxf` | Dosya → CAD Dosyası İçe Aktar | Analiz → Rapor Dışa Aktar |
| `.dwg` | Dosya → CAD Dosyası İçe Aktar | — |
| `.xls` | — | Analiz → Rapor Dışa Aktar |
| `.pdf` | — | Analiz → Rapor Dışa Aktar |

---

*VKT v1.0 — © 2026 — TS EN 806-3 + EN 12056-2 uyumlu*
