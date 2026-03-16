/**
 * @file MainWindow.cpp
 * @brief Ana Pencere İmplementasyonu (FINE SANI++)
 */

#include "ui/MainWindow.hpp"
#include "ui/DXFImportDialog.hpp"
#include "ui/SpacePanel.hpp"
#include "render/VulkanWindow.hpp"
#include "mep/HydraulicSolver.hpp"
#include "mep/ScheduleGenerator.hpp"
#include "mep/Database.hpp"
#include "core/Persistence.hpp"
#include "core/Application.hpp"
#include <QMenuBar>
#include <fstream>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <iostream>
#include <fstream>

static void LogCAD(const std::string& msg) {
    static std::ofstream logFile("C:/Users/afney/Desktop/vkt_cad_import.log", std::ios::trunc);
    if (logFile.is_open()) {
        logFile << msg << std::endl;
        logFile.flush();
    }
    std::cout << msg << std::endl;
}

namespace vkt {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setWindowTitle("VKT - Mekanik Tesisat CAD (FINE SANI++)");
    resize(1600, 900);

    // Space Manager initialize et
    m_spaceManager = std::make_unique<cad::SpaceManager>();

    // Vulkan rendering penceresi oluştur
    m_vulkanWindow = new render::VulkanWindow();
    QWidget* vulkanWidget = QWidget::createWindowContainer(m_vulkanWindow, this);
    vulkanWidget->setMinimumSize(400, 300);
    setCentralWidget(vulkanWidget);

    // Mouse callback'lerini bagla
    m_vulkanWindow->SetMousePressCallback(
        [this](double wx, double wy, Qt::MouseButton btn) { HandleMousePress(wx, wy, btn); });
    m_vulkanWindow->SetMouseMoveCallback(
        [this](double wx, double wy) { HandleMouseMove(wx, wy); });

    CreateActions();
    CreateMenus();
    CreateToolbars();
    CreateDockPanels();

    statusBar()->showMessage("VKT hazir - Muhendislik Modu ACIK");
}

MainWindow::~MainWindow() {
    std::cout << "MainWindow kapatılıyor..." << std::endl;
}

void MainWindow::SetDocument(core::Document* doc) {
    m_document = doc;
    if (m_vulkanWindow && doc) {
        m_vulkanWindow->SetNetwork(&doc->GetNetwork());
    }
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
    if (m_document && m_document->IsModified()) {
        auto reply = QMessageBox::question(this, "Yeni Proje",
            "Mevcut proje kaydedilmedi. Devam edilsin mi?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    auto& app = core::Application::Instance();
    auto* doc = app.CreateNewDocument();
    SetDocument(doc);
    setWindowTitle("VKT - Mekanik Tesisat CAD (FINE SANI++) - Yeni Proje");
    statusBar()->showMessage("Yeni proje olusturuldu");
}

void MainWindow::OnOpen() {
    QString filePath = QFileDialog::getOpenFileName(this,
        "Proje Ac", "", "VKT Projesi (*.vkt);;Tum Dosyalar (*)");
    if (filePath.isEmpty()) return;

    auto& app = core::Application::Instance();
    auto* doc = app.OpenDocument(filePath.toStdString());
    if (doc) {
        SetDocument(doc);
        setWindowTitle(QString("VKT - FINE SANI++ - %1").arg(filePath));
        statusBar()->showMessage(QString("Proje acildi: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Hata", "Proje dosyasi acilamadi!");
    }
}

void MainWindow::OnSave() {
    if (!m_document) return;

    if (m_document->GetFilePath().empty()) {
        OnSaveAs();
        return;
    }

    if (m_document->Save(m_document->GetFilePath())) {
        statusBar()->showMessage("Proje kaydedildi");
    } else {
        QMessageBox::critical(this, "Hata", "Proje kaydedilemedi!");
    }
}

void MainWindow::OnSaveAs() {
    if (!m_document) return;

    QString filePath = QFileDialog::getSaveFileName(this,
        "Projeyi Kaydet", "", "VKT Projesi (*.vkt)");
    if (filePath.isEmpty()) return;

    if (m_document->Save(filePath.toStdString())) {
        setWindowTitle(QString("VKT - FINE SANI++ - %1").arg(filePath));
        statusBar()->showMessage(QString("Proje kaydedildi: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Hata", "Proje kaydedilemedi!");
    }
}

void MainWindow::OnImportDXF() {
    // DXF Import Dialog aç
    DXFImportDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.WasSuccessful()) {
            // Import edilen entity'leri al (non-owning pointers for space detection)
            auto entityPtrs = dialog.GetImportedEntities();
            size_t entityCount = entityPtrs.size();

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

            // CAD entity'leri Document'a kaydet (ownership transfer)
            auto ownedEntities = dialog.TakeEntities();
            LogCAD("[MainWindow] TakeEntities returned: " + std::to_string(ownedEntities.size())
                   + " entities, m_document=" + std::string(m_document ? "valid" : "NULL"));
            if (!ownedEntities.empty() && m_document) {
                m_document->SetCADEntities(std::move(ownedEntities));
                LogCAD("[MainWindow] Document now has " + std::to_string(m_document->GetCADEntities().size()) + " CAD entities");
                if (m_vulkanWindow) {
                    m_vulkanWindow->SetCADEntities(&m_document->GetCADEntities());
                    LogCAD("[MainWindow] SetCADEntities on VulkanWindow done");
                } else {
                    LogCAD("[MainWindow] ERROR: m_vulkanWindow is NULL!");
                }
            } else {
                LogCAD("[MainWindow] WARNING: ownedEntities empty=" + std::to_string(ownedEntities.empty())
                       + " m_document=" + std::string(m_document ? "valid" : "NULL"));
            }

            // Space paneli güncelle
            if (m_spacePanel) {
                m_spacePanel->RefreshList();
            }

            m_logList->clear();
            m_logList->addItem(QString("CAD dosyası import başarılı!"));
            m_logList->addItem(QString("- %1 entity yüklendi").arg(entityCount));
            m_logList->addItem(QString("- %1 mahal eklendi").arg(addedCount));

            statusBar()->showMessage(QString("%1 entity, %2 mahal başarıyla yüklendi!")
                .arg(entityCount).arg(addedCount));

            // Auto zoom to fit imported drawing
            OnZoomExtents();
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
    // TODO: Secili node/edge'leri Command uzerinden sil
    statusBar()->showMessage("Silme islemi - secim yapilmadi");
}

void MainWindow::OnDrawPipe() {
    m_currentToolMode = ToolMode::DrawPipe;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Boru cizimi: Ilk noktayi tiklayin");
}

void MainWindow::OnDrawFixture() {
    m_currentToolMode = ToolMode::PlaceFixture;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Armatur: Yerlestirme noktasini tiklayin");
}

void MainWindow::OnDrawJunction() {
    m_currentToolMode = ToolMode::PlaceJunction;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Baglanti noktasi: Noktayi tiklayin");
}

void MainWindow::OnSelectMode() {
    m_currentToolMode = ToolMode::Select;
    m_drawState = DrawState::Idle;
    statusBar()->showMessage("Secim modu");
}

void MainWindow::OnPlanView() {
    if (m_vulkanWindow) {
        m_vulkanWindow->GetRenderer()->SetViewMode(render::ViewMode::Plan);
    }
    statusBar()->showMessage("Plan Gorunumu (2D)");
}

void MainWindow::OnIsometricView() {
    if (m_vulkanWindow) {
        m_vulkanWindow->GetRenderer()->SetViewMode(render::ViewMode::Isometric);
    }
    statusBar()->showMessage("Izometrik Gorunum (3D)");
}

void MainWindow::OnZoomExtents() {
    if (!m_vulkanWindow || !m_document) return;

    geom::Vec3 minPt(1e9, 1e9, 0), maxPt(-1e9, -1e9, 0);
    bool hasContent = false;

    // Network node extents
    auto& network = m_document->GetNetwork();
    const auto& nodes = network.GetNodes();
    for (const auto& n : nodes) {
        minPt.x = std::min(minPt.x, n.position.x);
        minPt.y = std::min(minPt.y, n.position.y);
        maxPt.x = std::max(maxPt.x, n.position.x);
        maxPt.y = std::max(maxPt.y, n.position.y);
        hasContent = true;
    }

    // CAD entity extents
    if (!m_document->GetCADEntities().empty()) {
        geom::Vec3 cadMin, cadMax;
        m_document->GetCADExtents(cadMin, cadMax);
        LogCAD("[OnZoomExtents] CAD extents: min=(" + std::to_string(cadMin.x) + "," + std::to_string(cadMin.y)
               + ") max=(" + std::to_string(cadMax.x) + "," + std::to_string(cadMax.y) + ")");
        if (cadMin.x < cadMax.x && cadMin.y < cadMax.y) {
            minPt.x = std::min(minPt.x, cadMin.x);
            minPt.y = std::min(minPt.y, cadMin.y);
            maxPt.x = std::max(maxPt.x, cadMax.x);
            maxPt.y = std::max(maxPt.y, cadMax.y);
            hasContent = true;
        }
    }

    if (hasContent) {
        m_vulkanWindow->GetViewport().ZoomExtents(minPt, maxPt, 0.15);
        auto& vp = m_vulkanWindow->GetViewport();
        LogCAD("[OnZoomExtents] Final bounds: min=(" + std::to_string(minPt.x) + "," + std::to_string(minPt.y)
               + ") max=(" + std::to_string(maxPt.x) + "," + std::to_string(maxPt.y) + ")");
        LogCAD("[OnZoomExtents] Viewport zoom=" + std::to_string(vp.GetZoom())
               + " center=(" + std::to_string(vp.GetCenter().x) + "," + std::to_string(vp.GetCenter().y)
               + ") size=" + std::to_string(vp.GetWidth()) + "x" + std::to_string(vp.GetHeight()));
    }
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

    QString filePath = QFileDialog::getSaveFileName(this,
        "Rapor Kaydet", "", "CSV Dosyasi (*.csv);;Metin Dosyasi (*.txt)");
    if (filePath.isEmpty()) return;

    auto& network = m_document->GetNetwork();
    mep::ScheduleGenerator generator(network);

    std::string content;
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        content = generator.ExportToCSV();
    } else {
        content = generator.GenerateHydraulicReport();
    }

    std::ofstream file(filePath.toStdString());
    if (file.is_open()) {
        file << content;
        file.close();
        statusBar()->showMessage(QString("Rapor kaydedildi: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Hata", "Dosya yazilamadi!");
    }
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
    std::cout << "Egim degisti: " << text.toStdString() << std::endl;
}

// ============================================================
// MOUSE EVENT HANDLERS (Cizim Araclari - Adim 6)
// ============================================================

void MainWindow::HandleMousePress(double worldX, double worldY, Qt::MouseButton button) {
    if (button != Qt::LeftButton || !m_document) return;

    auto& network = m_document->GetNetwork();

    switch (m_currentToolMode) {
    case ToolMode::DrawPipe: {
        if (m_drawState == DrawState::WaitingFirstPoint || m_drawState == DrawState::Idle) {
            // Ilk nokta: Node olustur veya mevcut node'a snap
            mep::Node node;
            node.type = mep::NodeType::Junction;
            node.position = geom::Vec3(worldX, worldY, 0.0);
            node.label = "J";
            m_firstNodeId = network.AddNode(node);
            m_firstClickPos = node.position;
            m_drawState = DrawState::WaitingSecondPoint;
            statusBar()->showMessage(QString("Boru: Ikinci noktayi tiklayin (x=%1, y=%2)")
                .arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        } else if (m_drawState == DrawState::WaitingSecondPoint) {
            // Ikinci nokta: Target node + Edge olustur
            mep::Node node;
            node.type = mep::NodeType::Junction;
            node.position = geom::Vec3(worldX, worldY, 0.0);
            node.label = "J";
            uint32_t secondNodeId = network.AddNode(node);

            mep::Edge edge;
            edge.nodeA = m_firstNodeId;
            edge.nodeB = secondNodeId;
            edge.type = mep::EdgeType::Supply;
            edge.diameter_mm = 20.0;
            edge.roughness_mm = 0.0015;
            edge.material = "PVC";
            edge.length_m = m_firstClickPos.DistanceTo(node.position);
            network.AddEdge(edge);

            m_document->SetModified(true);
            UpdateUI();

            // Zincirleme cizim: ikinci nokta sonraki borunun ilk noktasi olur
            m_firstNodeId = secondNodeId;
            m_firstClickPos = node.position;
            statusBar()->showMessage(QString("Boru eklendi (L=%1m). Sonraki noktayi tiklayin veya ESC")
                .arg(edge.length_m, 0, 'f', 2));
        }
        break;
    }
    case ToolMode::PlaceFixture: {
        mep::Node node;
        node.type = mep::NodeType::Fixture;
        node.position = geom::Vec3(worldX, worldY, 0.0);
        node.fixtureType = "Lavabo";
        node.label = "Lavabo";
        // Database'den varsayilan degerler
        auto& db = mep::Database::Instance();
        auto fixture = db.GetFixture("Lavabo");
        node.loadUnit = fixture.loadUnit;
        network.AddNode(node);
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("Armatur eklendi: %1 (x=%2, y=%3)")
            .arg("Lavabo").arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::PlaceJunction: {
        mep::Node node;
        node.type = mep::NodeType::Junction;
        node.position = geom::Vec3(worldX, worldY, 0.0);
        node.label = "J";
        network.AddNode(node);
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("Baglanti noktasi eklendi (x=%1, y=%2)")
            .arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::Select:
    default:
        // TODO: SelectionManager ile entity sec
        statusBar()->showMessage(QString("Konum: x=%1, y=%2")
            .arg(worldX, 0, 'f', 3).arg(worldY, 0, 'f', 3));
        break;
    }
}

void MainWindow::HandleMouseMove(double worldX, double worldY) {
    // Status bar'da koordinat goster
    statusBar()->showMessage(QString("x=%1  y=%2")
        .arg(worldX, 0, 'f', 3).arg(worldY, 0, 'f', 3));
}

} // namespace ui
} // namespace vkt
