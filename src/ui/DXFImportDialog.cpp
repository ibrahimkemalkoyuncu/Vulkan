/**
 * @file DXFImportDialog.cpp
 * @brief DXF Import Dialog implementasyonu
 */

#include "ui/DXFImportDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QFormLayout>
#include <QSplitter>

namespace vkt {
namespace ui {

DXFImportDialog::DXFImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("CAD Dosya İçe Aktarma (DXF/DWG)");
    resize(800, 600);
    
    SetupUI();
}

void DXFImportDialog::SetupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Tab widget (2 sayfa)
    auto* tabWidget = new QTabWidget(this);
    
    // ==================== SAYFA 1: DXF IMPORT ====================
    auto* importPage = new QWidget();
    auto* importLayout = new QVBoxLayout(importPage);
    
    // Dosya seçimi
    auto* fileGroup = new QGroupBox("CAD Dosyası (DXF/DWG)");
    auto* fileLayout = new QHBoxLayout(fileGroup);
    
    m_filePathEdit = new QLineEdit();
    m_filePathEdit->setPlaceholderText("DXF veya DWG dosya yolu...");
    m_filePathEdit->setReadOnly(true);
    
    m_browseButton = new QPushButton("Gözat...");
    connect(m_browseButton, &QPushButton::clicked, this, &DXFImportDialog::OnBrowseFile);
    
    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(m_browseButton);
    
    importLayout->addWidget(fileGroup);
    
    // Import ayarları
    auto* settingsGroup = new QGroupBox("Import Ayarları");
    auto* settingsLayout = new QFormLayout(settingsGroup);
    
    // Layer filtresi
    m_layerFilterEdit = new QLineEdit("DUVAR, WALL, Mimari-Duvar, A-WALL");
    m_layerFilterEdit->setToolTip("Virgülle ayrılmış layer isimleri");
    settingsLayout->addRow("Duvar Layer'ları:", m_layerFilterEdit);
    
    importLayout->addWidget(settingsGroup);
    
    // Import butonu
    m_importButton = new QPushButton("CAD Dosyasını İçe Aktar");
    m_importButton->setEnabled(false);
    connect(m_importButton, &QPushButton::clicked, this, &DXFImportDialog::OnImport);
    importLayout->addWidget(m_importButton);
    
    // Durum bilgisi
    m_statusLabel = new QLabel("Dosya seçilmedi");
    m_statusLabel->setStyleSheet("QLabel { color: gray; padding: 10px; }");
    importLayout->addWidget(m_statusLabel);
    
    // İstatistikler
    auto* statsGroup = new QGroupBox("Import İstatistikleri");
    auto* statsLayout = new QFormLayout(statsGroup);
    
    m_entityCountLabel = new QLabel("0");
    m_layerCountLabel = new QLabel("0");
    m_readTimeLabel = new QLabel("0 ms");
    
    statsLayout->addRow("Entity Sayısı:", m_entityCountLabel);
    statsLayout->addRow("Layer Sayısı:", m_layerCountLabel);
    statsLayout->addRow("Okuma Süresi:", m_readTimeLabel);
    
    importLayout->addWidget(statsGroup);
    
    importLayout->addStretch();
    
    tabWidget->addTab(importPage, "1. DXF İmport");
    
    // ==================== SAYFA 2: MAHAL TESPİTİ ====================
    auto* detectionPage = new QWidget();
    auto* detectionLayout = new QVBoxLayout(detectionPage);
    
    // Tespit ayarları
    auto* detectionSettingsGroup = new QGroupBox("Otomatik Mahal Tespit Ayarları");
    auto* detectionSettingsLayout = new QFormLayout(detectionSettingsGroup);
    
    m_detectSpacesCheckbox = new QCheckBox();
    m_detectSpacesCheckbox->setChecked(true);
    detectionSettingsLayout->addRow("Mahal Tespiti:", m_detectSpacesCheckbox);
    
    m_minAreaSpinBox = new QDoubleSpinBox();
    m_minAreaSpinBox->setRange(0.5, 1000.0);
    m_minAreaSpinBox->setValue(2.0);
    m_minAreaSpinBox->setSuffix(" m²");
    detectionSettingsLayout->addRow("Minimum Alan:", m_minAreaSpinBox);
    
    m_maxAreaSpinBox = new QDoubleSpinBox();
    m_maxAreaSpinBox->setRange(1.0, 100000000.0); // Support mm² coordinates
    m_maxAreaSpinBox->setValue(1000.0);
    m_maxAreaSpinBox->setSuffix(" m²");
    detectionSettingsLayout->addRow("Maximum Alan:", m_maxAreaSpinBox);
    
    m_detectNamesCheckbox = new QCheckBox();
    m_detectNamesCheckbox->setChecked(true);
    m_detectNamesCheckbox->setToolTip("TEXT entity'lerden mahal isimlerini tespit et");
    detectionSettingsLayout->addRow("İsim Tespiti:", m_detectNamesCheckbox);
    
    m_autoInferTypesCheckbox = new QCheckBox();
    m_autoInferTypesCheckbox->setChecked(true);
    m_autoInferTypesCheckbox->setToolTip("İsimden otomatik mahal tipi belirle (Banyo, WC, vb.)");
    detectionSettingsLayout->addRow("Otomatik Tip:", m_autoInferTypesCheckbox);
    
    detectionLayout->addWidget(detectionSettingsGroup);
    
    // Tespit butonu
    m_detectButton = new QPushButton("Mahalleri Tespit Et");
    m_detectButton->setEnabled(false);
    connect(m_detectButton, &QPushButton::clicked, this, &DXFImportDialog::OnDetectSpaces);
    detectionLayout->addWidget(m_detectButton);
    
    // Tespit sonuçları
    auto* resultsGroup = new QGroupBox("Tespit Edilen Mahaller");
    auto* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_candidateCountLabel = new QLabel("0 mahal tespit edildi");
    resultsLayout->addWidget(m_candidateCountLabel);
    
    m_candidateListWidget = new QListWidget();
    m_candidateListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_candidateListWidget, &QListWidget::itemSelectionChanged, 
            this, &DXFImportDialog::OnCandidateSelectionChanged);
    resultsLayout->addWidget(m_candidateListWidget);
    
    // Butonlar
    auto* buttonLayout = new QHBoxLayout();
    
    m_acceptAllButton = new QPushButton("Tümünü Onayla");
    m_acceptAllButton->setEnabled(false);
    connect(m_acceptAllButton, &QPushButton::clicked, this, &DXFImportDialog::OnAcceptAll);
    
    m_rejectAllButton = new QPushButton("Tümünü Reddet");
    m_rejectAllButton->setEnabled(false);
    connect(m_rejectAllButton, &QPushButton::clicked, this, &DXFImportDialog::OnRejectAll);
    
    m_acceptSelectedButton = new QPushButton("Seçilenleri Onayla");
    m_acceptSelectedButton->setEnabled(false);
    connect(m_acceptSelectedButton, &QPushButton::clicked, this, &DXFImportDialog::OnAcceptSelected);
    
    buttonLayout->addWidget(m_acceptAllButton);
    buttonLayout->addWidget(m_rejectAllButton);
    buttonLayout->addWidget(m_acceptSelectedButton);
    buttonLayout->addStretch();
    
    resultsLayout->addLayout(buttonLayout);
    
    detectionLayout->addWidget(resultsGroup);
    
    tabWidget->addTab(detectionPage, "2. Mahal Tespiti");
    
    mainLayout->addWidget(tabWidget);
    
    // Dialog butonları
    auto* dialogButtons = new QHBoxLayout();
    
    auto* okButton = new QPushButton("Tamam");
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    
    auto* cancelButton = new QPushButton("İptal");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    dialogButtons->addStretch();
    dialogButtons->addWidget(okButton);
    dialogButtons->addWidget(cancelButton);
    
    mainLayout->addLayout(dialogButtons);
}

void DXFImportDialog::OnBrowseFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Mimari Proje CAD Dosyası Seç",
        "",
#ifdef HAVE_LIBREDWG
        "CAD Files (*.dxf *.dwg);;DXF Files (*.dxf);;DWG Files (*.dwg);;All Files (*.*)"
#else
        "DXF Files (*.dxf);;All Files (*.*)"
#endif
    );
    
    if (!fileName.isEmpty()) {
        m_filePathEdit->setText(fileName);
        m_importButton->setEnabled(true);
        
        // Dosya tipini tespit et
#ifdef HAVE_LIBREDWG
        if (fileName.toLower().endsWith(".dwg")) {
            m_fileType = FileType::DWG;
            m_statusLabel->setText("DWG dosyası seçildi, import için hazır");
        } else
#endif
        if (fileName.toLower().endsWith(".dxf")) {
            m_fileType = FileType::DXF;
            m_statusLabel->setText("DXF dosyası seçildi, import için hazır");
        } else {
            m_fileType = FileType::Unknown;
            m_statusLabel->setText("Dosya seçildi, import için hazır");
        }
        
        m_statusLabel->setStyleSheet("QLabel { color: blue; padding: 10px; }");
    }
}

void DXFImportDialog::OnImport() {
    QString filePath = m_filePathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Uyarı", "Lütfen bir CAD dosyası seçin!");
        return;
    }
    
    m_statusLabel->setText("İçe aktarılıyor...");
    m_importButton->setEnabled(false);
    
    bool readSuccess = false;
    std::string errorMsg;
    size_t entityCount = 0;
    size_t layerCount = 0;
    double readTime = 0.0;
    
    // Dosya tipine göre uygun reader kullan
    if (m_fileType == FileType::DXF) {
        m_dxfReader = std::make_unique<cad::DXFReader>(filePath.toStdString());
        if (m_dxfReader->Read()) {
            readSuccess = true;
            const auto& stats = m_dxfReader->GetStatistics();
            entityCount = stats.totalEntities;
            layerCount = stats.totalLayers;
            readTime = stats.readTimeMs;
        } else {
            errorMsg = m_dxfReader->GetLastError();
        }
    }
#ifdef HAVE_LIBREDWG
    else if (m_fileType == FileType::DWG) {
        m_dwgReader = std::make_unique<cad::DWGReader>();
        if (m_dwgReader->Read(filePath.toStdString())) {
            readSuccess = true;
            const auto& stats = m_dwgReader->GetStatistics();
            entityCount = stats.entityCount;
            layerCount = stats.layerCount;
            readTime = stats.readTimeMs;

            // Bulunamayan xref dosyaları → kullanıcıya bilgi ver + yeniden arama seçeneği
            const auto& missing = m_dwgReader->GetMissingXrefs();
            if (!missing.empty()) {
                QString msg = QString("%1 xref dosyası bulunamadı:\n\n").arg(missing.size());
                for (const auto& p : missing)
                    msg += "  • " + QString::fromStdString(p) + "\n";
                msg += "\nXref dizinini belirtmek ister misiniz?";

                auto btn = QMessageBox::question(this, "Xref Bulunamadı", msg,
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                if (btn == QMessageBox::Yes) {
                    QString dir = QFileDialog::getExistingDirectory(
                        this, "Xref Dosyalarının Bulunduğu Dizini Seçin",
                        QFileInfo(filePath).dir().absolutePath());

                    if (!dir.isEmpty()) {
                        // Arama dizini ekle ve yeniden oku
                        m_dwgReader = std::make_unique<cad::DWGReader>();
                        m_dwgReader->AddXrefSearchPath(dir.toStdString());
                        if (m_dwgReader->Read(filePath.toStdString())) {
                            const auto& stats2 = m_dwgReader->GetStatistics();
                            entityCount = stats2.entityCount;
                            layerCount  = stats2.layerCount;
                            readTime    = stats2.readTimeMs;
                            // Hâlâ eksik xref varsa sessizce devam et
                        }
                    }
                }
            }
        } else {
            errorMsg = m_dwgReader->GetError();
        }
    }
#endif
    else {
        QMessageBox::critical(this, "Hata", "Bilinmeyen dosya formatı!");
        m_statusLabel->setText("Import başarısız!");
        m_statusLabel->setStyleSheet("QLabel { color: red; padding: 10px; }");
        m_importButton->setEnabled(true);
        return;
    }
    
    if (!readSuccess) {
        QMessageBox::critical(this, "Hata", 
            QString::fromStdString("CAD okuma hatası: " + errorMsg));
        m_statusLabel->setText("Import başarısız!");
        m_statusLabel->setStyleSheet("QLabel { color: red; padding: 10px; }");
        m_importButton->setEnabled(true);
        return;
    }
    
    m_importSuccess = true;
    
    // DWG dosyaları için layer filtresini temizle (tüm layer'ları dahil et)
    if (m_fileType == FileType::DWG) {
        m_layerFilterEdit->clear();
        m_layerFilterEdit->setPlaceholderText("Tüm layer'lar dahil edildi");
        
        // DWG coordinates are usually in mm, auto-adjust area limits
        m_minAreaSpinBox->setValue(0.01); // 0.01 mm² ~ very small
        m_maxAreaSpinBox->setValue(100000000.0); // 100M mm² = 100 m²
    }
    
    // İstatistikleri güncelle
    m_entityCountLabel->setText(QString::number(entityCount));
    m_layerCountLabel->setText(QString::number(layerCount));
    m_readTimeLabel->setText(QString::number(readTime, 'f', 2) + " ms");
    
    m_statusLabel->setText("Import başarılı! ✓");
    m_statusLabel->setStyleSheet("QLabel { color: green; padding: 10px; }");
    
    // Mahal tespit butonunu aktif et
    m_detectButton->setEnabled(true);
    
    QString fileTypeName = (m_fileType == FileType::DXF) ? "DXF" : "DWG";
    QMessageBox::information(this, "Başarılı", 
        QString("%1 dosyası başarıyla içe aktarıldı!\n\n"
                "%2 entity yüklendi\n"
                "%3 layer bulundu\n"
                "Okuma süresi: %4 ms")
            .arg(fileTypeName)
            .arg(entityCount)
            .arg(layerCount)
            .arg(readTime, 0, 'f', 2));
}

void DXFImportDialog::OnDetectSpaces() {
    if (!m_importSuccess) {
        QMessageBox::warning(this, "Uyarı", "Önce CAD dosyasını import edin!");
        return;
    }
    
    if (!m_detectSpacesCheckbox->isChecked()) {
        QMessageBox::information(this, "Bilgi", "Mahal tespiti devre dışı.");
        return;
    }
    
    // Detection ayarlarını hazırla
    cad::SpaceDetectionOptions options;
    
    // Layer filtresi
    QString layerFilter = m_layerFilterEdit->text();
    QStringList layers = layerFilter.split(",", Qt::SkipEmptyParts);
    options.wallLayerNames.clear();
    for (const QString& layer : layers) {
        options.wallLayerNames.push_back(layer.trimmed().toStdString());
    }
    
    options.minArea = m_minAreaSpinBox->value();
    options.maxArea = m_maxAreaSpinBox->value();
    options.detectNamesFromText = m_detectNamesCheckbox->isChecked();
    options.autoInferTypes = m_autoInferTypesCheckbox->isChecked();
    
    // SpaceManager ile mahal tespiti
    cad::SpaceManager spaceMgr;
    
    // Entity'leri al (DXF veya DWG)
    std::vector<cad::Entity*> entities = GetImportedEntities();
    if (entities.empty()) {
        QMessageBox::warning(this, "Uyarı", "Hiç entity bulunamadı!");
        return;
    }
    
    // Entity'lerden mahal tespit et
    if (m_fileType == FileType::DXF && m_dxfReader) {
        m_candidates = spaceMgr.DetectSpacesFromDXF(*m_dxfReader, options);
    }
#ifdef HAVE_LIBREDWG
    else if (m_fileType == FileType::DWG && m_dwgReader) {
        // DWG için de aynı logic (Entity'ler üzerinden)
        m_candidates = spaceMgr.DetectSpacesFromEntities(entities, options);
    }
#endif
    
    // Tüm adaylar başlangıçta onaylı
    m_candidateAccepted.resize(m_candidates.size(), true);
    
    UpdateCandidateList();
    
    if (m_candidates.empty()) {
        // Debug bilgisi: Kapalı polyline sayısı
        int closedPolyCount = 0;
        int totalPolyCount = 0;
        for (const auto* entity : entities) {
            if (entity->GetType() == cad::EntityType::Polyline) {
                totalPolyCount++;
                const cad::Polyline* poly = static_cast<const cad::Polyline*>(entity);
                if (poly->IsClosed()) closedPolyCount++;
            }
        }
        
        QString debugMsg = QString(
            "Hiç mahal tespit edilemedi.\n\n"
            "Debug Bilgisi:\n"
            "- Toplam entity: %1\n"
            "- Toplam polyline: %2\n"
            "- Kapalı polyline: %3\n"
            "- Layer filtresi: %4\n"
            "- Min alan: %5 m²\n"
            "- Max alan: %6 m²\n\n"
            "Olası nedenler:\n"
            "- Kapalı polyline bulunamadı\n"
            "- Layer filtreleri eşleşmedi\n"
            "- Alan kriterleri dışında")
            .arg(entities.size())
            .arg(totalPolyCount)
            .arg(closedPolyCount)
            .arg(options.wallLayerNames.empty() ? "Yok (tümü dahil)" : m_layerFilterEdit->text())
            .arg(options.minArea)
            .arg(options.maxArea);
            
        QMessageBox::information(this, "Sonuç", debugMsg);
    } else {
        QMessageBox::information(this, "Başarılı", 
            QString("%1 mahal tespit edildi!\n\n"
                    "Lütfen mahalleri inceleyin ve onaylayın.")
                .arg(m_candidates.size()));
    }
}

void DXFImportDialog::UpdateCandidateList() {
    m_candidateListWidget->clear();
    
    for (size_t i = 0; i < m_candidates.size(); ++i) {
        const auto& candidate = m_candidates[i];
        
        QString itemText = QString("%1. %2 (%3) - %.2f m²")
            .arg(i + 1)
            .arg(QString::fromStdString(candidate.suggestedName.empty() ? "İsimsiz Mahal" : candidate.suggestedName))
            .arg(QString::fromStdString(cad::Space::SpaceTypeToString(candidate.suggestedType)))
            .arg(candidate.area, 0, 'f', 2);
        
        if (!candidate.isValid) {
            itemText += " [GEÇERSİZ]";
        }
        
        if (!candidate.warnings.empty()) {
            itemText += " ⚠";
        }
        
        auto* item = new QListWidgetItem(itemText);
        
        // Onay durumuna göre renklendirme
        if (m_candidateAccepted[i]) {
            item->setBackground(QColor(200, 255, 200)); // Açık yeşil
            item->setCheckState(Qt::Checked);
        } else {
            item->setBackground(QColor(255, 200, 200)); // Açık kırmızı
            item->setCheckState(Qt::Unchecked);
        }
        
        // Geçersizse gri
        if (!candidate.isValid) {
            item->setForeground(QColor(128, 128, 128));
        }
        
        m_candidateListWidget->addItem(item);
    }
    
    m_candidateCountLabel->setText(QString("%1 mahal tespit edildi").arg(m_candidates.size()));
    
    bool hasItems = !m_candidates.empty();
    m_acceptAllButton->setEnabled(hasItems);
    m_rejectAllButton->setEnabled(hasItems);
    m_acceptSelectedButton->setEnabled(hasItems);
}

void DXFImportDialog::OnAcceptAll() {
    for (size_t i = 0; i < m_candidateAccepted.size(); ++i) {
        m_candidateAccepted[i] = true;
    }
    UpdateCandidateList();
}

void DXFImportDialog::OnRejectAll() {
    for (size_t i = 0; i < m_candidateAccepted.size(); ++i) {
        m_candidateAccepted[i] = false;
    }
    UpdateCandidateList();
}

void DXFImportDialog::OnAcceptSelected() {
    auto selectedItems = m_candidateListWidget->selectedItems();
    for (auto* item : selectedItems) {
        int index = m_candidateListWidget->row(item);
        if (index >= 0 && index < static_cast<int>(m_candidateAccepted.size())) {
            m_candidateAccepted[index] = !m_candidateAccepted[index]; // Toggle
        }
    }
    UpdateCandidateList();
}

void DXFImportDialog::OnCandidateSelectionChanged() {
    // Selection değiştiğinde detayları göster (ileride eklenebilir)
}

void DXFImportDialog::OnLayerFilterChanged() {
    // Layer filtresi değişirse yeniden tespit gerekebilir
}

std::vector<cad::Entity*> DXFImportDialog::GetImportedEntities() const {
    if (m_fileType == FileType::DXF && m_dxfReader) {
        return m_dxfReader->GetFilteredEntities();
    }
#ifdef HAVE_LIBREDWG
    else if (m_fileType == FileType::DWG && m_dwgReader) {
        return m_dwgReader->GetEntities();
    }
#endif
    return {};
}

std::vector<std::unique_ptr<cad::Entity>> DXFImportDialog::TakeEntities() {
    if (m_fileType == FileType::DXF && m_dxfReader) {
        return m_dxfReader->TakeEntities();
    }
#ifdef HAVE_LIBREDWG
    else if (m_fileType == FileType::DWG && m_dwgReader) {
        return m_dwgReader->TakeEntities();
    }
#endif
    return {};
}

std::vector<cad::SpaceCandidate> DXFImportDialog::GetAcceptedSpaces() const {
    std::vector<cad::SpaceCandidate> accepted;
    
    for (size_t i = 0; i < m_candidates.size(); ++i) {
        if (m_candidateAccepted[i] && m_candidates[i].isValid) {
            accepted.push_back(m_candidates[i]);
        }
    }
    
    return accepted;
}

/**
 * @brief DWG/DXF'ten okunan layer bilgilerini döndür
 */
std::unordered_map<std::string, cad::Layer> DXFImportDialog::GetLayers() const {
#ifdef HAVE_LIBREDWG
    if (m_fileType == FileType::DWG && m_dwgReader) {
        return m_dwgReader->GetLayers();
    }
#endif
    if (m_fileType == FileType::DXF && m_dxfReader) {
        // DXFReader std::map kullanıyor, unordered_map'e dönüştür
        const auto& layers = m_dxfReader->GetLayers();
        std::unordered_map<std::string, cad::Layer> result;
        for (const auto& pair : layers) {
            result[pair.first] = pair.second;
        }
        return result;
    }
    return {};
}

double DXFImportDialog::GetDXFLtscale() const {
    if (m_fileType == FileType::DXF && m_dxfReader)
        return m_dxfReader->GetHeader().ltscale;
    return 1.0;
}

} // namespace ui
} // namespace vkt
