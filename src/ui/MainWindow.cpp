/**
 * @file MainWindow.cpp
 * @brief Ana Pencere İmplementasyonu (FINE SANI++)
 */

#include "ui/MainWindow.hpp"
#include "ui/DXFImportDialog.hpp"
#include "ui/MimariBelirleDialog.hpp"
#include "core/ProjectManager.hpp"
#include "ui/SpacePanel.hpp"
#include "ui/CommandBar.hpp"
#include "ui/SnapOverlay.hpp"
#include "render/VulkanWindow.hpp"
#include "mep/HydraulicSolver.hpp"
#include "mep/ScheduleGenerator.hpp"
#include "mep/Database.hpp"
#include "mep/XLSXWriter.hpp"
#include "mep/MaterialProperties.hpp"
#include "core/Application.hpp"
#include "core/Commands.hpp"
#include "cad/Text.hpp"
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>

static void LogCAD(const std::string& msg) {
    std::cout << "[MainWindow] " << msg << std::endl;
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
    m_vulkanContainer = QWidget::createWindowContainer(m_vulkanWindow, this);
    m_vulkanContainer->setMinimumSize(400, 300);
    setCentralWidget(m_vulkanContainer);

    // Snap overlay — şeffaf, mouse olaylarını geçirir
    m_snapOverlay = new SnapOverlay(m_vulkanContainer);
    m_snapOverlay->resize(m_vulkanContainer->size());
    m_snapOverlay->raise();

    // Gerçek zamanlı hidrolik debounce timer (600ms, single-shot yeniden başlatılabilir)
    m_autoHydroTimer = new QTimer(this);
    m_autoHydroTimer->setSingleShot(true);
    connect(m_autoHydroTimer, &QTimer::timeout, this, &MainWindow::RunAutoHydro);

    // Mouse callback'lerini bagla
    m_vulkanWindow->SetMousePressCallback(
        [this](double wx, double wy, Qt::MouseButton btn) { HandleMousePress(wx, wy, btn); });
    m_vulkanWindow->SetMouseMoveCallback(
        [this](double wx, double wy) { HandleMouseMove(wx, wy); });
    m_vulkanWindow->SetViewportChangeCallback(
        [this]() { RefreshTextOverlay(); });

    CreateActions();
    CreateMenus();
    CreateToolbars();
    CreateDockPanels();

    // Proje kök dizinini kalıcı ayarlardan yükle
    QSettings settings("VKT", "MekanikTesisatCAD");
    QString projectsRoot = settings.value("projectsRoot", "").toString();
    if (!projectsRoot.isEmpty())
        core::ProjectManager::Instance().SetProjectsRoot(projectsRoot.toStdString());

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

    // Pis su araçları
    m_actDrawDrainPipe = new QAction("Pis Su Borusu", this);
    m_actDrawDrainPipe->setToolTip("Pis su/atık su borusu çiz (EN 12056-2)");
    connect(m_actDrawDrainPipe, &QAction::triggered, this, &MainWindow::OnDrawDrainPipe);

    m_actPlaceYerSuzgeci = new QAction("Yer Süzgeci", this);
    m_actPlaceYerSuzgeci->setToolTip("Yer süzgeci / tahliye süzgeci yerleştir");
    connect(m_actPlaceYerSuzgeci, &QAction::triggered, this, &MainWindow::OnPlaceYerSuzgeci);

    m_actPlaceRogar = new QAction("Rögar (Boşaltma)", this);
    m_actPlaceRogar->setToolTip("Pis su boşaltma noktası (rögar) yerleştir");
    connect(m_actPlaceRogar, &QAction::triggered, this, &MainWindow::OnPlaceRogar);

    m_actCopyFloor = new QAction("Tesisat Kopyala...", this);
    m_actCopyFloor->setToolTip("Seçilen katın yatay borularını ve armatürlerini başka kata kopyala (kolonlar hariç)");
    connect(m_actCopyFloor, &QAction::triggered, this, &MainWindow::OnCopyFloor);

    m_actConnectFixture = new QAction("Cihazı Tesisata Bagla", this);
    m_actConnectFixture->setToolTip("Armaturu ana boru hattina kisa dal boru ile bagla (BAGLA komutu)");
    m_actConnectFixture->setShortcut(QKeySequence("Ctrl+B"));
    connect(m_actConnectFixture, &QAction::triggered, this, &MainWindow::OnConnectFixture);

    m_actHidrofor = new QAction("Hidrofor Boyutlandirma...", this);
    m_actHidrofor->setToolTip("Kritik devre analizi sonucuna gore hidrofor/pompa secimi");
    connect(m_actHidrofor, &QAction::triggered, this, &MainWindow::OnHidrofor);

    m_actNormSelection = new QAction("Hesap Normu...", this);
    m_actNormSelection->setToolTip("Besleme debisi hesaplama normunu sec: EN 806-3 veya DIN 1988-300");
    connect(m_actNormSelection, &QAction::triggered, this, &MainWindow::OnNormSelection);

    m_actYagmurSuyu = new QAction("Yagmur Suyu Modulu...", this);
    m_actYagmurSuyu->setToolTip("EN 12056-3: Yagmur suyu tahliye boru boyutlandirmasi");
    connect(m_actYagmurSuyu, &QAction::triggered, this, &MainWindow::OnYagmurSuyu);

    m_actBOM = new QAction("Kesif Listesi (BOM)...", this);
    m_actBOM->setToolTip("Boru metrajlari ve baglanti elemanlari dokumu");
    m_actBOM->setShortcut(QKeySequence("Ctrl+K"));
    connect(m_actBOM, &QAction::triggered, this, &MainWindow::OnBOM);

    m_actSelect = new QAction("Sec", this);
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

    m_actAutoSizeDN = new QAction("DN Boyutlandır", this);
    m_actAutoSizeDN->setShortcut(Qt::Key_F6);
    m_actAutoSizeDN->setToolTip("Çizilen ağı EN 806-3'e göre otomatik boyutlandır (F6)");
    connect(m_actAutoSizeDN, &QAction::triggered, this, &MainWindow::OnAutoSizeDN);

    m_actGenerateSchedule = new QAction("Metraj Oluştur", this);
    connect(m_actGenerateSchedule, &QAction::triggered, this, &MainWindow::OnGenerateSchedule);

    m_actExportReport = new QAction("Rapor Dışa Aktar", this);
    connect(m_actExportReport, &QAction::triggered, this, &MainWindow::OnExportReport);

    m_actMimariBelirle = new QAction("&Mimari Belirle...", this);
    m_actMimariBelirle->setShortcut(QKeySequence("Ctrl+M"));
    m_actMimariBelirle->setToolTip("Kat tanımları ve mimari DXF/DWG dosyalarını belirle");
    connect(m_actMimariBelirle, &QAction::triggered, this, &MainWindow::OnMimariBelirle);

    // Proje çalışma alanı (CC klasörü eşdeğeri)
    m_actNewProject = new QAction("&Yeni Proje...", this);
    m_actNewProject->setShortcut(QKeySequence("Ctrl+Shift+N"));
    m_actNewProject->setToolTip("Proje kök dizininde yeni proje klasörü oluştur");
    connect(m_actNewProject, &QAction::triggered, this, &MainWindow::OnNewProject);

    m_actSetProjectsRoot = new QAction("Proje &Kök Klasörü Ayarla...", this);
    m_actSetProjectsRoot->setToolTip("Tüm projelerin tutulduğu ana dizini belirle (CC klasörü eşdeğeri)");
    connect(m_actSetProjectsRoot, &QAction::triggered, this, &MainWindow::OnSetProjectsRoot);

    m_actOpenProjectFolder = new QAction("Proje &Klasörünü Aç", this);
    m_actOpenProjectFolder->setToolTip("Aktif proje klasörünü Dosya Gezgini'nde aç");
    connect(m_actOpenProjectFolder, &QAction::triggered, this, &MainWindow::OnOpenProjectFolder);
}

void MainWindow::CreateMenus() {
    // Dosya
    auto* fileMenu = menuBar()->addMenu("&Dosya");
    fileMenu->addAction(m_actNewProject);
    fileMenu->addAction(m_actNew);
    fileMenu->addAction(m_actOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actImportDXF);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSetProjectsRoot);
    fileMenu->addAction(m_actOpenProjectFolder);
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
    drawMenu->addAction(m_actConnectFixture);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawDrainPipe);
    drawMenu->addAction(m_actPlaceYerSuzgeci);
    drawMenu->addAction(m_actPlaceRogar);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actCopyFloor);

    // Görünüm
    auto* viewMenu = menuBar()->addMenu("&Görünüm");
    viewMenu->addAction(m_actPlanView);
    viewMenu->addAction(m_actIsometricView);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actZoomExtents);

    // Analiz
    auto* analyzeMenu = menuBar()->addMenu("&Analiz");
    analyzeMenu->addAction(m_actRunHydraulics);
    analyzeMenu->addAction(m_actAutoSizeDN);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actHidrofor);
    analyzeMenu->addAction(m_actNormSelection);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actYagmurSuyu);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actBOM);
    analyzeMenu->addAction(m_actGenerateSchedule);
    analyzeMenu->addAction(m_actExportReport);

    // Mimari
    auto* mimariMenu = menuBar()->addMenu("&Mimari");
    mimariMenu->addAction(m_actMimariBelirle);
}

void MainWindow::CreateToolbars() {
    // Çizim toolbar
    m_drawToolbar = addToolBar("Çizim");
    m_drawToolbar->addAction(m_actSelect);
    m_drawToolbar->addAction(m_actDrawPipe);
    m_drawToolbar->addAction(m_actDrawFixture);
    m_drawToolbar->addAction(m_actDrawJunction);
    m_drawToolbar->addAction(m_actConnectFixture);

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
    m_analysisToolbar->addAction(m_actAutoSizeDN);
    m_analysisToolbar->addSeparator();
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

    // Fixture Properties Panel
    m_fixtureDock  = new QDockWidget("Armatür Özellikleri", this);
    m_fixturePanel = new FixturePropertiesPanel(this);
    m_fixtureDock->setWidget(m_fixturePanel);
    m_fixtureDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, m_fixtureDock);
    tabifyDockWidget(m_propertyPanel, m_fixtureDock);
    m_propertyPanel->raise(); // Boru özellikleri varsayılan sekme

    connect(m_fixturePanel, &FixturePropertiesPanel::NodeUpdated,
            this,           &MainWindow::OnNodeUpdated);

    // PBR Material Editor
    m_pbrDock   = new QDockWidget("PBR Malzeme Editörü", this);
    m_pbrEditor = new PBRMaterialEditor(this);
    m_pbrDock->setWidget(m_pbrEditor);
    m_pbrDock->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea, m_pbrDock);
    tabifyDockWidget(m_fixtureDock, m_pbrDock);

    connect(m_pbrEditor, &PBRMaterialEditor::MaterialChanged,
            this,        &MainWindow::OnPBRMaterialChanged);

    // ST Cihazları Panel
    m_stDock  = new QDockWidget("ST Cihazları", this);
    m_stPanel = new STFixturePanel(this);
    m_stDock->setWidget(m_stPanel);
    m_stDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, m_stDock);
    tabifyDockWidget(m_pbrDock, m_stDock);

    connect(m_stPanel, &STFixturePanel::FixtureSelected,
            this,      &MainWindow::OnSTFixtureSelected);

    // Log Panel
    m_logPanel = new QDockWidget("Analiz Logu", this);
    m_logList = new QListWidget();
    m_logPanel->setWidget(m_logList);
    addDockWidget(Qt::BottomDockWidgetArea, m_logPanel);

    // Komut Satırı (alt bar — Log'un yanına yerleştirilir)
    m_commandBar = new CommandBar(this);
    auto* cmdDock = new QDockWidget("Komut Satırı", this);
    cmdDock->setWidget(m_commandBar);
    cmdDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    cmdDock->setTitleBarWidget(new QWidget()); // başlık çubuğunu gizle
    addDockWidget(Qt::BottomDockWidgetArea, cmdDock);
    tabifyDockWidget(m_logPanel, cmdDock);
    cmdDock->raise();

    connect(m_commandBar, &CommandBar::CommandEntered,
            this,          &MainWindow::OnCommandEntered);
    connect(m_commandBar, &CommandBar::EscapePressed,
            this,          &MainWindow::OnCommandEscape);
}

void MainWindow::UpdateUI() {
    bool hasDoc = (m_document != nullptr);

    m_actSave->setEnabled(hasDoc);
    m_actSaveAs->setEnabled(hasDoc);
    m_actUndo->setEnabled(hasDoc && m_document->CanUndo());
    m_actRedo->setEnabled(hasDoc && m_document->CanRedo());
    m_actRunHydraulics->setEnabled(hasDoc);
    m_actGenerateSchedule->setEnabled(hasDoc);

    // MEP node/edge label'larını overlay'e yansıt
    if (m_vulkanWindow) RefreshTextOverlay();
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
    // Proje kök klasörü varsa oradan başla
    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasProjectsRoot()
        ? QString::fromStdString(pm.GetProjectsRoot())
        : "";

    QString filePath = QFileDialog::getOpenFileName(this,
        "Proje Aç", startDir, "VKT Projesi (*.vkt);;Tüm Dosyalar (*)");
    if (filePath.isEmpty()) return;

    // Projeyi ProjectManager'a bildir
    QFileInfo fi(filePath);
    pm.SetActiveProject(fi.dir().absolutePath().toStdString());

    auto& app = core::Application::Instance();
    auto* doc = app.OpenDocument(filePath.toStdString());
    if (doc) {
        SetDocument(doc);
        setWindowTitle(QString("VKT - FINE SANI++ — %1").arg(
            QString::fromStdString(pm.GetProjectName())));
        statusBar()->showMessage(QString("Proje açıldı: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Hata", "Proje dosyası açılamadı!");
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

    // Aktif proje klasöründen başla; yoksa kök dizininden
    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetProjectFolder())
        : pm.HasProjectsRoot()
            ? QString::fromStdString(pm.GetProjectsRoot())
            : "";

    QString filePath = QFileDialog::getSaveFileName(this,
        "Projeyi Kaydet", startDir, "VKT Projesi (*.vkt)");
    if (filePath.isEmpty()) return;

    // Proje klasörünü güncelle
    QFileInfo fi(filePath);
    pm.SetActiveProject(fi.dir().absolutePath().toStdString());

    if (m_document->Save(filePath.toStdString())) {
        setWindowTitle(QString("VKT - FINE SANI++ — %1").arg(
            QString::fromStdString(pm.GetProjectName())));
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

            // Dialog'dan mahal kabul edilmediyse otomatik tespit yap
            if (addedCount == 0 && !entityPtrs.empty()) {
                cad::SpaceDetectionOptions autoOpts;
                autoOpts.wallLayerNames   = {};      // tüm layer'lardan tara
                autoOpts.minArea          = 1.0;     // 1 m²
                autoOpts.maxArea          = 10000.0;
                autoOpts.detectNamesFromText = true;
                autoOpts.autoInferTypes   = true;

                // Önce kapalı polyline'lardan dene
                auto autoCandidates = m_spaceManager->DetectSpacesFromEntities(entityPtrs, autoOpts);

                // Kapalı polyline'dan bulunamadıysa LINE/LWPOLYLINE segmentlerini birleştir
                if (autoCandidates.empty()) {
                    autoCandidates = m_spaceManager->DetectSpacesFromSegments(entityPtrs, autoOpts, 50.0);
                    LogCAD("[MainWindow] Segment birleştirme ile " + std::to_string(autoCandidates.size()) + " mahal adayı");
                }

                for (const auto& c : autoCandidates) {
                    if (c.isValid) {
                        if (m_spaceManager->AcceptCandidate(c)) {
                            addedCount++;
                        }
                    }
                }
                LogCAD("[MainWindow] Otomatik mahal tespiti: " + std::to_string(autoCandidates.size())
                       + " aday, " + std::to_string(addedCount) + " kabul edildi");
            }

            // Komşulukları tespit et
            if (addedCount > 0) {
                m_spaceManager->DetectAllAdjacencies(10.0); // 10mm tolerance
            }

            // Layer bilgilerini kaydet (entity'lerden ÖNCE — renk çözümlemesi tamamlanmış)
            auto layers = dialog.GetLayers();
            if (m_document && !layers.empty()) {
                m_document->SetLayers(layers);
                LogCAD("[MainWindow] " + std::to_string(layers.size()) + " layers stored in Document");
            }

            // CAD entity'leri Document'a kaydet (ownership transfer)
            auto ownedEntities = dialog.TakeEntities();
            LogCAD("[MainWindow] TakeEntities returned: " + std::to_string(ownedEntities.size())
                   + " entities, m_document=" + std::string(m_document ? "valid" : "NULL"));
            if (!ownedEntities.empty() && m_document) {
                m_document->SetCADEntities(std::move(ownedEntities));
                LogCAD("[MainWindow] Document now has " + std::to_string(m_document->GetCADEntities().size()) + " CAD entities");

                // Büyük koordinatlı çizimleri (ulusal grid, coğrafi referans) origin'e kaydır.
                // float32 vertex hassasiyetini korur; eksen dağılmasını önler.
                auto offset = m_document->NormalizeCoordinates();
                if (offset.x != 0.0 || offset.y != 0.0) {
                    LogCAD("[MainWindow] Koordinat normalizasyonu uygulandı: offset=("
                           + std::to_string(offset.x) + ", " + std::to_string(offset.y) + ")");
                }

                if (m_vulkanWindow) {
                    m_vulkanWindow->SetCADEntities(&m_document->GetCADEntities());
                    // Layer renk haritasını renderer'a ilet (ByLayer çözümlemesi için)
                    if (auto* r = m_vulkanWindow->GetRenderer()) {
                        r->SetLayerMap(m_document->GetLayers());
                        r->SetGlobalLtscale(static_cast<float>(dialog.GetDXFLtscale()));
                    }
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
            m_logList->addItem(QString("- %1 layer bulundu").arg(layers.size()));
            m_logList->addItem(QString("- %1 mahal eklendi").arg(addedCount));

            statusBar()->showMessage(QString("%1 entity, %2 layer, %3 mahal başarıyla yüklendi!")
                .arg(entityCount).arg(layers.size()).arg(addedCount));

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
        ScheduleAutoHydro(); // MEP ağı değişmiş olabilir → DN yeniden hesapla
    }
}

void MainWindow::OnRedo() {
    if (m_document) {
        m_document->Redo();
        UpdateUI();
        ScheduleAutoHydro();
    }
}

void MainWindow::OnDelete() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();

    if (m_selectedNodeId != 0) {
        auto cmd = std::make_unique<core::DeleteNodeCommand>(network, m_selectedNodeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Node #%1 silindi").arg(m_selectedNodeId));
        m_selectedNodeId = 0;
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
        if (m_fixturePanel) m_fixturePanel->Clear();
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else if (m_selectedEdgeId != 0) {
        auto cmd = std::make_unique<core::DeleteEdgeCommand>(network, m_selectedEdgeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Kenar #%1 silindi").arg(m_selectedEdgeId));
        m_selectedEdgeId = 0;
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else {
        statusBar()->showMessage("Silme: önce bir öğe seçin (Select modu)");
    }
}

void MainWindow::OnDrawPipe() {
    m_currentToolMode = ToolMode::DrawPipe;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Boru cizimi: Ilk noktayi tiklayin");
}

void MainWindow::OnDrawFixture() {
    // Fixture tipi seçici
    auto& db = mep::Database::Instance();
    QStringList types;
    for (const auto& n : db.GetFixtureNames())
        types << QString::fromStdString(n);
    if (types.isEmpty()) types << "Lavabo";

    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Armatür Tipi",
        "Yerleştirilecek armatürü seçin:", types,
        types.indexOf(m_selectedFixtureType), false, &ok);
    if (!ok) return;

    m_selectedFixtureType = chosen;
    m_currentToolMode = ToolMode::PlaceFixture;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage(QString("Armatür [%1]: Yerlestirme noktasını tıklayın").arg(m_selectedFixtureType));
}

void MainWindow::OnDrawJunction() {
    m_currentToolMode = ToolMode::PlaceJunction;
    m_drawState = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Baglanti noktasi: Noktayi tiklayin");
}

void MainWindow::OnSelectMode() {
    m_currentToolMode  = ToolMode::Select;
    m_drawState        = DrawState::Idle;
    m_firstNodeInGraph = false;
    if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }
    statusBar()->showMessage("Seçim modu");
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
        m_vulkanWindow->requestUpdate();
        RefreshTextOverlay();
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

// ============================================================
//  DN OTOMATİK BOYUTLANDIRMA (EN 806-3)
// ============================================================
void MainWindow::OnAutoSizeDN() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        statusBar()->showMessage("DN boyutlandırma: önce boru çizin.");
        return;
    }

    mep::HydraulicSolver solver(network);
    solver.Solve();
    solver.SolveDrainage();

    // EN 806-3: v_max=3 m/s → D_min = sqrt(4Q/π/v_max) mm
    // Solver flow_rate_lps yazılmışsa kullan, yoksa LU→Q tahmin
    const double v_max = 2.5; // m/s (konforlu)
    int sized = 0;

    // Standart DN serileri (PVC/PP mm OD → iç çap yaklaşık)
    static const double kDN[] = {16,20,25,32,40,50,63,75,90,110,125,160,200,0};

    auto selectDN = [&](double Q_lps) -> double {
        double Q_m3s = Q_lps / 1000.0;
        double d_m   = std::sqrt(4.0 * Q_m3s / M_PI / v_max);
        double d_mm  = d_m * 1000.0;
        for (int i = 0; kDN[i] > 0; ++i)
            if (kDN[i] >= d_mm) return kDN[i];
        return 200.0;
    };

    m_logList->clear();
    m_logList->addItem("── DN Otomatik Boyutlandırma (EN 806-3) ──");

    // Toplam LU: tüm fixture'lar (EN 806-3 Tablo 3 yaklaşımı)
    double totalLU = 0.0;
    for (const auto& [nid, node] : network.GetNodeMap())
        if (node.type == mep::NodeType::Fixture) totalLU += node.loadUnit;
    totalLU = std::max(totalLU, 0.1);

    int edgeCount = static_cast<int>(network.GetEdgeMap().size());

    for (auto& [eid, edge] : network.GetEdgeMap()) {
        // Önce solver sonucunu kullan, yoksa LU'dan tahmin et
        double Q_m3s = edge.flowRate_m3s;
        double Q_lps;
        if (Q_m3s > 1e-9) {
            Q_lps = Q_m3s * 1000.0;
        } else {
            // EN 806-3: Q = 0.682 * LU^0.45  (l/s), eşit dağılım varsayımı
            double edgeLU = totalLU / std::max(edgeCount, 1);
            Q_lps = 0.682 * std::pow(edgeLU, 0.45);
        }
        Q_lps = std::max(Q_lps, 0.05);
        double newDN = selectDN(Q_lps);
        if (std::abs(newDN - edge.diameter_mm) > 0.5) {
            m_logList->addItem(QString("Boru #%1: DN%2 → DN%3 (Q=%4 l/s)")
                .arg(eid).arg(edge.diameter_mm, 0, 'f', 0)
                .arg(newDN, 0, 'f', 0).arg(Q_lps, 0, 'f', 3));
        }
        edge.diameter_mm = newDN;
        edge.label = "DN" + std::to_string(static_cast<int>(newDN));
        sized++;
    }

    m_logList->addItem(QString("✅ %1 boru boyutlandırıldı").arg(sized));
    m_document->SetModified(true);
    UpdateUI();
    statusBar()->showMessage(QString("DN boyutlandırma tamamlandı: %1 boru güncellendi").arg(sized));
}

// ============================================================
//  GERÇEK ZAMANLI HİDROLİK (DEBOUNCED AUTO-DN)
// ============================================================

void MainWindow::ScheduleAutoHydro() {
    // QTimer::start() var olan timer'ı yeniden başlatır (debounce)
    if (m_autoHydroTimer) m_autoHydroTimer->start(600);
}

void MainWindow::RunAutoHydro() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) return;

    static const double kDN[] = {16,20,25,32,40,50,63,75,90,110,125,160,200,0};
    const double v_max = 2.5;

    auto selectDN = [&](double Q_lps) -> double {
        double d_mm = std::sqrt(4.0 * (Q_lps / 1000.0) / M_PI / v_max) * 1000.0;
        for (int i = 0; kDN[i] > 0; ++i)
            if (kDN[i] >= d_mm) return kDN[i];
        return 200.0;
    };

    // Solver'ı sessizce çalıştır (flowRate_m3s doldurur)
    mep::HydraulicSolver solver(network);
    solver.Solve();
    solver.SolveDrainage();

    // ── Topoloji tabanlı downstream LU (DFS) ─────────────────
    // Her edge için: o edge'in "ilerisi"ndeki fixture'ların toplam LU'su
    // Tree-like ağlarda doğru; döngü varsa fallback eşit dağılım
    std::unordered_map<uint32_t, double> edgeDownstreamLU; // eid → downstream LU
    {
        // Her node'un kendi LU katkısı
        std::unordered_map<uint32_t, double> nodeLU;
        double totalLU = 0.0;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            double lu = (node.type == mep::NodeType::Fixture) ? std::max(node.loadUnit, 0.1) : 0.0;
            nodeLU[nid] = lu;
            totalLU += lu;
        }
        totalLU = std::max(totalLU, 0.1);

        // DFS: her Source'dan başla, her edge için downstream subtree LU hesapla
        std::unordered_set<uint32_t> visited;
        std::function<double(uint32_t, uint32_t)> dfs = [&](uint32_t nid, uint32_t parentId) -> double {
            visited.insert(nid);
            double lu = nodeLU[nid];
            for (uint32_t eid : network.GetConnectedEdges(nid)) {
                const mep::Edge* e = network.GetEdge(eid);
                if (!e) continue;
                uint32_t neighbor = (e->nodeA == nid) ? e->nodeB : e->nodeA;
                if (neighbor == parentId || visited.count(neighbor)) continue;
                double childLU = dfs(neighbor, nid);
                lu += childLU;
                // Bu edge, neighbor tarafındaki subtree'yi taşıyor
                edgeDownstreamLU[eid] = childLU;
            }
            return lu;
        };

        // Source node'lardan başla; source yoksa tüm node'lardan başla
        bool hasSource = false;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            if (node.type == mep::NodeType::Source) {
                if (!visited.count(nid)) dfs(nid, UINT32_MAX);
                hasSource = true;
            }
        }
        if (!hasSource) {
            for (const auto& [nid, node] : network.GetNodeMap())
                if (!visited.count(nid)) dfs(nid, UINT32_MAX);
        }

        // Hâlâ atanmamış edge'ler (izole bölgeler) → eşit dağılım
        int edgeCount = static_cast<int>(network.GetEdgeMap().size());
        double equalLU = totalLU / std::max(edgeCount, 1);
        for (const auto& [eid, edge] : network.GetEdgeMap())
            if (!edgeDownstreamLU.count(eid))
                edgeDownstreamLU[eid] = equalLU;
    }

    // ── Edge başına DN seçimi ──────────────────────────────────
    int updated = 0;
    for (auto& [eid, edge] : network.GetEdgeMap()) {
        double Q_lps;
        if (edge.flowRate_m3s > 1e-9) {
            // Solver çözdüyse doğrudan kullan
            Q_lps = edge.flowRate_m3s * 1000.0;
        } else {
            // Downstream LU'dan EN 806-3 tahmini
            double lu = std::max(edgeDownstreamLU[eid], 0.1);
            Q_lps = 0.682 * std::pow(lu, 0.45);
        }
        Q_lps = std::max(Q_lps, 0.05);
        double newDN = selectDN(Q_lps);
        if (std::abs(newDN - edge.diameter_mm) > 0.5 || edge.label.empty()) {
            edge.diameter_mm = newDN;
            edge.label = "DN" + std::to_string(static_cast<int>(newDN));
            updated++;
        }
    }

    if (updated > 0) {
        RefreshTextOverlay();
        statusBar()->showMessage(
            QString("⚡ Otomatik DN: %1 boru güncellendi (EN 806-3)").arg(updated), 3000);
    }
}

// ============================================================
//  ESC — ÇİZİM MODU İPTAL
// ============================================================
void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (m_currentToolMode != ToolMode::Select) {
            m_currentToolMode = ToolMode::Select;
            m_drawState       = DrawState::Idle;
            m_firstNodeInGraph = false;
            if (m_snapOverlay) m_snapOverlay->ClearRubberBand();
            if (m_snapOverlay) m_snapOverlay->Hide();
            statusBar()->showMessage("Çizim iptal edildi — Seçim modu");
        }
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
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

    // Proje varsa rapor/ klasöründen başla
    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder())
        : "";

    QString filePath = QFileDialog::getSaveFileName(this,
        "Rapor Kaydet", startDir,
        "Excel Raporu (*.xls);;"
        "CSV Dosyası (*.csv);;"
        "Metin Dosyası (*.txt)");
    if (filePath.isEmpty()) return;

    auto& network = m_document->GetNetwork();
    bool ok = false;

    if (filePath.endsWith(".xls", Qt::CaseInsensitive)) {
        ok = mep::XLSXWriter::ExportProjectReport(filePath.toStdString(), network);
    } else {
        mep::ScheduleGenerator generator(network);
        std::string content = filePath.endsWith(".csv", Qt::CaseInsensitive)
                              ? generator.ExportToCSV()
                              : generator.GenerateHydraulicReport();
        std::ofstream f(filePath.toStdString());
        if (f.is_open()) { f << content; ok = true; }
    }

    if (ok) {
        statusBar()->showMessage(QString("Rapor kaydedildi: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Hata", "Dosya yazılamadı!");
    }
}

void MainWindow::OnNewProject() {
    auto& pm = core::ProjectManager::Instance();

    // 1. Proje kök dizini yoksa önce belirlet
    if (!pm.HasProjectsRoot()) {
        QMessageBox::information(this, "Proje Kök Dizini Gerekli",
            "Yeni proje oluşturmadan önce projelerin tutulacağı ana klasörü belirlemeniz gerekir.\n"
            "(Dosya → Proje Kök Klasörü Ayarla...)");
        OnSetProjectsRoot();
        if (!pm.HasProjectsRoot()) return;
    }

    // 2. Proje adı al
    bool ok = false;
    QString name = QInputDialog::getText(this, "Yeni Proje",
        QString("Proje adı:\n(Klasör: %1/[ad])")
            .arg(QString::fromStdString(pm.GetProjectsRoot())),
        QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // 3. Proje klasörünü oluştur
    std::string error;
    if (!pm.CreateProject(name.trimmed().toStdString(), error)) {
        QMessageBox::critical(this, "Proje Oluşturulamadı", QString::fromStdString(error));
        return;
    }

    // 4. Yeni belge oluştur ve proje dosya yoluna kaydet
    auto& app = core::Application::Instance();
    auto* doc = app.CreateNewDocument();
    SetDocument(doc);

    // Ana .vkt dosyasına kaydet
    std::string mainFile = pm.GetMainFilePath();
    if (!mainFile.empty()) {
        doc->Save(mainFile);
    }

    QString displayName = QString::fromStdString(pm.GetProjectName());
    setWindowTitle(QString("VKT - FINE SANI++ — %1").arg(displayName));
    statusBar()->showMessage(
        QString("Proje oluşturuldu: %1").arg(QString::fromStdString(pm.GetProjectFolder())));

    if (m_logList)
        m_logList->addItem(QString("Yeni proje: %1").arg(
            QString::fromStdString(pm.GetProjectFolder())));
}

void MainWindow::OnSetProjectsRoot() {
    QString current = QString::fromStdString(
        core::ProjectManager::Instance().GetProjectsRoot());

    QString dir = QFileDialog::getExistingDirectory(this,
        "Proje Kök Klasörünü Seç (CC Klasörü Eşdeğeri)",
        current.isEmpty() ? QDir::homePath() : current,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir.isEmpty()) return;

    core::ProjectManager::Instance().SetProjectsRoot(dir.toStdString());

    // Kalıcı olarak kaydet
    QSettings settings("VKT", "MekanikTesisatCAD");
    settings.setValue("projectsRoot", dir);

    statusBar()->showMessage(
        QString("Proje kök klasörü ayarlandı: %1").arg(dir));
}

void MainWindow::OnOpenProjectFolder() {
    auto& pm = core::ProjectManager::Instance();
    std::string folder = pm.HasActiveProject()
        ? pm.GetProjectFolder()
        : pm.GetProjectsRoot();

    if (folder.empty()) {
        QMessageBox::information(this, "Proje Klasörü",
            "Henüz aktif proje veya kök klasör belirlenmemiş.\n"
            "Dosya → Proje Kök Klasörü Ayarla...");
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(folder)));
}

void MainWindow::OnMimariBelirle() {
    MimariBelirleDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        dlg.ApplyToFloorManager(m_floorManager);
        const auto& floors = m_floorManager.GetFloors();
        statusBar()->showMessage(
            QString("%1 kat tanımlandı.").arg((int)floors.size()));
        LogCAD("Mimari Belirle: " + std::to_string(floors.size()) + " kat");

        // Her kat için DXF/DWG dosyası varsa DXFImportDialog aç
        // Referans noktası ofseti: tüm entity'ler (-refX, -refY) kadar kaydırılır,
        // böylece seçilen ortak nokta (kolon köşesi vb.) projenin (0,0)'ına eşlenir.
        // Global referans noktası — tüm katlarda aynı fiziksel nokta (0,0)'a eşlenir
        double globalRefX = dlg.GetGlobalRefX();
        double globalRefY = dlg.GetGlobalRefY();

        for (const auto& def : dlg.GetFloorDefs()) {
            if (def.dosya.empty()) continue;
            if (m_document) {
                LogCAD("Mimari import baslatiyor: " + def.dosya + " (" + def.isim + ")");
                DXFImportDialog importDlg(this);
                importDlg.SetInsertionOffset(globalRefX, globalRefY);
                importDlg.exec();
            }
        }
    }
}

void MainWindow::OnSTFixtureSelected(const QString& name) {
    m_selectedFixtureType = name;
    m_currentToolMode     = ToolMode::PlaceFixture;
    m_drawState           = DrawState::WaitingFirstPoint;
    statusBar()->showMessage(
        QString("ST Cihaz [%1]: Yerlestirme noktasini tiklayin").arg(name));
    if (m_stDock) m_stDock->raise();
}

void MainWindow::OnDrawDrainPipe() {
    m_currentPipeType  = mep::EdgeType::Drainage;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Pis su borusu: Baslangic noktasini tiklayin (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Pis su bas.");
}

void MainWindow::OnPlaceYerSuzgeci() {
    m_drainLabel      = "Yer Suzgeci";
    m_currentToolMode = ToolMode::PlaceDrain;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Yer suzgeci: Konumu tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Yer suzgeci");
}

void MainWindow::OnPlaceRogar() {
    m_drainLabel      = "Rogar";
    m_currentToolMode = ToolMode::PlaceDrain;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Bosaltma noktasi (Rogar): Konumu tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Rogar konumu");
}

void MainWindow::OnCopyFloor() {
    if (!m_document) return;

    const auto& floors = m_floorManager.GetFloors();
    if (floors.size() < 2) {
        QMessageBox::information(this, "Tesisat Kopyala",
            "En az 2 kat tanımlanmış olmalıdır.\n"
            "Önce Mimari → Mimari Belirle ile katları tanımlayın.");
        return;
    }

    // Kat listesi için string listesi oluştur
    QStringList floorNames;
    for (const auto& f : floors)
        floorNames << QString("%1 — %2 m (%3)")
            .arg(f.index).arg(f.elevation_m, 0, 'f', 2)
            .arg(QString::fromStdString(f.label));

    bool ok = false;
    QString srcChoice = QInputDialog::getItem(this, "Tesisat Kopyala",
        "Kaynak kat (kopyalanacak):", floorNames, 0, false, &ok);
    if (!ok) return;

    QString dstChoice = QInputDialog::getItem(this, "Tesisat Kopyala",
        "Hedef kat (kopyalanacağı yer):", floorNames, 1, false, &ok);
    if (!ok || srcChoice == dstChoice) return;

    int srcIdx = floorNames.indexOf(srcChoice);
    int dstIdx = floorNames.indexOf(dstChoice);
    if (srcIdx < 0 || dstIdx < 0 || srcIdx >= (int)floors.size() || dstIdx >= (int)floors.size()) return;

    double srcZ = floors[srcIdx].elevation_m;
    double dstZ = floors[dstIdx].elevation_m;
    double dz   = dstZ - srcZ;

    auto& network = m_document->GetNetwork();

    // Kaynak kattaki node'ları bul (z ≈ srcZ, tolerans 0.1 m)
    constexpr double kZTol = 0.1;
    std::unordered_map<uint32_t, uint32_t> oldToNew; // srcNodeId → dstNodeId

    auto composite = std::make_unique<core::CompositeCommand>();

    // 1. Node'ları kopyala (kolonlar hariç tutmak için sadece srcZ'deki node'lar)
    for (const auto& [nid, node] : network.GetNodeMap()) {
        if (std::abs(node.position.z - srcZ) > kZTol) continue;

        mep::Node newNode = node;
        newNode.position.z += dz;
        auto cmd = std::make_unique<core::AddNodeCommand>(network, newNode);
        cmd->Execute();
        uint32_t newId = cmd->GetNodeId();
        oldToNew[nid] = newId;
        composite->AddCommand(std::move(cmd));
    }

    if (oldToNew.empty()) {
        QMessageBox::warning(this, "Tesisat Kopyala",
            QString("Kaynak katta (%1 m) hiç node bulunamadı.\n"
                    "Node'ların Z koordinatları kat kotuna eşlenmiş olmalıdır.")
            .arg(srcZ, 0, 'f', 2));
        return;
    }

    // 2. Yatay edge'leri kopyala — her iki ucu da srcZ'de olan borular (kolonlar değil)
    int copiedEdges = 0;
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        auto itA = oldToNew.find(edge.nodeA);
        auto itB = oldToNew.find(edge.nodeB);
        if (itA == oldToNew.end() || itB == oldToNew.end()) continue; // kolon veya dışarıdaki

        mep::Edge newEdge = edge;
        newEdge.nodeA = itA->second;
        newEdge.nodeB = itB->second;
        auto cmd = std::make_unique<core::AddEdgeCommand>(network, newEdge);
        cmd->Execute();
        composite->AddCommand(std::move(cmd));
        ++copiedEdges;
    }

    m_document->TrackExecuted(std::move(composite));
    m_document->SetModified(true);
    UpdateUI();
    ScheduleAutoHydro();

    LogCAD(std::string("FloorCopy: ") + std::to_string(oldToNew.size()) +
           " node, " + std::to_string(copiedEdges) + " boru kopyalandi.");
    statusBar()->showMessage(
        QString("Tesisat kopyalandi: %1 node, %2 boru (%3 m → %4 m)")
        .arg((int)oldToNew.size()).arg(copiedEdges)
        .arg(srcZ, 0, 'f', 2).arg(dstZ, 0, 'f', 2));
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

    // Mesafe ölçüm modu (UZAKLIK/DISTANCE komutu)
    if (m_measureMode) {
        if (!m_measureHasFirstPt) {
            m_measurePt1 = geom::Vec3(worldX, worldY, 0.0);
            m_measureHasFirstPt = true;
            statusBar()->showMessage("Mesafe: İkinci nokta seçin");
            if (m_commandBar) m_commandBar->SetPrompt("2. Nokta");
        } else {
            geom::Vec3 pt2(worldX, worldY, 0.0);
            double dist = m_measurePt1.DistanceTo(pt2);
            QString msg = QString("Mesafe: %1 mm  (%2 m)")
                .arg(dist, 0, 'f', 2)
                .arg(dist / 1000.0, 0, 'f', 3);
            statusBar()->showMessage(msg);
            if (m_logList) m_logList->addItem(msg);
            m_measureMode = false;
            m_measureHasFirstPt = false;
            if (m_commandBar) m_commandBar->SetPrompt("Komut");
        }
        return;
    }

    auto& network = m_document->GetNetwork();

    switch (m_currentToolMode) {
    case ToolMode::DrawPipe: {
        if (m_drawState == DrawState::WaitingFirstPoint || m_drawState == DrawState::Idle) {
            // İlk nokta: sadece konumu kaydet, henüz graph'a ekleme
            m_firstClickPos = geom::Vec3(worldX, worldY, 0.0);
            m_firstNodeInGraph = false;
            m_drawState = DrawState::WaitingSecondPoint;
            // Rubber-band başlangıcı için ekran koordinatı
            const auto& vp = m_vulkanWindow->GetViewport();
            geom::Vec3 sp = vp.WorldToScreen(m_firstClickPos);
            m_firstClickScreen = QPoint(static_cast<int>(sp.x), static_cast<int>(sp.y));
            statusBar()->showMessage(QString("Boru: İkinci noktayı tıklayın (ESC = iptal)"));
        } else if (m_drawState == DrawState::WaitingSecondPoint) {
            // Mevcut boru malzeme ayarları
            const std::string material = "PVC";
            const double roughness = 0.0015;
            const double diameter  = 20.0;

            geom::Vec3 secondPos(worldX, worldY, 0.0);
            double length_m = m_firstClickPos.DistanceTo(secondPos) / 1000.0; // mm → m
            if (length_m < 1e-6) length_m = 0.001;

            // CompositeCommand: atomic undo/redo
            auto composite = std::make_unique<core::CompositeCommand>();

            // Eğer ilk node daha graph'ta değilse ekle
            if (!m_firstNodeInGraph) {
                mep::Node firstNode;
                firstNode.type = mep::NodeType::Junction;
                firstNode.position = m_firstClickPos;
                firstNode.label = "J";
                auto addFirst = std::make_unique<core::AddNodeCommand>(network, firstNode);
                addFirst->Execute();                // ID almak için hemen çalıştır
                m_firstNodeId = addFirst->GetNodeId();
                composite->AddCommand(std::move(addFirst));
                m_firstNodeInGraph = true;
            }

            // İkinci node
            mep::Node secondNode;
            secondNode.type = mep::NodeType::Junction;
            secondNode.position = secondPos;
            secondNode.label = "J";
            auto addSecond = std::make_unique<core::AddNodeCommand>(network, secondNode);
            addSecond->Execute();
            uint32_t secondNodeId = addSecond->GetNodeId();
            composite->AddCommand(std::move(addSecond));

            // Edge
            mep::Edge edge;
            edge.nodeA = m_firstNodeId;
            edge.nodeB = secondNodeId;
            edge.type = m_currentPipeType;
            edge.diameter_mm = diameter;
            edge.roughness_mm = roughness;
            edge.material = material;
            edge.length_m = length_m;
            auto addEdge = std::make_unique<core::AddEdgeCommand>(network, edge);
            addEdge->Execute();
            composite->AddCommand(std::move(addEdge));

            // Composite'i stack'e kaydet (Execute zaten yapıldı, sadece track et)
            m_document->TrackExecuted(std::move(composite));
            m_document->SetModified(true);
            UpdateUI();
            ScheduleAutoHydro(); // boru eklendi → debounced DN güncelle

            // Zincirleme: ikinci nokta bir sonraki borunun başlangıcı
            m_firstNodeId = secondNodeId;
            m_firstNodeInGraph = true;
            m_firstClickPos = secondPos;
            const auto& vp2 = m_vulkanWindow->GetViewport();
            geom::Vec3 sp2 = vp2.WorldToScreen(secondPos);
            m_firstClickScreen = QPoint(static_cast<int>(sp2.x), static_cast<int>(sp2.y));

            statusBar()->showMessage(QString("Boru eklendi (L=%1m). Sonraki noktayı tıklayın veya ESC")
                .arg(length_m, 0, 'f', 2));
        }
        break;
    }
    case ToolMode::PlaceFixture: {
        mep::Node node;
        node.type = mep::NodeType::Fixture;
        node.position = geom::Vec3(worldX, worldY, 0.0);
        std::string typeName = m_selectedFixtureType.toStdString();
        node.fixtureType = typeName;
        node.label = typeName;
        auto& db = mep::Database::Instance();
        auto fixture = db.GetFixture(typeName);
        node.loadUnit = fixture.loadUnit;
        auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
        m_document->ExecuteCommand(std::move(cmd));
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro(); // armatür eklendi → LU değişti → DN yeniden hesapla
        statusBar()->showMessage(QString("Armatür eklendi: %1 (x=%2, y=%3)")
            .arg(m_selectedFixtureType).arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::PlaceJunction: {
        mep::Node node;
        node.type = mep::NodeType::Junction;
        node.position = geom::Vec3(worldX, worldY, 0.0);
        node.label = "J";
        auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
        m_document->ExecuteCommand(std::move(cmd));
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("Bağlantı noktası eklendi (x=%1, y=%2)")
            .arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::PlaceDrain: {
        mep::Node node;
        node.type     = mep::NodeType::Drain;
        node.position = geom::Vec3(worldX, worldY, 0.0);
        node.label    = m_drainLabel.toStdString();
        auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
        m_document->ExecuteCommand(std::move(cmd));
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
        statusBar()->showMessage(QString("%1 eklendi (x=%2, y=%3)")
            .arg(m_drainLabel).arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::ConnectFixture: {
        if (m_connectFixtureNodeId == 0) {
            // 1. Adim: en yakin Fixture node'u sec
            constexpr double SNAP = 50.0;
            uint32_t bestId = 0;
            double bestD2 = SNAP * SNAP;
            for (const auto& [nid, node] : network.GetNodeMap()) {
                if (node.type != mep::NodeType::Fixture) continue;
                double dx = node.position.x - worldX;
                double dy = node.position.y - worldY;
                double d2 = dx*dx + dy*dy;
                if (d2 < bestD2) { bestD2 = d2; bestId = nid; }
            }
            if (bestId == 0) {
                statusBar()->showMessage("BAGLA: Armatur bulunamadi — armatur konumuna daha yakin tiklayin");
                break;
            }
            m_connectFixtureNodeId = bestId;
            const mep::Node* fn = network.GetNode(bestId);
            statusBar()->showMessage(QString("BAGLA: [%1] secildi — boru hattini tiklayin (2/2)")
                .arg(fn ? QString::fromStdString(fn->fixtureType) : "?"));
            if (m_commandBar) m_commandBar->SetPrompt("Boru hatti");
        } else {
            // 2. Adim: en yakin edge'i bul, dis noktadan dik ayak cikar, dal boru ekle
            constexpr double SNAP_EDGE = 100.0;
            uint32_t bestEdgeId = 0;
            double bestDist = SNAP_EDGE;

            for (const auto& [eid, edge] : network.GetEdgeMap()) {
                const mep::Node* nA = network.GetNode(edge.nodeA);
                const mep::Node* nB = network.GetNode(edge.nodeB);
                if (!nA || !nB) continue;
                double ex = nB->position.x - nA->position.x;
                double ey = nB->position.y - nA->position.y;
                double len2 = ex*ex + ey*ey;
                double dist;
                if (len2 < 1e-9) {
                    double dx = nA->position.x - worldX, dy = nA->position.y - worldY;
                    dist = std::sqrt(dx*dx + dy*dy);
                } else {
                    double t = ((worldX - nA->position.x)*ex + (worldY - nA->position.y)*ey) / len2;
                    t = std::max(0.0, std::min(1.0, t));
                    double px = nA->position.x + t*ex, py = nA->position.y + t*ey;
                    double dx = px - worldX, dy = py - worldY;
                    dist = std::sqrt(dx*dx + dy*dy);
                }
                if (dist < bestDist) { bestDist = dist; bestEdgeId = eid; }
            }

            if (bestEdgeId == 0) {
                statusBar()->showMessage("BAGLA: Boru bulunamadi — boru hattina daha yakin tiklayin");
                break;
            }

            const mep::Edge* srcEdge = network.GetEdge(bestEdgeId);
            const mep::Node* nA = network.GetNode(srcEdge->nodeA);
            const mep::Node* nB = network.GetNode(srcEdge->nodeB);
            const mep::Node* fixtureNode = network.GetNode(m_connectFixtureNodeId);
            if (!nA || !nB || !fixtureNode) { m_connectFixtureNodeId = 0; break; }

            // Dik ayak noktasi hesapla (en yakin nokta boru uzerinde)
            double ex = nB->position.x - nA->position.x;
            double ey = nB->position.y - nA->position.y;
            double len2 = ex*ex + ey*ey;
            double t = 0.5;
            if (len2 > 1e-9) {
                t = ((fixtureNode->position.x - nA->position.x)*ex
                   + (fixtureNode->position.y - nA->position.y)*ey) / len2;
                t = std::max(0.05, std::min(0.95, t)); // uca cok yakin olmasin
            }
            geom::Vec3 footPos{
                nA->position.x + t * ex,
                nA->position.y + t * ey,
                fixtureNode->position.z
            };

            auto composite = std::make_unique<core::CompositeCommand>();

            // Junction boru uzerinde
            mep::Node jNode;
            jNode.type     = mep::NodeType::Junction;
            jNode.position = footPos;
            jNode.label    = "J";
            auto addJ = std::make_unique<core::AddNodeCommand>(network, jNode);
            addJ->Execute();
            uint32_t jId = addJ->GetNodeId();
            composite->AddCommand(std::move(addJ));

            // Dal boru: fixture → junction
            mep::Edge branchEdge;
            branchEdge.nodeA = m_connectFixtureNodeId;
            branchEdge.nodeB = jId;
            branchEdge.type  = srcEdge->type;
            branchEdge.diameter_mm = srcEdge->diameter_mm;
            branchEdge.roughness_mm = srcEdge->roughness_mm;
            branchEdge.material = srcEdge->material;
            double dx = footPos.x - fixtureNode->position.x;
            double dy = footPos.y - fixtureNode->position.y;
            branchEdge.length_m = std::sqrt(dx*dx + dy*dy) / 1000.0;
            if (branchEdge.length_m < 0.001) branchEdge.length_m = 0.001;
            auto addBranch = std::make_unique<core::AddEdgeCommand>(network, branchEdge);
            addBranch->Execute();
            composite->AddCommand(std::move(addBranch));

            m_document->TrackExecuted(std::move(composite));
            m_document->SetModified(true);
            m_connectFixtureNodeId = 0;
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("BAGLA: Dal boru eklendi (L=%1 m)")
                .arg(branchEdge.length_m, 0, 'f', 2));
            // Ardindan bir sonraki armaturu baglayabilmek icin mod devam eder
        }
        break;
    }
    case ToolMode::Select:
    default: {
        const double NODE_SNAP = 10.0; // world unit
        const double EDGE_SNAP = 5.0;

        // Node picking: nearest within snap radius
        uint32_t closestNodeId = 0;
        double   closestNodeDist = NODE_SNAP * NODE_SNAP;

        for (const auto& [id, node] : network.GetNodeMap()) {
            double dx = node.position.x - worldX;
            double dy = node.position.y - worldY;
            double d2 = dx*dx + dy*dy;
            if (d2 < closestNodeDist) {
                closestNodeDist = d2;
                closestNodeId   = id;
            }
        }

        if (closestNodeId != 0) {
            OnNodeSelected(closestNodeId);
            break;
        }

        // Edge picking: point-to-segment distance in world space
        uint32_t closestEdgeId   = 0;
        double   closestEdgeDist = EDGE_SNAP;

        for (const auto& [id, edge] : network.GetEdgeMap()) {
            const mep::Node* nA = network.GetNode(edge.nodeA);
            const mep::Node* nB = network.GetNode(edge.nodeB);
            if (!nA || !nB) continue;

            double ex   = nB->position.x - nA->position.x;
            double ey   = nB->position.y - nA->position.y;
            double len2 = ex*ex + ey*ey;
            double dist;
            if (len2 < 1e-9) {
                double dx = nA->position.x - worldX, dy = nA->position.y - worldY;
                dist = std::sqrt(dx*dx + dy*dy);
            } else {
                double t  = ((worldX - nA->position.x)*ex + (worldY - nA->position.y)*ey) / len2;
                t         = std::max(0.0, std::min(1.0, t));
                double px = nA->position.x + t*ex;
                double py = nA->position.y + t*ey;
                double dx = px - worldX, dy = py - worldY;
                dist = std::sqrt(dx*dx + dy*dy);
            }
            if (dist < closestEdgeDist) {
                closestEdgeDist = dist;
                closestEdgeId   = id;
            }
        }

        if (closestEdgeId != 0) {
            m_selectedNodeId = 0;
            m_selectedEdgeId = closestEdgeId;
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
            const mep::Edge* edge = network.GetEdge(closestEdgeId);
            if (m_fixturePanel) m_fixturePanel->Clear();
            statusBar()->showMessage(QString("Kenar #%1 seçildi — Çap:%2mm, Malzeme:%3")
                .arg(closestEdgeId)
                .arg(edge ? edge->diameter_mm : 0.0, 0, 'f', 1)
                .arg(edge ? QString::fromStdString(edge->material) : ""));
        } else {
            m_selectedNodeId = 0;
            m_selectedEdgeId = 0;
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
            if (m_fixturePanel) m_fixturePanel->Clear();
            statusBar()->showMessage(QString("Konum: x=%1, y=%2")
                .arg(worldX, 0, 'f', 3).arg(worldY, 0, 'f', 3));
        }
        break;
    }
    } // switch
} // HandleMousePress

void MainWindow::HandleMouseMove(double worldX, double worldY) {
    statusBar()->showMessage(QString("x=%1  y=%2")
        .arg(worldX, 0, 'f', 3).arg(worldY, 0, 'f', 3));

    if (!m_snapOverlay) return;

    // Overlay boyutunu container ile senkronize tut
    if (m_vulkanContainer &&
        m_snapOverlay->size() != m_vulkanContainer->size()) {
        m_snapOverlay->resize(m_vulkanContainer->size());
    }

    // Çizim modunda değilse overlay'i gizle
    if (m_currentToolMode == ToolMode::Select) {
        m_snapOverlay->Hide();
        return;
    }

    // World → screen koordinat dönüşümü
    const auto& vp = m_vulkanWindow->GetViewport();
    geom::Vec3 screenPt = vp.WorldToScreen({worldX, worldY, 0.0});
    QPoint screenPos(static_cast<int>(screenPt.x), static_cast<int>(screenPt.y));

    // Snap hesabı (CAD entity varsa)
    cad::SnapResult snap;
    if (m_document && !m_document->GetCADEntities().empty()) {
        std::vector<cad::Entity*> entityPtrs;
        entityPtrs.reserve(m_document->GetCADEntities().size());
        for (const auto& e : m_document->GetCADEntities())
            entityPtrs.push_back(e.get());
        snap = m_snapManager.FindSnapPoint({worldX, worldY, 0.0},
                                           entityPtrs,
                                           vp.GetZoom());
    }

    m_snapOverlay->Update(screenPos, snap);

    // Rubber-band: DrawPipe ikinci nokta bekliyorsa çizgi göster
    if (m_currentToolMode == ToolMode::DrawPipe &&
        m_drawState == DrawState::WaitingSecondPoint) {
        m_snapOverlay->SetRubberBand(m_firstClickScreen, screenPos, true);
    } else {
        m_snapOverlay->ClearRubberBand();
    }
}

// ═══════════════════════════════════════════════════════════
//  NODE (ARMATÜR) SEÇİMİ
// ═══════════════════════════════════════════════════════════

void MainWindow::OnNodeSelected(uint32_t nodeId) {
    if (!m_document || !m_fixturePanel) return;

    m_selectedNodeId = nodeId;
    m_selectedEdgeId = 0;

    // Fixture ve Source/Drain node'ları için panel göster
    const mep::Node* node = m_document->GetNetwork().GetNode(nodeId);
    if (!node) { m_fixturePanel->Clear(); return; }

    // Gizmo + PBR material: seçili node'a göre güncelle
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) {
        auto* r = m_vulkanWindow->GetRenderer();
        r->GetGizmo().SetPosition(node->position);
        r->GetGizmo().SetMode(render::GizmoMode::Translate);
        r->SetGizmoVisible(true);

        // PBR: bağlı ilk Supply borusunun malzeme parametrelerini uygula
        for (uint32_t eid : m_document->GetNetwork().GetConnectedEdges(nodeId)) {
            const mep::Edge* e = m_document->GetNetwork().GetEdge(eid);
            if (e && e->type == mep::EdgeType::Supply) {
                mep::MaterialProperties mp = mep::GetPipeMaterial(e->material);
                r->SetCompositeMaterial(mp.roughness, mp.metallic);
                break;
            }
        }
    }

    if (node->type == mep::NodeType::Fixture ||
        node->type == mep::NodeType::Source   ||
        node->type == mep::NodeType::Drain     ||
        node->type == mep::NodeType::Pump      ||
        node->type == mep::NodeType::Tank) {

        m_fixtureDock->raise(); // Armatür sekmesini öne al
        m_fixturePanel->LoadNode(nodeId,
                                 &m_document->GetNetwork(),
                                 &m_document->GetFloorManager());
    } else {
        m_fixturePanel->Clear();
    }
}

void MainWindow::OnCommandEntered(const QString& cmd) {
    // Komut adını büyük harfe çevir (zaten CommandBar yapar ama güvenlik için)
    QString c = cmd.trimmed().toUpper();

    if (c == "HELP") {
        if (m_logList) {
            m_logList->addItem("Komutlar: LINE/PIPE  FIXTURE  JUNCTION  SOURCE  DRAIN");
            m_logList->addItem("Baglama : BAGLA/CONNECT  (armaturu boru hattina bagla)");
            m_logList->addItem("Pis Su  : PIS-SU  YER-SUZGECI  ROGAR/BOSALTMA");
            m_logList->addItem("Gorunum : ZOOM-EXTENTS  VIEW-PLAN  VIEW-ISO");
            m_logList->addItem("Analiz  : HYDRAULICS  HIDROFOR  NORM  YAGMUR  BOM");
            m_logList->addItem("Diger   : UNDO  REDO  SAVE  EXPORT-DXF  UZAKLIK  MIMARI");
        }
    } else if (c == "BAGLA" || c == "CONNECT") {
        OnConnectFixture();
    } else if (c == "HIDROFOR" || c == "PUMP-SIZE") {
        OnHidrofor();
    } else if (c == "NORM" || c == "NORM-SEC") {
        OnNormSelection();
    } else if (c == "YAGMUR" || c == "RAINWATER") {
        OnYagmurSuyu();
    } else if (c == "BOM" || c == "KESIF") {
        OnBOM();
    } else if (c == "UZAKLIK" || c == "DISTANCE" || c == "DIST") {
        m_measureMode = true;
        m_measureHasFirstPt = false;
        statusBar()->showMessage("Mesafe: İlk nokta seçin");
        if (m_commandBar) m_commandBar->SetPrompt("1. Nokta");
    } else if (c == "MIMARI") {
        OnMimariBelirle();
    } else if (c == "PIPE" || c == "LINE") {
        m_currentPipeType = mep::EdgeType::Supply;
        OnDrawPipe();
        if (m_commandBar) m_commandBar->SetPrompt("Boru başlangıcı");
    } else if (c == "FIXTURE") {
        OnDrawFixture();
        if (m_commandBar) m_commandBar->SetPrompt("Armatür konumu");
    } else if (c == "JUNCTION") {
        OnDrawJunction();
        if (m_commandBar) m_commandBar->SetPrompt("Bağlantı noktası");
    } else if (c == "PIS-SU" || c == "DRAINAGE-PIPE" || c == "DRAIN-PIPE") {
        OnDrawDrainPipe();
    } else if (c == "YER-SUZGECI" || c == "FLOOR-DRAIN") {
        OnPlaceYerSuzgeci();
    } else if (c == "ROGAR" || c == "BOSALTMA") {
        OnPlaceRogar();
    } else if (c == "KOPYA-KAT" || c == "FLOOR-COPY") {
        OnCopyFloor();
    } else if (c == "ZOOM-EXTENTS" || c == "ZE") {
        OnZoomExtents();
    } else if (c == "VIEW-PLAN") {
        OnPlanView();
    } else if (c == "VIEW-ISO") {
        OnIsometricView();
    } else if (c == "HYDRAULICS") {
        OnRunHydraulics();
    } else if (c == "SCHEDULE") {
        OnGenerateSchedule();
    } else if (c == "UNDO") {
        OnUndo();
    } else if (c == "REDO") {
        OnRedo();
    } else if (c == "SAVE") {
        OnSave();
    } else if (c == "NEW") {
        OnNew();
    } else if (c == "OPEN") {
        OnOpen();
    } else if (c == "EXPORT-DXF" || c == "EXPORT-PDF") {
        OnExportReport();
    } else {
        statusBar()->showMessage(QString("Bilinmeyen komut: %1  (HELP yazın)").arg(cmd));
        if (m_commandBar) m_commandBar->SetPrompt("Komut");
    }
}

void MainWindow::RefreshTextOverlay() {
    if (!m_snapOverlay || !m_document || !m_vulkanWindow) return;

    const cad::Viewport& vp = m_vulkanWindow->GetViewport();
    const auto& layerMap    = m_document->GetLayers();

    std::vector<SnapOverlay::TextLabel> labels;
    labels.reserve(128);

    // ── CAD Text / MText entity'leri (DXF/DWG import) ────────
    for (const auto& ent : m_document->GetCADEntities()) {
        if (!ent) continue;
        if (ent->GetType() != cad::EntityType::Text &&
            ent->GetType() != cad::EntityType::MText) continue;

        const auto* txt = static_cast<const cad::Text*>(ent.get());
        const std::string& content = txt->GetText();
        if (content.empty()) continue;

        geom::Vec3 sp = vp.WorldToScreen(txt->GetEffectiveInsertPoint());
        if (sp.x < -200 || sp.x > vp.GetWidth() + 200 ||
            sp.y < -200 || sp.y > vp.GetHeight() + 200) continue;

        double heightPx = txt->GetHeight() * vp.GetZoom();
        if (heightPx < 3.0) continue;

        cad::Color col = ent->GetColor();
        QColor qcol;
        if (col.a == 0) {
            auto it = layerMap.find(ent->GetLayerName());
            qcol = (it != layerMap.end())
                ? QColor(it->second.GetColor().r, it->second.GetColor().g, it->second.GetColor().b)
                : Qt::white;
        } else {
            qcol = QColor(col.r, col.g, col.b);
        }

        SnapOverlay::TextLabel lbl;
        lbl.pos        = QPointF(sp.x, sp.y);
        lbl.text       = QString::fromStdString(content);
        lbl.pixelH     = static_cast<int>(heightPx);
        lbl.color      = qcol;
        lbl.rotDeg     = txt->GetRotationDeg();
        lbl.hAlign     = txt->GetHAlign();
        lbl.vAlign     = txt->GetVAlign();
        lbl.maxWidthPx = txt->GetRectWidth() * vp.GetZoom(); // 0 → sarma yok
        labels.push_back(std::move(lbl));
    }

    // ── MEP Edge label'ları (DN boyutu, boru etiketi) ─────────
    const auto& network = m_document->GetNetwork();
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.label.empty()) continue;

        const mep::Node* nA = network.GetNode(edge.nodeA);
        const mep::Node* nB = network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        // Borunun orta noktası (world)
        geom::Vec3 mid{
            (nA->position.x + nB->position.x) * 0.5,
            (nA->position.y + nB->position.y) * 0.5,
            0.0
        };
        geom::Vec3 sp = vp.WorldToScreen(mid);
        if (sp.x < -40 || sp.x > vp.GetWidth() + 40 ||
            sp.y < -40 || sp.y > vp.GetHeight() + 40) continue;

        // Minimum zoom: çok küçük ölçekte gösterme
        if (vp.GetZoom() < 0.3) continue;

        SnapOverlay::TextLabel lbl;
        lbl.pos    = QPointF(sp.x + 4, sp.y - 4); // hafif offset, borudan kaçsın
        lbl.text   = QString::fromStdString(edge.label);
        lbl.pixelH = 11;
        lbl.color  = QColor(100, 220, 255); // açık mavi — boru rengiyle uyumlu
        lbl.rotDeg = 0.0;
        lbl.hAlign = 0; // sol hizalı
        lbl.vAlign = 0;
        labels.push_back(std::move(lbl));
    }

    // ── MEP Node label'ları (armatür tipi) ─ collision avoidance ile ──────
    {
        // Yerleştirilen bounding rect'leri tut — çakışma varsa yukarı kaydır
        std::vector<QRectF> placedRects;

        for (const auto& [nid, node] : network.GetNodeMap()) {
            if (node.label.empty() || node.type == mep::NodeType::Junction) continue;

            geom::Vec3 sp = vp.WorldToScreen(node.position);
            if (sp.x < -40 || sp.x > vp.GetWidth() + 40 ||
                sp.y < -40 || sp.y > vp.GetHeight() + 40) continue;
            if (vp.GetZoom() < 0.3) continue;

            QColor qcol;
            switch (node.type) {
                case mep::NodeType::Fixture: qcol = QColor(100, 255, 120); break;
                case mep::NodeType::Source:  qcol = QColor(100, 160, 255); break;
                case mep::NodeType::Drain:   qcol = QColor(210, 140,  80); break;
                case mep::NodeType::Pump:    qcol = QColor(255, 220,  50); break;
                default:                     qcol = Qt::white;              break;
            }

            constexpr int kLblH = 10;
            constexpr int kLblW = 50; // yaklaşık genişlik tahmini
            QPointF base(sp.x + 6, sp.y - 6);

            // Greedy placement: en fazla 4 kez yukarı kaydır
            QRectF r(base.x(), base.y() - kLblH, kLblW, kLblH + 2);
            for (int attempt = 0; attempt < 4; ++attempt) {
                bool clash = false;
                for (const auto& pr : placedRects)
                    if (r.intersects(pr)) { clash = true; break; }
                if (!clash) break;
                r.moveTop(r.top() - (kLblH + 3));
            }
            placedRects.push_back(r);

            SnapOverlay::TextLabel lbl;
            lbl.pos    = QPointF(r.left(), r.bottom() - 2);
            lbl.text   = QString::fromStdString(node.label);
            lbl.pixelH = kLblH;
            lbl.color  = qcol;
            lbl.rotDeg = 0.0;
            lbl.hAlign = 0;
            lbl.vAlign = 0;
            labels.push_back(std::move(lbl));
        }
    }

    m_snapOverlay->SetTextLabels(std::move(labels));

    // ── Açık uç (bağlantısız / dead-end) node uyarıları ──────
    {
        std::vector<QPoint> openEndPts;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            auto edges = network.GetConnectedEdges(nid);
            bool isOpenEnd = false;
            if (edges.empty()) {
                // Hiç boru bağlı değil — kesinlikle hata
                isOpenEnd = true;
            } else if (edges.size() == 1 &&
                       node.type == mep::NodeType::Junction) {
                // Tek kenar bağlı Junction = dead-end boru ucu
                isOpenEnd = true;
            }
            if (isOpenEnd) {
                geom::Vec3 sp = vp.WorldToScreen(node.position);
                if (sp.x >= -20 && sp.x <= vp.GetWidth() + 20 &&
                    sp.y >= -20 && sp.y <= vp.GetHeight() + 20)
                    openEndPts.emplace_back(static_cast<int>(sp.x),
                                            static_cast<int>(sp.y));
            }
        }
        m_snapOverlay->SetOpenEndMarkers(std::move(openEndPts));
    }
}

void MainWindow::OnCommandEscape() {
    m_drawState        = DrawState::Idle;
    m_currentToolMode  = ToolMode::Select;
    m_firstNodeInGraph = false;
    m_measureMode      = false;
    m_measureHasFirstPt = false;
    m_currentPipeType  = mep::EdgeType::Supply; // pis su modunu sıfırla
    m_connectFixtureNodeId = 0;
    if (m_commandBar) m_commandBar->SetPrompt("Komut");
    if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }
    statusBar()->showMessage("İptal edildi — Seçim modu");
}

// ============================================================
//  CIHAZLARI TESİSATA BAGLA (FineSANI BAGLA eşdeğeri)
// ============================================================
void MainWindow::OnConnectFixture() {
    m_currentToolMode      = ToolMode::ConnectFixture;
    m_drawState            = DrawState::WaitingFirstPoint;
    m_connectFixtureNodeId = 0;
    statusBar()->showMessage("BAGLA: Armaturu tiklayin (1/2)");
    if (m_commandBar) m_commandBar->SetPrompt("Armatur sec");
}

// ============================================================
//  HİDROFOR BOYUTLANDIRMA DİYALOGU
// ============================================================
void MainWindow::OnHidrofor() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Hidrofor Boyutlandirma",
            "Once tesisat sebekesi cizin (boru + kaynak + armaturler).");
        return;
    }

    mep::HydraulicSolver solver(network);
    solver.Solve();
    auto result = solver.CalculateCriticalPath();

    QString msg;
    msg += QString("<h3>Hidrofor / Pompa Boyutlandirma</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td><b>Kritik devre kaybi:</b></td><td><font color='red'><b>%1 mSS</b></font></td></tr>")
               .arg(result.totalHeadLoss_m, 0, 'f', 2);
    msg += QString("<tr><td><b>Gerekli pompa basma yuksekligi:</b></td><td><font color='red'><b>%1 mSS</b></font></td></tr>")
               .arg(result.requiredPumpHead_m, 0, 'f', 2);
    msg += QString("<tr><td><b>Gerekli debi:</b></td><td>%1 m³/h</td></tr>")
               .arg(result.requiredFlow_m3h, 0, 'f', 2);
    msg += QString("</table><hr>");
    msg += QString("<b>Onerilen Ekipman:</b><br>");
    msg += QString("Model: %1<br>").arg(QString::fromStdString(result.suggestedPumpModel));
    msg += QString("Maks. basinc: %1 mSS<br>").arg(result.suggestedPumpHead_m, 0, 'f', 1);
    msg += QString("Maks. debi: %1 m³/h<br>").arg(result.suggestedPumpFlow_m3h, 0, 'f', 1);
    msg += QString("Guc: %1 kW").arg(result.suggestedPumpPower_kW, 0, 'f', 2);

    QMessageBox dlg(this);
    dlg.setWindowTitle("Hidrofor Boyutlandirma (TS EN 806-3)");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(QMessageBox::Information);
    dlg.exec();

    if (m_logList) {
        m_logList->addItem(QString("Hidrofor: %1 mSS / %2 m3/h → %3")
            .arg(result.requiredPumpHead_m, 0, 'f', 2)
            .arg(result.requiredFlow_m3h, 0, 'f', 2)
            .arg(QString::fromStdString(result.suggestedPumpModel)));
    }
}

// ============================================================
//  HESAP NORMU SEÇİMİ (EN 806-3 / DIN 1988-300)
// ============================================================
void MainWindow::OnNormSelection() {
    QStringList norms;
    norms << "TS EN 806-3  (Avrupa / Turkiye standardi — varsayilan)"
          << "DIN 1988-300  (Alman standardi — esszamanlilik katsayili)";

    bool isEN = (mep::HydraulicSolver::GlobalNorm() == mep::HydroNorm::EN806_3);
    int current = isEN ? 0 : 1;

    bool ok = false;
    QString chosen = QInputDialog::getItem(this,
        "Hesap Normu Secimi",
        "Besleme debisi hesaplama standardi:",
        norms, current, false, &ok);
    if (!ok) return;

    if (chosen.startsWith("DIN")) {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
        statusBar()->showMessage("Hesap normu: DIN 1988-300 (esszamanlilik katsayili)");
        if (m_logList) m_logList->addItem("Norm degistirildi: DIN 1988-300");
    } else {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;
        statusBar()->showMessage("Hesap normu: TS EN 806-3");
        if (m_logList) m_logList->addItem("Norm degistirildi: TS EN 806-3");
    }
    ScheduleAutoHydro(); // normu guncelledik → yeniden hesapla
}

// ============================================================
//  YAGMUR SUYU MODULU (EN 12056-3)
// ============================================================
void MainWindow::OnYagmurSuyu() {
    // Alan girisi
    bool ok = false;
    double area = QInputDialog::getDouble(this,
        "Yagmur Suyu — Cati Alani",
        "Tahliye edilecek cati/zemin alani (m²):",
        100.0, 0.1, 100000.0, 1, &ok);
    if (!ok) return;

    QStringList surfaceTypes;
    surfaceTypes << "Cati (C=1.0)"
                 << "Beton/Asfalt (C=0.9)"
                 << "Yeşil çati (C=0.5)"
                 << "Çakilli zemin (C=0.6)";
    QString surface = QInputDialog::getItem(this,
        "Yuzey Tipi", "Yuzey/ drenaj tipi:", surfaceTypes, 0, false, &ok);
    if (!ok) return;

    double C = 1.0;
    if (surface.contains("Beton")) C = 0.9;
    else if (surface.contains("Yeşil") || surface.contains("yesil")) C = 0.5;
    else if (surface.contains("akilli") || surface.contains("Çakil")) C = 0.6;

    // EN 12056-3: tasarim yagmur yuku r_D = 0.03 l/(s*m²) — Turkiye 2-yillik
    double rD = 0.03;
    double Q_ls = C * area * rD; // l/s

    // Standart drenaj DN secimi (Manning, %2 egim)
    static const struct { double dn; double cap_ls; } kTable[] = {
        {50, 1.2}, {75, 3.5}, {100, 8.0}, {125, 15.0},
        {150, 25.0}, {200, 50.0}, {0, 0}
    };
    double selectedDN = 200.0;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= Q_ls) { selectedDN = kTable[i].dn; break; }
    }

    // Yagmurluk borusu sayisi (tek boru maks 50 l/s kabul)
    int numPipes = (Q_ls > 50.0) ? (int)std::ceil(Q_ls / 50.0) : 1;
    double qPerPipe = Q_ls / numPipes;
    // Her boru icin DN yeniden sec
    double dnPerPipe = 200.0;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= qPerPipe) { dnPerPipe = kTable[i].dn; break; }
    }

    QString msg;
    msg += QString("<h3>Yagmur Suyu Tahliye Hesabi (EN 12056-3)</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td>Alan:</td><td><b>%1 m²</b></td></tr>").arg(area, 0, 'f', 1);
    msg += QString("<tr><td>Yuzey katsayisi (C):</td><td>%1</td></tr>").arg(C, 0, 'f', 2);
    msg += QString("<tr><td>Tasarim yagmur yuku (r_D):</td><td>%1 l/(s·m²)</td></tr>").arg(rD, 0, 'f', 3);
    msg += QString("<tr><td><b>Tasarim debisi (Q):</b></td><td><font color='red'><b>%1 l/s</b></font></td></tr>").arg(Q_ls, 0, 'f', 2);
    msg += QString("<tr><td>Onerilen boru:</td><td><b>%1x DN%2</b></td></tr>").arg(numPipes).arg((int)dnPerPipe);
    msg += QString("</table><hr>");
    msg += QString("<small>r_D = 0.03 l/(s·m²) — Turkiye iklim bolgesi (2 yillik donus periyodu)<br>");
    msg += QString("Egim: %%2 min — Manning n=0.012 (PVC)</small>");

    QMessageBox dlg(this);
    dlg.setWindowTitle("Yagmur Suyu Modulu (EN 12056-3)");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(QMessageBox::Information);
    dlg.exec();

    if (m_logList) {
        m_logList->addItem(QString("Yagmur Suyu: A=%1m2, Q=%2l/s → %3x DN%4")
            .arg(area,0,'f',0).arg(Q_ls,0,'f',2).arg(numPipes).arg((int)dnPerPipe));
    }
}

// ============================================================
//  KEŞİF LİSTESİ / BOM (Bill of Materials)
// ============================================================
void MainWindow::OnBOM() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Kesif Listesi", "Oncelikle tesisat sebekesi cizin.");
        return;
    }

    // Boru metrajlari — DN'e gore grupla
    std::map<int, double> pipeLength_m; // DN → toplam uzunluk
    std::map<int, int>    pipeCount;    // DN → adet
    std::map<std::string, std::map<int, double>> byMaterial; // material → {DN → length}

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        int dn = static_cast<int>(std::round(edge.diameter_mm));
        pipeLength_m[dn] += edge.length_m;
        pipeCount[dn]++;
        byMaterial[edge.material][dn] += edge.length_m;
    }

    // Baglanti elemanlari — node derecesinden tur tahmini
    int nTee    = 0; // T-parcasi: degree == 3
    int nElbow  = 0; // dirsek: degree == 2 (junction)
    int nFixture = 0;
    int nSource  = 0;
    int nDrain   = 0;

    for (const auto& [nid, node] : network.GetNodeMap()) {
        auto edges = network.GetConnectedEdges(nid);
        int deg = static_cast<int>(edges.size());
        switch (node.type) {
            case mep::NodeType::Fixture: ++nFixture; break;
            case mep::NodeType::Source:  ++nSource;  break;
            case mep::NodeType::Drain:   ++nDrain;   break;
            case mep::NodeType::Junction:
                if (deg >= 3) ++nTee;
                else if (deg == 2) ++nElbow;
                break;
            default: break;
        }
    }

    // Rapor olustur
    QString msg;
    msg += "<h3>Kesif Listesi / Malzeme Dokumu (BOM)</h3>";
    msg += "<b>Boru Metrajlari:</b><br>";
    msg += "<table border='1' cellspacing='0' cellpadding='3'>";
    msg += "<tr><th>DN (mm)</th><th>Toplam Boy (m)</th><th>Parca</th></tr>";

    double totalLength = 0.0;
    for (const auto& [dn, len] : pipeLength_m) {
        msg += QString("<tr><td>DN%1</td><td>%2</td><td>%3</td></tr>")
                   .arg(dn).arg(len, 0, 'f', 2).arg(pipeCount[dn]);
        totalLength += len;
    }
    msg += QString("<tr><td><b>TOPLAM</b></td><td><b>%1 m</b></td><td><b>%2 adet</b></td></tr>")
               .arg(totalLength, 0, 'f', 2).arg((int)network.GetEdgeCount());
    msg += "</table><br>";

    msg += "<b>Baglanti Elemanlari (tahmini):</b><br>";
    msg += "<table border='1' cellspacing='0' cellpadding='3'>";
    msg += "<tr><th>Eleman</th><th>Adet</th></tr>";
    msg += QString("<tr><td>T-parca</td><td>%1</td></tr>").arg(nTee);
    msg += QString("<tr><td>Dirsek</td><td>%1</td></tr>").arg(nElbow);
    msg += QString("<tr><td>Armatur baglantisi</td><td>%1</td></tr>").arg(nFixture);
    msg += QString("<tr><td>Kaynak (giris)</td><td>%1</td></tr>").arg(nSource);
    msg += QString("<tr><td>Tahliye noktasi</td><td>%1</td></tr>").arg(nDrain);
    msg += "</table>";

    QDialog dlg(this);
    dlg.setWindowTitle("Kesif Listesi / BOM");
    dlg.resize(500, 450);
    auto* layout = new QVBoxLayout(&dlg);
    auto* label = new QLabel(msg, &dlg);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    layout->addWidget(label);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(btn);

    // Log'a da yaz
    if (m_logList) {
        m_logList->addItem(QString("BOM: Toplam %1 m boru, %2 T, %3 dirsek")
            .arg(totalLength, 0, 'f', 2).arg(nTee).arg(nElbow));
        for (const auto& [dn, len] : pipeLength_m)
            m_logList->addItem(QString("  DN%1: %2 m").arg(dn).arg(len, 0, 'f', 2));
    }

    dlg.exec();
}

void MainWindow::OnNodeUpdated(uint32_t nodeId) {
    if (!m_document) return;
    // Renderer'ı ve status bar'ı güncelle
    if (m_vulkanWindow)
        m_vulkanWindow->requestUpdate();

    const mep::Node* node = m_document->GetNetwork().GetNode(nodeId);
    if (node) {
        statusBar()->showMessage(
            QString("Node %1 güncellendi: %2")
                .arg(nodeId)
                .arg(QString::fromStdString(node->label.empty()
                         ? node->fixtureType : node->label)));
    }
}

void MainWindow::OnPBRMaterialChanged(float roughness, float metalness, float ambient,
                                       float r, float g, float b,
                                       float lightAzimuth, float lightElevation) {
    auto* renderer = m_vulkanWindow ? m_vulkanWindow->GetRenderer() : nullptr;
    if (!renderer) return;

    renderer->SetCompositeMaterial(roughness, metalness);

    // Convert azimuth/elevation (degrees) to view-space light direction XYZ
    float azRad = lightAzimuth   * (3.14159265f / 180.0f);
    float elRad = lightElevation * (3.14159265f / 180.0f);
    float lx =  std::cos(elRad) * std::sin(azRad);
    float ly =  std::sin(elRad);
    float lz = -std::cos(elRad) * std::cos(azRad);
    float len = std::sqrt(lx*lx + ly*ly + lz*lz);
    if (len > 1e-4f) { lx /= len; ly /= len; lz /= len; }

    auto& pc = renderer->GetCompositePC();
    pc.lightDir[0] = lx;
    pc.lightDir[1] = ly;
    pc.lightDir[2] = lz;
    pc.ambient     = ambient;

    (void)r; (void)g; (void)b; // base color applies to individual pipes via SetCurrentMaterial

    if (m_vulkanWindow) m_vulkanWindow->requestUpdate();
}

} // namespace ui
} // namespace vkt
