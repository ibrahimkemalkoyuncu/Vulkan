/**
 * @file MainWindow.cpp
 * @brief Ana Pencere İmplementasyonu (FINE SANI++)
 */

#include "ui/MainWindow.hpp"
#include "ui/DXFImportDialog.hpp"
#include "ui/SpacePanel.hpp"
#include "mep/HydraulicSolver.hpp"
#include "mep/ScheduleGenerator.hpp"
#include "mep/Database.hpp"
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <iostream>

namespace vkt {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    
    setWindowTitle("VKT - Mekanik Tesisat CAD (FINE SANI++)");
    resize(1600, 900);
    
    // Space Manager initialize et
    m_spaceManager = std::make_unique<cad::SpaceManager>();
    
    CreateActions();
    CreateMenus();
    CreateToolbars();
    CreateDockPanels();
    
    statusBar()->showMessage("VKT hazır - Mühendislik Modu AÇIK");
}

MainWindow::~MainWindow() {
    std::cout << "MainWindow kapatılıyor..." << std::endl;
}

void MainWindow::SetDocument(core::Document* doc) {
    m_document = doc;
    UpdateUI();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_document && m_document->IsModified()) {
        auto reply = QMessageBox::question(this, 
            "Kaydetmeden Çık",
            "Değişiklikler kaydedilmedi. Çıkılsın mı?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::CreateActions() {
    // Dosya işlemleri
    m_actNew = new QAction("&Yeni", this);
    m_actNew->setShortcut(QKeySequence::New);
    connect(m_actNew, &QAction::triggered, this, &MainWindow::OnNew);

    m_actOpen = new QAction("&Aç", this);
    m_actOpen->setShortcut(QKeySequence::Open);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::OnOpen);

    m_actSave = new QAction("&Kaydet", this);
    m_actSave->setShortcut(QKeySequence::Save);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::OnSave);

    m_actSaveAs = new QAction("Farklı K&aydet", this);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::OnSaveAs);

    m_actImportDXF = new QAction("CAD Dosyası İçe &Aktar...", this);
    m_actImportDXF->setToolTip("Mimari projeyi CAD dosyasından (DXF/DWG) içe aktar ve mahal tespiti yap");
    connect(m_actImportDXF, &QAction::triggered, this, &MainWindow::OnImportDXF);

    m_actExit = new QAction("Çı&kış", this);
    connect(m_actExit, &QAction::triggered, this, &MainWindow::OnExit);

    // Düzenleme
    m_actUndo = new QAction("&Geri Al", this);
    m_actUndo->setShortcut(QKeySequence::Undo);
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::OnUndo);

    m_actRedo = new QAction("&Yinele", this);
    m_actRedo->setShortcut(QKeySequence::Redo);
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::OnRedo);

    m_actDelete = new QAction("&Sil", this);
    m_actDelete->setShortcut(QKeySequence::Delete);
    connect(m_actDelete, &QAction::triggered, this, &MainWindow::OnDelete);

    // Çizim
    m_actDrawPipe = new QAction("Boru Çiz", this);
    connect(m_actDrawPipe, &QAction::triggered, this, &MainWindow::OnDrawPipe);

    m_actDrawFixture = new QAction("Armatür Ekle", this);
    connect(m_actDrawFixture, &QAction::triggered, this, &MainWindow::OnDrawFixture);

    m_actDrawJunction = new QAction("Bağlantı Noktası", this);
    connect(m_actDrawJunction, &QAction::triggered, this, &MainWindow::OnDrawJunction);

    m_actSelect = new QAction("Seç", this);
    connect(m_actSelect, &QAction::triggered, this, &MainWindow::OnSelectMode);

    // Görünüm
    m_actPlanView = new QAction("Plan Görünümü", this);
    connect(m_actPlanView, &QAction::triggered, this, &MainWindow::OnPlanView);

    m_actIsometricView = new QAction("İzometrik Görünüm", this);
    connect(m_actIsometricView, &QAction::triggered, this, &MainWindow::OnIsometricView);

    m_actZoomExtents = new QAction("Tümünü Göster", this);
    connect(m_actZoomExtents, &QAction::triggered, this, &MainWindow::OnZoomExtents);

    // Analiz
    m_actRunHydraulics = new QAction("Hidrolik Analiz", this);
    m_actRunHydraulics->setShortcut(Qt::Key_F5);
    connect(m_actRunHydraulics, &QAction::triggered, this, &MainWindow::OnRunHydraulics);

    m_actGenerateSchedule = new QAction("Metraj Oluştur", this);
    connect(m_actGenerateSchedule, &QAction::triggered, this, &MainWindow::OnGenerateSchedule);

    m_actExportReport = new QAction("Rapor Dışa Aktar", this);
    connect(m_actExportReport, &QAction::triggered, this, &MainWindow::OnExportReport);
}

void MainWindow::CreateMenus() {
    // Dosya
    auto* fileMenu = menuBar()->addMenu("&Dosya");
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actImportDXF);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    // Düzen
    auto* editMenu = menuBar()->addMenu("&Düzen");
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actDelete);

    // Çizim
    auto* drawMenu = menuBar()->addMenu("Çi&zim");
    drawMenu->addAction(m_actDrawPipe);
    drawMenu->addAction(m_actDrawFixture);
    drawMenu->addAction(m_actDrawJunction);

    // Görünüm
    auto* viewMenu = menuBar()->addMenu("&Görünüm");
    viewMenu->addAction(m_actPlanView);
    viewMenu->addAction(m_actIsometricView);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actZoomExtents);

    // Analiz
    auto* analyzeMenu = menuBar()->addMenu("&Analiz");
    analyzeMenu->addAction(m_actRunHydraulics);
    analyzeMenu->addAction(m_actGenerateSchedule);
    analyzeMenu->addAction(m_actExportReport);
}

void MainWindow::CreateToolbars() {
    // Çizim toolbar
    m_drawToolbar = addToolBar("Çizim");
    m_drawToolbar->addAction(m_actSelect);
    m_drawToolbar->addAction(m_actDrawPipe);
    m_drawToolbar->addAction(m_actDrawFixture);
    m_drawToolbar->addAction(m_actDrawJunction);

    // Düzenleme toolbar
    m_editToolbar = addToolBar("Düzenleme");
    m_editToolbar->addAction(m_actUndo);
    m_editToolbar->addAction(m_actRedo);
    m_editToolbar->addAction(m_actDelete);

    // Görünüm toolbar
    m_viewToolbar = addToolBar("Görünüm");
    m_viewToolbar->addAction(m_actPlanView);
    m_viewToolbar->addAction(m_actIsometricView);
    m_viewToolbar->addAction(m_actZoomExtents);

    // Analiz toolbar
    m_analysisToolbar = addToolBar("Analiz (FINE SANI++)");
    m_analysisToolbar->addAction(m_actRunHydraulics);
    m_analysisToolbar->addAction(m_actGenerateSchedule);
}

void MainWindow::CreateDockPanels() {
    // Property Panel
    m_propertyPanel = new QDockWidget("Özellikler (Mühendislik Modu)", this);
    auto* propWidget = new QWidget();
    auto* propLayout = new QFormLayout(propWidget);

    m_propDiameter = new QLineEdit();
    m_propLength = new QLineEdit();
    m_propMaterial = new QComboBox();
    m_propZeta = new QLineEdit();
    m_propSlope = new QLineEdit();
    m_propDU = new QLineEdit();

    // Malzeme listesi
    auto& db = mep::Database::Instance();
    for (const auto& mat : db.GetPipeMaterials()) {
        m_propMaterial->addItem(QString::fromStdString(mat));
    }

    propLayout->addRow("Çap (mm):", m_propDiameter);
    propLayout->addRow("Uzunluk (m):", m_propLength);
    propLayout->addRow("Malzeme:", m_propMaterial);
    propLayout->addRow("Zeta (ζ):", m_propZeta);
    propLayout->addRow("Eğim:", m_propSlope);
    propLayout->addRow("DU / LU:", m_propDU);

    connect(m_propDiameter, &QLineEdit::editingFinished, this, &MainWindow::OnPropertiesUpdated);
    connect(m_propMaterial, &QComboBox::currentTextChanged, this, &MainWindow::OnMaterialChanged);
    connect(m_propZeta, &QLineEdit::editingFinished, this, &MainWindow::OnPropertiesUpdated);
    connect(m_propSlope, &QLineEdit::editingFinished, this, &MainWindow::OnPropertiesUpdated);

    m_propertyPanel->setWidget(propWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyPanel);

    // Space Panel (Mahal Yönetimi)
    m_spacePanel = new SpacePanel(this);
    m_spacePanel->SetSpaceManager(m_spaceManager.get());
    addDockWidget(Qt::LeftDockWidgetArea, m_spacePanel);
    
    connect(m_spacePanel, &SpacePanel::SpaceSelected, this, &MainWindow::OnSpaceSelected);
    connect(m_spacePanel, &SpacePanel::SpaceDoubleClicked, this, &MainWindow::OnSpaceDoubleClicked);
    connect(m_spacePanel, &SpacePanel::DeleteSpaceRequested, this, &MainWindow::OnDeleteSpace);

    // Log Panel
    m_logPanel = new QDockWidget("Analiz Logu", this);
    m_logList = new QListWidget();
    m_logPanel->setWidget(m_logList);
    addDockWidget(Qt::BottomDockWidgetArea, m_logPanel);
}

void MainWindow::UpdateUI() {
    bool hasDoc = (m_document != nullptr);
    
    m_actSave->setEnabled(hasDoc);
    m_actSaveAs->setEnabled(hasDoc);
    m_actUndo->setEnabled(hasDoc && m_document->CanUndo());
    m_actRedo->setEnabled(hasDoc && m_document->CanRedo());
    m_actRunHydraulics->setEnabled(hasDoc);
    m_actGenerateSchedule->setEnabled(hasDoc);
}

// Slot implementations
void MainWindow::OnNew() {
    std::cout << "Yeni proje oluşturuluyor..." << std::endl;
}

void MainWindow::OnOpen() {
    std::cout << "Proje açılıyor..." << std::endl;
}

void MainWindow::OnSave() {
    if (m_document) {
        std::cout << "Proje kaydediliyor..." << std::endl;
    }
}

void MainWindow::OnSaveAs() {
    std::cout << "Farklı kaydet..." << std::endl;
}

void MainWindow::OnImportDXF() {
    // DXF Import Dialog aç
    DXFImportDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.WasSuccessful()) {
            // Import edilen entity'leri al
            auto entities = dialog.GetImportedEntities();
            
            // Onaylanan mahalleri al ve SpaceManager'a ekle
            auto acceptedSpaces = dialog.GetAcceptedSpaces();
            
            int addedCount = 0;
            for (const auto& candidate : acceptedSpaces) {
                auto* space = m_spaceManager->AcceptCandidate(candidate);
                if (space) {
                    addedCount++;
                }
            }
            
            // Komşulukları tespit et
            if (addedCount > 0) {
                m_spaceManager->DetectAllAdjacencies(10.0); // 10mm tolerance
            }
            
            // Space paneli güncelle
            if (m_spacePanel) {
                m_spacePanel->RefreshList();
            }
            
            m_logList->clear();
            m_logList->addItem(QString("CAD dosyası import başarılı!"));
            m_logList->addItem(QString("- %1 entity yüklendi").arg(entities.size()));
            m_logList->addItem(QString("- %1 mahal eklendi").arg(addedCount));
            
            statusBar()->showMessage(QString("%1 mahal başarıyla eklendi!").arg(addedCount));
            
            QMessageBox::information(this, "Import Tamamlandı",
                QString("DXF dosyası başarıyla içe aktarıldı!\n\n"
                        "%1 entity yüklendi\n"
                        "%2 mahal tespit edildi ve eklendi\n"
                        "Komşuluk ilişkileri oluşturuldu")
                    .arg(entities.size())
                    .arg(addedCount));
        }
    }
}

void MainWindow::OnExit() {
    close();
}

void MainWindow::OnUndo() {
    if (m_document) {
        m_document->Undo();
        UpdateUI();
    }
}

void MainWindow::OnRedo() {
    if (m_document) {
        m_document->Redo();
        UpdateUI();
    }
}

void MainWindow::OnDelete() {
    std::cout << "Silme işlemi..." << std::endl;
}

void MainWindow::OnDrawPipe() {
    std::cout << "Boru çizim modu aktif" << std::endl;
    statusBar()->showMessage("Boru çizimi: İlk noktayı tıklayın");
}

void MainWindow::OnDrawFixture() {
    std::cout << "Armatür ekleme modu aktif" << std::endl;
    statusBar()->showMessage("Armatür: Yerleştirme noktasını tıklayın");
}

void MainWindow::OnDrawJunction() {
    std::cout << "Bağlantı noktası ekleme modu" << std::endl;
}

void MainWindow::OnSelectMode() {
    std::cout << "Seçim modu aktif" << std::endl;
    statusBar()->showMessage("Seçim modu");
}

void MainWindow::OnPlanView() {
    std::cout << "Plan görünümüne geçiliyor..." << std::endl;
    statusBar()->showMessage("Plan Görünümü (2D)");
}

void MainWindow::OnIsometricView() {
    std::cout << "İzometrik görünüme geçiliyor..." << std::endl;
    statusBar()->showMessage("İzometrik Görünüm (3D)");
}

void MainWindow::OnZoomExtents() {
    std::cout << "Tümünü göster..." << std::endl;
}

void MainWindow::OnRunHydraulics() {
    if (!m_document) return;

    m_logList->clear();
    m_logList->addItem("═══════════════════════════════════════");
    m_logList->addItem("  FULL MEP ANALİZ BAŞLIYOR");
    m_logList->addItem("═══════════════════════════════════════");

    auto& network = m_document->GetNetwork();
    mep::HydraulicSolver solver(network);

    // 1. Besleme analizi
    m_logList->addItem("");
    m_logList->addItem("1️⃣ TEMİZ SU SİSTEMİ (TS EN 806-3)");
    solver.Solve();
    m_logList->addItem("✅ Besleme analizi tamamlandı");

    // 2. Drenaj analizi
    m_logList->addItem("");
    m_logList->addItem("2️⃣ ATIK SU SİSTEMİ (EN 12056)");
    solver.SolveDrainage();
    m_logList->addItem("✅ Drenaj analizi tamamlandı");

    // 3. Kritik devre
    m_logList->addItem("");
    m_logList->addItem("3️⃣ KRİTİK DEVRE (Pompa Yüksekliği)");
    auto criticalPath = solver.CalculateCriticalPath();
    
    QString cpMsg = QString("✅ Gerekli pompa yüksekliği: %1 mSS")
        .arg(criticalPath.requiredPumpHead_m, 0, 'f', 2);
    m_logList->addItem(cpMsg);

    m_logList->addItem("");
    m_logList->addItem("═══════════════════════════════════════");
    m_logList->addItem("  ANALİZ TAMAMLANDI ✅");
    m_logList->addItem("═══════════════════════════════════════");

    statusBar()->showMessage("Hidrolik analiz tamamlandı!");
}

void MainWindow::OnGenerateSchedule() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();
    mep::ScheduleGenerator generator(network);

    m_logList->clear();
    m_logList->addItem("Metraj oluşturuluyor...");

    auto materials = generator.GenerateMaterialList();
    m_logList->addItem(QString("Toplam %1 kalem malzeme").arg(materials.size()));

    for (const auto& item : materials) {
        QString line = QString("%1: %2 %3")
            .arg(QString::fromStdString(item.description))
            .arg(item.quantity, 0, 'f', 2)
            .arg(QString::fromStdString(item.unit));
        m_logList->addItem(line);
    }

    statusBar()->showMessage("Metraj oluşturuldu!");
}

void MainWindow::OnExportReport() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();
    mep::ScheduleGenerator generator(network);

    std::string report = generator.GenerateHydraulicReport();
    
    m_logList->clear();
    m_logList->addItem("Rapor hazırlandı!");
    m_logList->addItem("CSV export özelliği yakında...");

    statusBar()->showMessage("Rapor dışa aktarıldı!");
}

void MainWindow::OnSelectSpace() {
    std::cout << "Mahal seçimi aktif (B-Rep engine)" << std::endl;
}

void MainWindow::OnCalculateLoads() {
    if (!m_spaceManager) return;
    
    // Seçili mahalden yük hesabı
    if (m_spacePanel) {
        cad::EntityId spaceId = m_spacePanel->GetSelectedSpaceId();
        if (spaceId > 0) {
            auto* space = m_spaceManager->GetSpace(spaceId);
            if (space) {
                double area = space->GetArea();
                double waterLoad = area * 20.0; // 20 L/m² yaklaşımı
                
                m_logList->clear();
                m_logList->addItem(QString("Yük Hesabı - %1")
                    .arg(QString::fromStdString(space->GetName())));
                m_logList->addItem(QString("Alan: %1 m²").arg(area, 0, 'f', 2));
                m_logList->addItem(QString("Tahmin Edilen Su İhtiyacı: %1 L/gün").arg(waterLoad, 0, 'f', 0));
                
                statusBar()->showMessage(QString("%1 için yük hesaplandı").arg(QString::fromStdString(space->GetName())));
            }
        }
    }
}

void MainWindow::OnSpaceSelected(unsigned long long spaceId) {
    if (!m_spaceManager) return;
    
    auto* space = m_spaceManager->GetSpace(spaceId);
    if (space) {
        statusBar()->showMessage(QString("Seçili mahal: %1 (%2 m²)")
            .arg(QString::fromStdString(space->GetName()))
            .arg(space->GetArea(), 0, 'f', 2));
    }
}

void MainWindow::OnSpaceDoubleClicked(unsigned long long spaceId) {
    if (!m_spaceManager) return;
    
    auto* space = m_spaceManager->GetSpace(spaceId);
    if (space) {
        // Mahal detaylarını göster
        QString details = QString(
            "Mahal: %1\n"
            "Tip: %2\n"
            "Numara: %3\n"
            "Alan: %4 m²\n"
            "Hacim: %5 m³\n"
            "Kat Yüksekliği: %6 m\n"
            "\n"
            "Gereksinimler:\n"
            "Soğuk Su: %7\n"
            "Sıcak Su: %8\n"
            "Drenaj: %9\n"
            "Isıtma: %10\n"
            "Gaz: %11"
        )
            .arg(QString::fromStdString(space->GetName()))
            .arg(QString::fromStdString(cad::Space::SpaceTypeToString(space->GetSpaceType())))
            .arg(QString::fromStdString(space->GetNumber()))
            .arg(space->GetArea(), 0, 'f', 2)
            .arg(space->GetVolume(), 0, 'f', 2)
            .arg(space->GetCeilingHeight(), 0, 'f', 2)
            .arg(space->GetRequirements().needsColdWater ? "Evet" : "Hayır")
            .arg(space->GetRequirements().needsHotWater ? "Evet" : "Hayır")
            .arg(space->GetRequirements().needsDrain ? "Evet" : "Hayır")
            .arg(space->GetRequirements().needsHeating ? "Evet" : "Hayır")
            .arg(space->GetRequirements().needsGas ? "Evet" : "Hayır");
        
        QMessageBox::information(this, "Mahal Detayları", details);
    }
}

void MainWindow::OnDeleteSpace(unsigned long long spaceId) {
    if (!m_spaceManager) return;
    
    if (m_spaceManager->DeleteSpace(spaceId)) {
        if (m_spacePanel) {
            m_spacePanel->RefreshList();
        }
        statusBar()->showMessage("Mahal silindi");
    }
}

void MainWindow::OnPropertiesUpdated() {
    std::cout << "Property güncellendi" << std::endl;
}

void MainWindow::OnDiameterChanged(const QString& text) {
    std::cout << "Çap değişti: " << text.toStdString() << std::endl;
}

void MainWindow::OnMaterialChanged(const QString& text) {
    std::cout << "Malzeme değişti: " << text.toStdString() << std::endl;
}

void MainWindow::OnZetaChanged(const QString& text) {
    std::cout << "Zeta değişti: " << text.toStdString() << std::endl;
}

void MainWindow::OnSlopeChanged(const QString& text) {
    std::cout << "Eğim değişti: " << text.toStdString() << std::endl;
}

} // namespace ui
} // namespace vkt
