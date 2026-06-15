/**
 * @file MainWindow.cpp
 * @brief Ana Pencere İmplementasyonu (FINE SANI++)
 */

#include "ui/MainWindow.hpp"
#include "ui/DXFImportDialog.hpp"
#include "ui/MimariBelirleDialog.hpp"
#include "ui/NewProjectDialog.hpp"
#include "ui/FloorAlignmentDialog.hpp"
#include "ui/PrintLayoutDialog.hpp"
#include "ui/PreflightCheckDialog.hpp"
#include "core/ProjectManager.hpp"
#include "core/DocxWriter.hpp"
#include "ui/SpacePanel.hpp"
#include "ui/CommandBar.hpp"
#include "ui/SnapOverlay.hpp"
#include "render/VulkanWindow.hpp"
#include "mep/HydraulicSolver.hpp"
#include "mep/RiserDiagram.hpp"
#include "mep/ScheduleGenerator.hpp"
#include "mep/Database.hpp"
#include "mep/XLSXWriter.hpp"
#include "mep/MaterialProperties.hpp"
#include "core/Application.hpp"
#include "core/Commands.hpp"
#include "cad/Text.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/DXFWriter.hpp"
#include "cad/Dimension.hpp"
#include "core/Version.hpp"
#include <QGuiApplication>
#include <QRegularExpression>
#include <filesystem>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QColorDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QTextBrowser>
#include <QTableWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QComboBox>
#include <QPrinter>
#include <QPainter>
#include <QPageLayout>
#include <QSvgRenderer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

static void LogCAD(const std::string& msg) {
#ifndef NDEBUG
    std::cout << "[MainWindow] " << msg << std::endl;
#else
    (void)msg;
#endif
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
    m_vulkanWindow->SetMouseReleaseCallback(
        [this](double wx, double wy, Qt::MouseButton btn) { HandleMouseRelease(wx, wy, btn); });
    // Viewport callback throttle: pan/zoom sırasında her frame yeniden çizmeyi önler
    m_vulkanWindow->SetViewportChangeCallback([this]() {
        using Clock = std::chrono::steady_clock;
        static auto s_lastOverlayRefresh = Clock::now();
        auto now = Clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastOverlayRefresh).count() >= 50) {
            s_lastOverlayRefresh = now;
            RefreshTextOverlay();
        }
    });
    // Çift tık orta tuş → Zoom Extents (AutoCAD standart kısayolu)
    m_vulkanWindow->SetZoomExtentsCallback([this]() { OnZoomExtents(); });

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
    // Kapatma sırasında callback'ler tetiklenmemeli (use-after-free riski)
    if (m_autoHydroTimer) m_autoHydroTimer->stop();
    if (m_vulkanWindow) {
        m_vulkanWindow->SetMousePressCallback({});
        m_vulkanWindow->SetMouseMoveCallback({});
        m_vulkanWindow->SetMouseReleaseCallback({});
        m_vulkanWindow->SetViewportChangeCallback({});
        m_vulkanWindow->SetZoomExtentsCallback({});
    }
    // m_vulkanWindow: createWindowContainer ownership'i alıyor, Qt cleanup ile silinir
    // m_snapOverlay, m_autoHydroTimer: Qt parent-child ile silinir
}

void MainWindow::SetDocument(core::Document* doc) {
    m_document = doc;
    if (m_vulkanWindow && doc) {
        m_vulkanWindow->SetNetwork(&doc->GetNetwork());
    }
    RefreshLayerPanel();
    RebuildCADEntityCache();
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

    m_actPlaceSmartPoint = new QAction("Akilli Baglanti Noktasi", this);
    m_actPlaceSmartPoint->setToolTip("Mimari planda cihaz zaten varsa sadece baglanti sembolü (yildiz) yerlestir — görsel kirlilik olmaz");
    connect(m_actPlaceSmartPoint, &QAction::triggered, this, &MainWindow::OnPlaceSmartPoint);

    m_actBosaltmaNoktasi = new QAction("Ana Tahliye Noktasini Isaretle", this);
    m_actBosaltmaNoktasi->setToolTip("En alttaki Drain node'unu bina ana kanalizasyon baglantisi olarak isaretle");
    connect(m_actBosaltmaNoktasi, &QAction::triggered, this, &MainWindow::OnBosaltmaNoktasi);

    m_actLayerVisibility = new QAction("Katman Gorunurlugu...", this);
    m_actLayerVisibility->setToolTip("Temiz Su / Sicak Su / Pis Su katmanlarini bagimsiz goster veya gizle");
    m_actLayerVisibility->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(m_actLayerVisibility, &QAction::triggered, this, &MainWindow::OnLayerVisibility);

    m_actCopyFloor = new QAction("Tesisat Kopyala...", this);
    m_actCopyFloor->setToolTip("Seçilen katın yatay borularını ve armatürlerini başka kata kopyala (kolonlar hariç)");
    connect(m_actCopyFloor, &QAction::triggered, this, &MainWindow::OnCopyFloor);

    m_actConnectFixture = new QAction("Cihazı Tesisata Bagla", this);
    m_actConnectFixture->setToolTip("Armaturu ana boru hattina kisa dal boru ile bagla (BAGLA komutu)");
    m_actConnectFixture->setShortcut(QKeySequence("Ctrl+B"));
    connect(m_actConnectFixture, &QAction::triggered, this, &MainWindow::OnConnectFixture);

    m_actDrawColumn = new QAction("Kolon Boru Ciz", this);
    m_actDrawColumn->setToolTip("Dikey kolon borusu: bir node sec, hedef kati sec — ayni XY, farkli Z (Ctrl+Shift+K)");
    m_actDrawColumn->setShortcut(QKeySequence("Ctrl+Shift+K"));
    connect(m_actDrawColumn, &QAction::triggered, this, &MainWindow::OnDrawColumn);

    // Sıcak su araçları
    m_actDrawHotWaterPipe = new QAction("Sicak Su Borusu", this);
    m_actDrawHotWaterPipe->setToolTip("Sicak su borusu ciz (kirmizi) — sofben / kazan hattina bagli");
    connect(m_actDrawHotWaterPipe, &QAction::triggered, this, &MainWindow::OnDrawHotWaterPipe);

    m_actPlaceHotSource = new QAction("Sicak Su Kaynagi (Sofben/Kazan)", this);
    m_actPlaceHotSource->setToolTip("Sofben veya kazan — sicak su baslangi noktasi (kirmizi dugum)");
    connect(m_actPlaceHotSource, &QAction::triggered, this, &MainWindow::OnPlaceHotSource);

    // Doğal Gaz araçları
    m_actDrawGasPipe = new QAction("Dogalgaz Borusu", this);
    m_actDrawGasPipe->setToolTip("Dogalgaz borusu ciz (sari) — TS EN 1775");
    connect(m_actDrawGasPipe, &QAction::triggered, this, &MainWindow::OnDrawGasPipe);

    m_actPlaceGasSource = new QAction("Gaz Sayaci / Giris", this);
    m_actPlaceGasSource->setToolTip("Dogalgaz sayaci veya sehir gazi giris noktasi");
    connect(m_actPlaceGasSource, &QAction::triggered, this, &MainWindow::OnPlaceGasSource);

    m_actPlaceGasAppliance = new QAction("Gaz Cihazi Yerles...", this);
    m_actPlaceGasAppliance->setToolTip("Kombi / Ocak / Sofben / Soba yerles");
    connect(m_actPlaceGasAppliance, &QAction::triggered, this, &MainWindow::OnPlaceGasAppliance);

    // Isıtma araçları
    m_actDrawHeatingPipe = new QAction("Isitma Boru Gidis", this);
    m_actDrawHeatingPipe->setToolTip("Isitma gidis borusu ciz (kirmizi-turuncu) — EN 12831");
    connect(m_actDrawHeatingPipe, &QAction::triggered, this, &MainWindow::OnDrawHeatingPipe);

    m_actDrawHeatingReturn = new QAction("Isitma Boru Donus", this);
    m_actDrawHeatingReturn->setToolTip("Isitma donus borusu ciz (mavi) — EN 12831");
    connect(m_actDrawHeatingReturn, &QAction::triggered, this, &MainWindow::OnDrawHeatingReturn);

    m_actPlaceBoiler = new QAction("Kazan / Isitma Kaynagi", this);
    m_actPlaceBoiler->setToolTip("Isitma kazani veya kombi — gidis/donus baslangic noktasi");
    connect(m_actPlaceBoiler, &QAction::triggered, this, &MainWindow::OnPlaceBoiler);

    m_actPlaceRadiator = new QAction("Radyator Yerles...", this);
    m_actPlaceRadiator->setToolTip("Radyator isi yukunu ayarla ve yerles");
    connect(m_actPlaceRadiator, &QAction::triggered, this, &MainWindow::OnPlaceRadiator);

    // Yangın araçları
    m_actDrawFireLine = new QAction("Yangin Hatti", this);
    m_actDrawFireLine->setToolTip("Yangin borusu ciz (koyu kirmizi) — EN 12845");
    connect(m_actDrawFireLine, &QAction::triggered, this, &MainWindow::OnDrawFireLine);

    m_actPlaceSprinkler = new QAction("Sprinkler Yerles", this);
    m_actPlaceSprinkler->setToolTip("Sprinkler basligi yerles — EN 12845");
    connect(m_actPlaceSprinkler, &QAction::triggered, this, &MainWindow::OnPlaceSprinkler);

    m_actPlaceFirePump = new QAction("Yangin Pompasi", this);
    m_actPlaceFirePump->setToolTip("Yangin pompasi / tank giris noktasi");
    connect(m_actPlaceFirePump, &QAction::triggered, this, &MainWindow::OnPlaceFirePump);

    // Tesisatı Kabul Et
    m_actTesistatKabul = new QAction("Tesisati Kabul Et", this);
    m_actTesistatKabul->setToolTip("Tum tesisati dogrula, parcaları numaralandır ve hesap modülüne hazırla (Ctrl+Enter)");
    m_actTesistatKabul->setShortcut(QKeySequence("Ctrl+Return"));
    connect(m_actTesistatKabul, &QAction::triggered, this, &MainWindow::OnTesistatKabul);

    m_actPrintLayout = new QAction("Pafta Duzenle ve Yazdir...", this);
    m_actPrintLayout->setToolTip("A3/A4 pafta + ISO 7200 baslik bloku + PDF/SVG kaydet (Ctrl+P)");
    m_actPrintLayout->setShortcut(QKeySequence("Ctrl+P"));
    connect(m_actPrintLayout, &QAction::triggered, this, &MainWindow::OnPrintLayout);

    m_actExportDXF = new QAction("Tam Proje DXF Olarak Kaydet...", this);
    m_actExportDXF->setToolTip("Tum katlar + MEP sebekesi DXF R2000 formatinda");
    m_actExportDXF->setShortcut(QKeySequence("Ctrl+Shift+E"));
    connect(m_actExportDXF, &QAction::triggered, this, &MainWindow::OnExportDXF);

    m_actExportFloorDXF = new QAction("Aktif Kat DXF Olarak Kaydet...", this);
    m_actExportFloorDXF->setToolTip("Sadece aktif katin tesisat sebekesini DXF'e aktar");
    connect(m_actExportFloorDXF, &QAction::triggered, this, &MainWindow::OnExportFloorDXF);

    m_actHidrofor = new QAction("Hidrofor Boyutlandirma...", this);
    m_actHidrofor->setToolTip("Kritik devre analizi sonucuna gore hidrofor/pompa secimi");
    connect(m_actHidrofor, &QAction::triggered, this, &MainWindow::OnHidrofor);

    m_actNormSelection = new QAction("Hesap Normu...", this);
    m_actNormSelection->setToolTip("Besleme debisi hesaplama normunu sec: EN 806-3 veya DIN 1988-300");
    connect(m_actNormSelection, &QAction::triggered, this, &MainWindow::OnNormSelection);

    m_actDevreSecenekleri = new QAction("Devre Secenekleri...", this);
    m_actDevreSecenekleri->setToolTip("Bina tipi, boru cinsi, puruzluluk, max hiz — tek yerden ayar (FineSANI Devre Secenekleri)");
    m_actDevreSecenekleri->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(m_actDevreSecenekleri, &QAction::triggered, this, &MainWindow::OnDevreSecenekleri);

    m_actBaskiIcerigi = new QAction("Baski Icerigi...", this);
    m_actBaskiIcerigi->setToolTip("Cizimde hangi etiketler gorunsun: DN, debi, uzunluk, hiz, basinc kaybi");
    connect(m_actBaskiIcerigi, &QAction::triggered, this, &MainWindow::OnBaskiIcerigi);

    m_actBaskiKaybi = new QAction("Parcalarin Basinc Kaybi...", this);
    m_actBaskiKaybi->setToolTip("Tum devrelerin basinc kaybi tablosu — kritik devre vurgulu");
    connect(m_actBaskiKaybi, &QAction::triggered, this, &MainWindow::OnBaskiKaybi);

    m_actWordRapor = new QAction("Word Rapor Olustur (.docx)...", this);
    m_actWordRapor->setToolTip("Hesap foyu + kritik devre + armatur listesi OOXML .docx olarak kaydet (Microsoft Word acar)");
    connect(m_actWordRapor, &QAction::triggered, this, &MainWindow::OnWordRapor);

    m_actYagmurSuyu = new QAction("Yagmur Suyu Modulu...", this);
    m_actYagmurSuyu->setToolTip("EN 12056-3: Yagmur suyu tahliye boru boyutlandirmasi");
    connect(m_actYagmurSuyu, &QAction::triggered, this, &MainWindow::OnYagmurSuyu);

    m_actBOM = new QAction("Kesif Listesi (BOM)...", this);
    m_actBOM->setToolTip("Boru metrajlari ve baglanti elemanlari dokumu");
    m_actBOM->setShortcut(QKeySequence("Ctrl+K"));
    connect(m_actBOM, &QAction::triggered, this, &MainWindow::OnBOM);

    m_actRiserDiagram = new QAction("Kolon Semasi (Riser)...", this);
    m_actRiserDiagram->setToolTip("SVG kolon semasi uret ve goster");
    m_actRiserDiagram->setShortcut(QKeySequence("Ctrl+R"));
    connect(m_actRiserDiagram, &QAction::triggered, this, &MainWindow::OnRiserDiagram);

    m_actDNOverride = new QAction("DN Manuel Override...", this);
    m_actDNOverride->setToolTip("Hesap foyunde boru caplarinI manuel duzenle");
    connect(m_actDNOverride, &QAction::triggered, this, &MainWindow::OnDNOverride);

    m_actCizimiGuncelle = new QAction("Cizimi Guncelle...", this);
    m_actCizimiGuncelle->setShortcut(QKeySequence("Ctrl+Shift+U"));
    m_actCizimiGuncelle->setToolTip("Hesap sonuclarini (uzunluk/doluluk/egim) cizime yaz");
    connect(m_actCizimiGuncelle, &QAction::triggered, this, &MainWindow::OnCizimiGuncelle);

    m_actFoseptik = new QAction("Kapali Cukur / Foseptik Hesabi...", this);
    m_actFoseptik->setToolTip("TS 822 / EN 12566-1 hacim hesabi");
    connect(m_actFoseptik, &QAction::triggered, this, &MainWindow::OnFoseptik);

    m_actPisSuHesapFoyu = new QAction("Pis Su Hesap Foyu...", this);
    m_actPisSuHesapFoyu->setToolTip("Drenaj devresi: DU + egim + doluluk tablosu");
    connect(m_actPisSuHesapFoyu, &QAction::triggered, this, &MainWindow::OnPisSuHesapFoyu);

    m_actEmdirmeCukuru = new QAction("Emdirme Cukuru Hesabi...", this);
    m_actEmdirmeCukuru->setToolTip("Toprak emme kapasitesi hesabi");
    connect(m_actEmdirmeCukuru, &QAction::triggered, this, &MainWindow::OnEmdirmeCukuru);

    m_actPisSuCukuru = new QAction("Pis Su Cukuru Hesabi...", this);
    m_actPisSuCukuru->setToolTip("Gecirimsiz depolama tanki hacim hesabi");
    connect(m_actPisSuCukuru, &QAction::triggered, this, &MainWindow::OnPisSuCukuru);

    m_actPisSuPompasi = new QAction("Pis Su Pompasi Boyutlandirma...", this);
    m_actPisSuPompasi->setToolTip("Fosseptik/cukur tahliye pompa guc ve debi hesabi");
    connect(m_actPisSuPompasi, &QAction::triggered, this, &MainWindow::OnPisSuPompasi);

    m_actGenlesimTanki = new QAction("Membranlı Genleşme Tankı...", this);
    m_actGenlesimTanki->setToolTip("EN 12828 — kapalı ısıtma/sıcak su devresinde tank hacmi");
    connect(m_actGenlesimTanki, &QAction::triggered, this, &MainWindow::OnGenlesimTanki);

    m_actYagmurAlani = new QAction("Yağmur Alanı (Poligon)...", this);
    m_actYagmurAlani->setToolTip("Çizimden poligon seçip yağmur suyu debi hesabı");
    connect(m_actYagmurAlani, &QAction::triggered, this, &MainWindow::OnYagmurAlani);

    m_actNormKarsilastirma = new QAction("Norm Karşılaştırma (EN vs DIN)...", this);
    m_actNormKarsilastirma->setToolTip("EN 806-3 ve DIN 1988-300 hesap sonuçlarını yan yana karşılaştır");
    connect(m_actNormKarsilastirma, &QAction::triggered, this, &MainWindow::OnNormKarsilastirma);

    m_actHesapKarari = new QAction("Hesap Kararı (Neden Bu Çap?)...", this);
    m_actHesapKarari->setToolTip("Her boru için çap seçim gerekçesini göster");
    connect(m_actHesapKarari, &QAction::triggered, this, &MainWindow::OnHesapKarari);

    m_actBirleskMod = new QAction("Birleşik Yerleştirme Modu", this);
    m_actBirleskMod->setCheckable(true);
    m_actBirleskMod->setToolTip("Armatür yerleştirince otomatik boru hattına bağla (BAGLA modu)");
    connect(m_actBirleskMod, &QAction::triggered, this, &MainWindow::OnBirleskMod);

    m_actSelect = new QAction("Sec", this);
    connect(m_actSelect, &QAction::triggered, this, &MainWindow::OnSelectMode);

    // Görünüm
    m_actPlanView = new QAction("Plan Görünümü", this);
    connect(m_actPlanView, &QAction::triggered, this, &MainWindow::OnPlanView);

    m_actIsometricView = new QAction("İzometrik Görünüm", this);
    connect(m_actIsometricView, &QAction::triggered, this, &MainWindow::OnIsometricView);

    m_actZoomExtents = new QAction("Tümünü Göster", this);
    connect(m_actZoomExtents, &QAction::triggered, this, &MainWindow::OnZoomExtents);

    m_actSnapIntersection = new QAction("Kesişim Snap (F3)", this);
    m_actSnapIntersection->setShortcut(Qt::Key_F3);
    m_actSnapIntersection->setCheckable(true);
    m_actSnapIntersection->setChecked(true);
    m_actSnapIntersection->setToolTip("Büyük DXF/DWG dosyalarında performans için Kesişim snap'i kapat (F3)");
    connect(m_actSnapIntersection, &QAction::toggled, this, [this](bool on) {
        if (on)
            m_snapManager.EnableSnap(cad::SnapType::Intersection);
        else
            m_snapManager.DisableSnap(cad::SnapType::Intersection);
        statusBar()->showMessage(on ? "Kesişim snap AÇIK" : "Kesişim snap KAPALI", 2000);
    });

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

    m_actFloorAlignment = new QAction("&3D Hizalama Kontrolü...", this);
    m_actFloorAlignment->setShortcut(QKeySequence("Ctrl+Shift+H"));
    m_actFloorAlignment->setToolTip("Katlara göre Z kotlarını ve boru hizalamasını doğrula");
    connect(m_actFloorAlignment, &QAction::triggered, this, &MainWindow::OnFloorAlignment);

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
    auto* actNewFromTemplate = fileMenu->addAction("Şablondan Yeni...");
    actNewFromTemplate->setToolTip("Hazir sablondan yeni proje olustur (3 katli konut, otel odasi, vb.)");
    actNewFromTemplate->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(actNewFromTemplate, &QAction::triggered, this, &MainWindow::OnNewFromTemplate);
    fileMenu->addAction(m_actOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actImportDXF);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExportDXF);
    fileMenu->addAction(m_actExportFloorDXF);
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
    editMenu->addSeparator();
    {
        auto* actSelectAll = editMenu->addAction("Tümünü Seç");
        actSelectAll->setShortcut(QKeySequence::SelectAll);
        connect(actSelectAll, &QAction::triggered, this, &MainWindow::OnSelectAll);

        auto* actMirror = editMenu->addAction("Ayna (Mirror)...");
        actMirror->setShortcut(Qt::Key_M | Qt::NoModifier);
        connect(actMirror, &QAction::triggered, this, &MainWindow::OnMirror);

        auto* actOffset = editMenu->addAction("Offset (Paralel Kopya)...");
        actOffset->setShortcut(Qt::Key_O | Qt::NoModifier);
        connect(actOffset, &QAction::triggered, this, &MainWindow::OnOffset);

        editMenu->addSeparator();

        auto* actCopy  = editMenu->addAction("Kopyala");
        actCopy->setShortcut(QKeySequence::Copy);
        connect(actCopy, &QAction::triggered, this, &MainWindow::OnCopy);

        auto* actPaste = editMenu->addAction("Yapıştır...");
        actPaste->setShortcut(QKeySequence::Paste);
        connect(actPaste, &QAction::triggered, this, &MainWindow::OnPaste);

        editMenu->addSeparator();
        auto* actTrim  = editMenu->addAction("Trim (Kısalt)...");
        actTrim->setShortcut(Qt::Key_T | Qt::NoModifier);
        connect(actTrim, &QAction::triggered, this, &MainWindow::OnTrim);

        auto* actExtend = editMenu->addAction("Extend (Uzat)...");
        actExtend->setShortcut(Qt::Key_X | Qt::NoModifier);
        actExtend->setToolTip("Seçili sınıra kadar çizgiyi uzat (TRIM tersi)");
        connect(actExtend, &QAction::triggered, this, &MainWindow::OnExtend);
    }

    // Çizim
    auto* drawMenu = menuBar()->addMenu("Çi&zim");
    drawMenu->addAction(m_actDrawPipe);
    drawMenu->addAction(m_actDrawFixture);
    drawMenu->addAction(m_actDrawJunction);
    drawMenu->addAction(m_actConnectFixture);
    drawMenu->addAction(m_actDrawColumn);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawHotWaterPipe);
    drawMenu->addAction(m_actPlaceHotSource);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawGasPipe);
    drawMenu->addAction(m_actPlaceGasSource);
    drawMenu->addAction(m_actPlaceGasAppliance);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawHeatingPipe);
    drawMenu->addAction(m_actDrawHeatingReturn);
    drawMenu->addAction(m_actPlaceBoiler);
    drawMenu->addAction(m_actPlaceRadiator);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawFireLine);
    drawMenu->addAction(m_actPlaceSprinkler);
    drawMenu->addAction(m_actPlaceFirePump);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actDrawDrainPipe);
    drawMenu->addAction(m_actPlaceYerSuzgeci);
    drawMenu->addAction(m_actPlaceRogar);
    drawMenu->addAction(m_actPlaceSmartPoint);
    drawMenu->addAction(m_actBosaltmaNoktasi);
    drawMenu->addSeparator();
    drawMenu->addAction(m_actCopyFloor);

    // Görünüm
    auto* viewMenu = menuBar()->addMenu("&Görünüm");
    viewMenu->addAction(m_actPlanView);
    viewMenu->addAction(m_actIsometricView);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actZoomExtents);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actLayerVisibility);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actSnapIntersection);

    // Analiz
    auto* analyzeMenu = menuBar()->addMenu("&Analiz");
    analyzeMenu->addAction(m_actTesistatKabul);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actRunHydraulics);
    analyzeMenu->addAction(m_actAutoSizeDN);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actHidrofor);
    analyzeMenu->addAction(m_actNormSelection);
    analyzeMenu->addAction(m_actDevreSecenekleri);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actBaskiKaybi);
    analyzeMenu->addAction(m_actBaskiIcerigi);
    analyzeMenu->addAction(m_actWordRapor);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actYagmurSuyu);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actBOM);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actRiserDiagram);
    analyzeMenu->addAction(m_actDNOverride);
    analyzeMenu->addAction(m_actCizimiGuncelle);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actPisSuHesapFoyu);
    analyzeMenu->addAction(m_actFoseptik);
    analyzeMenu->addAction(m_actPisSuCukuru);
    analyzeMenu->addAction(m_actEmdirmeCukuru);
    analyzeMenu->addAction(m_actPisSuPompasi);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actGenlesimTanki);
    analyzeMenu->addAction(m_actYagmurAlani);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actNormKarsilastirma);
    analyzeMenu->addAction(m_actHesapKarari);
    analyzeMenu->addAction(m_actBirleskMod);
    analyzeMenu->addSeparator();
    // Yeni özellikler: AUTO-OLCULENDIR, BASINC-BOLGESI, URETICI-KATALOG
    {
        auto* actAutoOlc = new QAction("Otomatik Olculendir (DN+L)", this);
        actAutoOlc->setToolTip("MEP borularina DN ve uzunluk olculeri ekle (AUTO-OLCULENDIR)");
        connect(actAutoOlc, &QAction::triggered, this, &MainWindow::OnAutoOlculendir);
        analyzeMenu->addAction(actAutoOlc);

        auto* actBolge = new QAction("Basinc Bolgesi Analizi...", this);
        actBolge->setToolTip("Kaynak bazli basinc bolgesi ve pompa boyutlandirmasi (BOLGE)");
        connect(actBolge, &QAction::triggered, this, &MainWindow::OnBaskiBolgesi);
        analyzeMenu->addAction(actBolge);

        auto* actKatalog = new QAction("Uretici Katalog...", this);
        actKatalog->setToolTip("Wavin / Valsir / Henco boru katalogu ve secimi (KATALOG)");
        connect(actKatalog, &QAction::triggered, this, &MainWindow::OnUreticiKatalog);
        analyzeMenu->addAction(actKatalog);
    }
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actGenerateSchedule);
    analyzeMenu->addAction(m_actExportReport);
    analyzeMenu->addSeparator();
    analyzeMenu->addAction(m_actPrintLayout);

    // Mimari
    auto* mimariMenu = menuBar()->addMenu("&Mimari");
    mimariMenu->addAction(m_actMimariBelirle);
    mimariMenu->addSeparator();
    mimariMenu->addAction(m_actFloorAlignment);

    auto* helpMenu = menuBar()->addMenu("&Yardım");
    auto* actHelp  = helpMenu->addAction("Kullanıcı Kılavuzu (F1)");
    actHelp->setShortcut(QKeySequence::HelpContents);
    auto* actAbout = helpMenu->addAction("Hakkında...");
    connect(actHelp,  &QAction::triggered, this, &MainWindow::OnHelp);
    connect(actAbout, &QAction::triggered, this, &MainWindow::OnAbout);
}

void MainWindow::CreateToolbars() {
    // Çizim toolbar
    m_drawToolbar = addToolBar("Çizim");
    m_drawToolbar->addAction(m_actSelect);
    m_drawToolbar->addAction(m_actDrawPipe);
    m_drawToolbar->addAction(m_actDrawFixture);
    m_drawToolbar->addAction(m_actDrawJunction);
    m_drawToolbar->addAction(m_actConnectFixture);
    m_drawToolbar->addAction(m_actDrawColumn);

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

    // Aktif kat seçici toolbar
    auto* floorToolbar = addToolBar("Aktif Kat");
    floorToolbar->addWidget(new QLabel("  Kat: "));
    m_floorSelector = new QComboBox();
    m_floorSelector->setMinimumWidth(160);
    m_floorSelector->setToolTip("Aktif kat — yeni cihaz ve borular bu katın yüksekliğinde (Z) yerleşir");
    m_floorSelector->addItem("Zemin Kat (z=0.00 m)");
    connect(m_floorSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnActiveFloorChanged);
    floorToolbar->addWidget(m_floorSelector);
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
    m_fixtureDock->raise(); // Armatür Özellikleri varsayılan sekme

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
    connect(m_stPanel, &STFixturePanel::SmartPointModeChanged,
            this, [this](bool on) {
                statusBar()->showMessage(
                    on ? "Akilli Baglanti Noktasi: AKTIF — cihazlar yildiz sembolu olarak yerlestirilecek"
                       : "Akilli Baglanti Noktasi: KAPALI — tam cihaz simgesi yerlestirilecek", 3000);
            });

    // Layer Manager Paneli — AutoCAD tarzı katman görünürlük kontrolü
    m_layerPanel = new QDockWidget("Katman Yöneticisi", this);
    auto* layWid = new QWidget();
    auto* layVBox = new QVBoxLayout(layWid);
    layVBox->setContentsMargins(2, 2, 2, 2);
    layVBox->setSpacing(2);

    // Araç çubuğu satırı 1: Göster/Gizle
    auto* layToolRow = new QHBoxLayout();
    auto* btnLayerAll  = new QPushButton("Tümünü Göster");
    auto* btnLayerNone = new QPushButton("Tümünü Gizle");
    btnLayerAll->setFixedHeight(22);
    btnLayerNone->setFixedHeight(22);
    layToolRow->addWidget(btnLayerAll);
    layToolRow->addWidget(btnLayerNone);
    layVBox->addLayout(layToolRow);

    // Araç çubuğu satırı 2: LAYISO / LAYUNISO / PURGE
    auto* layToolRow2 = new QHBoxLayout();
    auto* btnLayIso    = new QPushButton("İzole (LAYISO)");
    auto* btnLayUnIso  = new QPushButton("İzole Kaldır");
    auto* btnPurge     = new QPushButton("Temizle (PURGE)");
    btnLayIso->setFixedHeight(22);
    btnLayUnIso->setFixedHeight(22);
    btnPurge->setFixedHeight(22);
    btnLayIso->setToolTip("Seçili entity'nin katmanını izole et — diğerlerini gizle (LAYISO)");
    btnLayUnIso->setToolTip("İzolasyonu geri al — tüm katmanları eski haline döndür (LAYUNISO)");
    btnPurge->setToolTip("Kullanılmayan katmanları ve blokları sil (PURGE)");
    layToolRow2->addWidget(btnLayIso);
    layToolRow2->addWidget(btnLayUnIso);
    layToolRow2->addWidget(btnPurge);
    layVBox->addLayout(layToolRow2);

    // QTreeWidget: 4 sütun — G (görünür), K (kilit), D (dondur), İsim
    m_layerList = new QTreeWidget();
    m_layerList->setColumnCount(4);
    m_layerList->setHeaderLabels({"G", "K", "D", "Katman Adı"});
    m_layerList->header()->setDefaultSectionSize(24);
    m_layerList->header()->resizeSection(0, 24);
    m_layerList->header()->resizeSection(1, 24);
    m_layerList->header()->resizeSection(2, 24);
    m_layerList->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_layerList->setRootIsDecorated(false);
    m_layerList->setSortingEnabled(true);
    m_layerList->sortByColumn(3, Qt::AscendingOrder);
    m_layerList->setToolTip("G=Görünür  K=Kilitli  D=Dondurulmuş\nÇift tıklama=Renk değiştir\nSağ tık=Menü");
    m_layerList->setStyleSheet(
        "QTreeWidget::item { padding: 1px; }"
        "QTreeWidget::item:selected { background: #2255aa; color: white; }");
    layVBox->addWidget(m_layerList);

    m_layerPanel->setWidget(layWid);
    m_layerPanel->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, m_layerPanel);
    tabifyDockWidget(m_spacePanel, m_layerPanel);
    m_layerPanel->raise();

    // Sütun tıklama — G/K/D toggle
    connect(m_layerList, &QTreeWidget::itemClicked, this,
        [this](QTreeWidgetItem* item, int col) {
            if (!m_document || !item) return;
            QString layName = item->text(3);
            auto& layers = m_document->GetLayersMutable();
            auto it = layers.find(layName.toStdString());
            if (it == layers.end()) return;
            auto& lay = it->second;

            if (col == 0) {
                lay.SetVisible(!lay.IsVisible());
            } else if (col == 1) {
                lay.SetLocked(!lay.IsLocked());
            } else if (col == 2) {
                lay.SetFrozen(!lay.IsFrozen());
            } else {
                return; // isim sütunu — tıklama seçim, eylem yok
            }
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
            RefreshLayerPanel();
            InvalidateRenderer();
        });

    // Çift tıklama → katman rengi düzenleme
    connect(m_layerList, &QTreeWidget::itemDoubleClicked, this,
        [this](QTreeWidgetItem* item, int /*col*/) {
            if (!m_document || !item) return;
            QString layName = item->text(3);
            auto& layers = m_document->GetLayersMutable();
            auto it = layers.find(layName.toStdString());
            if (it == layers.end()) return;
            auto& lay = it->second;
            QColor cur(lay.GetColor().r, lay.GetColor().g, lay.GetColor().b);
            QColor chosen = QColorDialog::getColor(cur, this,
                QString("Katman Rengi: %1").arg(layName));
            if (!chosen.isValid()) return;
            cad::Color nc{static_cast<uint8_t>(chosen.red()),
                          static_cast<uint8_t>(chosen.green()),
                          static_cast<uint8_t>(chosen.blue())};
            lay.SetColor(nc);
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
            RefreshLayerPanel();
            InvalidateRenderer();
            statusBar()->showMessage(QString("'%1' katman rengi değiştirildi").arg(layName), 2000);
        });

    // Sağ tık menüsü — Seçimi Katmana Taşı / Katmandakileri Seç / Kilitle / Dondur
    m_layerList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_layerList, &QTreeWidget::customContextMenuRequested, this,
        [this](const QPoint& pos) {
            auto* item = m_layerList->itemAt(pos);
            if (!item || !m_document) return;
            QString layName = item->text(3);
            QMenu menu(this);
            menu.addAction(QString("Katmandakileri Seç: %1").arg(layName), this,
                [this, layName]() { OnSelectByLayer(layName); });
            if (!m_selectedCADEntityIds.empty() || m_selectedCADEntityId != 0)
                menu.addAction("Seçimi Bu Katmana Taşı", this,
                    [this, layName]() {
                        m_moveToLayerTarget = layName;
                        OnMoveToLayer();
                    });
            menu.addSeparator();
            auto& layers = m_document->GetLayersMutable();
            auto it = layers.find(layName.toStdString());
            if (it != layers.end()) {
                bool locked = it->second.IsLocked();
                bool frozen = it->second.IsFrozen();
                menu.addAction(locked ? "Kilidi Kaldır" : "Kilitle", this,
                    [this, layName, locked]() {
                        auto& lm = m_document->GetLayersMutable();
                        if (lm.count(layName.toStdString()))
                            lm.at(layName.toStdString()).SetLocked(!locked);
                        RefreshLayerPanel(); InvalidateRenderer();
                    });
                menu.addAction(frozen ? "Çöz (Thaw)" : "Dondur (Freeze)", this,
                    [this, layName, frozen]() {
                        auto& lm = m_document->GetLayersMutable();
                        if (lm.count(layName.toStdString()))
                            lm.at(layName.toStdString()).SetFrozen(!frozen);
                        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                            m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
                        RefreshLayerPanel(); InvalidateRenderer();
                    });
            }
            menu.exec(m_layerList->mapToGlobal(pos));
        });

    connect(btnLayerAll, &QPushButton::clicked, this, [this]() {
        if (!m_document) return;
        for (auto& [name, layer] : m_document->GetLayersMutable())
            layer.SetVisible(true);
        RefreshLayerPanel();
        InvalidateRenderer();
    });

    connect(btnLayerNone, &QPushButton::clicked, this, [this]() {
        if (!m_document) return;
        for (auto& [name, layer] : m_document->GetLayersMutable())
            layer.SetVisible(false);
        RefreshLayerPanel();
        InvalidateRenderer();
    });

    connect(btnLayIso,   &QPushButton::clicked, this, &MainWindow::OnLayIso);
    connect(btnLayUnIso, &QPushButton::clicked, this, &MainWindow::OnLayUnIso);
    connect(btnPurge,    &QPushButton::clicked, this, &MainWindow::OnPurge);

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
    statusBar()->showMessage("Yeni proje olusturuldu", 3000);
}

// ============================================================
//  ŞABLONDAN YENİ PROJE
// ============================================================
void MainWindow::OnNewFromTemplate() {
    if (m_document && m_document->IsModified()) {
        auto reply = QMessageBox::question(this, "Sablondan Yeni",
            "Mevcut proje kaydedilmedi. Devam edilsin mi?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    // Şablon dizinini exe yanında veya kaynak dizininde ara
    QString templateDir;
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath() + "/templates",
        QCoreApplication::applicationDirPath() + "/../templates",
        QString::fromStdString([]() -> std::string {
            // kaynak ağacındaki templates/ (geliştirme ortamı)
            return "C:/Users/afney/Desktop/vulkan/templates";
        }())
    };
    for (const auto& d : searchDirs) {
        if (QDir(d).exists()) { templateDir = d; break; }
    }

    if (templateDir.isEmpty()) {
        QMessageBox::information(this, "Sablondan Yeni",
            "Sablon dizini bulunamadi.\n"
            "templates/ klasorunu uygulama yani ile ayni dizine koyun.");
        return;
    }

    // index.json oku
    QFile indexFile(templateDir + "/index.json");
    if (!indexFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Sablon Hatasi", "templates/index.json okunamadi.");
        return;
    }
    QJsonDocument idxDoc = QJsonDocument::fromJson(indexFile.readAll());
    indexFile.close();
    if (idxDoc.isNull() || !idxDoc.isObject()) {
        QMessageBox::warning(this, "Sablon Hatasi", "templates/index.json gecersiz.");
        return;
    }

    QJsonArray templates = idxDoc.object().value("templates").toArray();
    if (templates.isEmpty()) {
        QMessageBox::information(this, "Sablon Yok", "Hicbir sablon bulunamadi.");
        return;
    }

    // Şablon seçim dialog
    QStringList names;
    QStringList files;
    QStringList descs;
    for (const auto& t : templates) {
        QJsonObject o = t.toObject();
        names  << o.value("name").toString();
        files  << o.value("file").toString();
        descs  << o.value("description").toString();
    }

    bool ok;
    QString selected = QInputDialog::getItem(this, "Sablondan Yeni Proje",
        "Sablon secin:", names, 0, false, &ok);
    if (!ok) return;

    int idx = names.indexOf(selected);
    if (idx < 0) return;

    QString tmplFile = templateDir + "/" + files[idx];
    auto& app = core::Application::Instance();
    auto* doc = app.CreateNewDocument();
    if (!doc->Load(tmplFile.toStdString())) {
        QMessageBox::critical(this, "Sablon Yuklenemedi",
            QString("Dosya acilamadi:\n%1").arg(tmplFile));
        return;
    }
    // Şablonu yeni proje olarak aç (kaydedilmemiş)
    doc->SetFilePath("");

    SetDocument(doc);
    setWindowTitle(QString("VKT — %1 (Sablondan)").arg(selected));
    ScheduleAutoHydro();

    int nodeCount = 0;
    for (auto& [id, n] : doc->GetNetwork().GetNodeMap()) ++nodeCount;
    int edgeCount = 0;
    for (auto& [id, e] : doc->GetNetwork().GetEdgeMap()) ++edgeCount;

    statusBar()->showMessage(
        QString("Sablon yuklendi: %1 — %2 node, %3 boru (Ctrl+S ile kaydedin)")
            .arg(selected).arg(nodeCount).arg(edgeCount), 5000);
    if (m_logList)
        m_logList->addItem(QString("Sablon: %1 (%2 boru)").arg(selected).arg(edgeCount));
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
        statusBar()->showMessage(QString("Proje açıldı: %1").arg(filePath), 3000);
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
        statusBar()->showMessage("Proje kaydedildi", 3000);
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
        statusBar()->showMessage(QString("Proje kaydedildi: %1").arg(filePath), 3000);
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

            // Blok tanımlarını Document'a aktar (DXF BLOCK section)
            {
                auto blockReg = dialog.TakeBlockRegistry();
                if (blockReg.Size() > 0 && m_document) {
                    m_document->GetBlockRegistry() = std::move(blockReg);
                    LogCAD("[MainWindow] " + std::to_string(m_document->GetBlockRegistry().Size())
                           + " blok tanımı Document'a aktarıldı");
                }
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

            // Space + Layer paneli güncelle
            if (m_spacePanel) m_spacePanel->RefreshList();
            RefreshLayerPanel();
            RebuildCADEntityCache();

            m_logList->clear();
            m_logList->addItem(QString("CAD dosyası import başarılı!"));
            m_logList->addItem(QString("- %1 entity yüklendi").arg(entityCount));
            m_logList->addItem(QString("- %1 layer bulundu").arg(layers.size()));
            m_logList->addItem(QString("- %1 mahal eklendi").arg(addedCount));

            statusBar()->showMessage(QString("%1 entity, %2 layer, %3 mahal başarıyla yüklendi!")
                .arg(entityCount).arg(layers.size()).arg(addedCount), 4000);

            // Auto zoom to fit imported drawing
            OnZoomExtents();

            // Fixture oto-tanıma: Text entity anahtar kelimelerinden MEP node öner
            ScanForFixtureBlocks();
        }
    }
}

// ============================================================
//  Fixture oto-tanıma — DWG blok/metin tarama  (#18)
// ============================================================
void MainWindow::ScanForFixtureBlocks() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // Anahtar kelime → fixture tipi eşlemesi (Türkçe + İngilizce)
    static const std::vector<std::pair<std::string, std::string>> kKeywords = {
        {"LAVABO",    "Lavabo"}, {"WASHBASIN",  "Lavabo"}, {"SINK",    "Lavabo"},
        {"WC",        "WC"},     {"KLOZET",     "WC"},     {"TOILET",  "WC"},
        {"DUS",       "Dus"},    {"DUŞAKABIN",  "Dus"},    {"SHOWER",  "Dus"},
        {"KUVET",     "Kuvet"},  {"BATHTUB",    "Kuvet"},  {"BANYO",   "Kuvet"},
        {"EVYE",      "Evye"},   {"KITCHEN",    "Evye"},   {"MUTFAK",  "Evye"},
        {"PISUAR",    "Pisuar"}, {"URINAL",     "Pisuar"},
        {"BULAŞIK",   "Bulaşık Makinesi"}, {"DISHWASHER", "Bulaşık Makinesi"},
        {"CAMASIR",   "Çamaşır Makinesi"}, {"WASHING",    "Çamaşır Makinesi"},
    };

    int detected = 0;
    const auto& entities = m_document->GetCADEntities();

    for (const auto& e : entities) {
        if (!e) continue;
        if (e->GetType() != cad::EntityType::Text &&
            e->GetType() != cad::EntityType::MText &&
            e->GetType() != cad::EntityType::Insert) continue;

        std::string textContent;
        geom::Vec3  position;

        if (e->GetType() == cad::EntityType::Text ||
            e->GetType() == cad::EntityType::MText) {
            const auto* txt = static_cast<const cad::Text*>(e.get());
            textContent = txt->GetText();
            position    = txt->GetEffectiveInsertPoint();
        }

        if (textContent.empty()) continue;

        // Büyük harfe çevir (ASCII hızlı yaklaşım)
        std::string upper = textContent;
        for (auto& c : upper) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));

        for (const auto& [kw, fixtureType] : kKeywords) {
            if (upper.find(kw) != std::string::npos) {
                // Bu konumda zaten node var mı? (5m tolerans)
                bool alreadyExists = false;
                for (const auto& [nid, nd] : network.GetNodeMap()) {
                    double dx = nd.position.x - position.x;
                    double dy = nd.position.y - position.y;
                    if (dx*dx + dy*dy < 5000.0*5000.0) { alreadyExists = true; break; }
                }
                if (alreadyExists) break;

                mep::Node node;
                node.type        = mep::NodeType::Fixture;
                node.fixtureType = fixtureType;
                node.position    = {position.x, position.y, position.z};
                node.loadUnit    = mep::Database::Instance()
                                       .GetFixture(fixtureType).loadUnit;
                auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
                m_document->ExecuteCommand(std::move(cmd));
                ++detected;
                break; // her text için tek fixture
            }
        }
    }

    if (detected > 0) {
        statusBar()->showMessage(
            QString("Fixture oto-tanıma: %1 cihaz bulundu ve MEP ağına eklendi (Ctrl+Z ile geri alınabilir)")
                .arg(detected), 5000);
        if (m_logList) m_logList->addItem(
            QString("Fixture oto-tanıma: %1 cihaz eklendi (lavabo, WC, duş, vb.)").arg(detected));
        RefreshTextOverlay();
        ScheduleAutoHydro();
    }
}

void MainWindow::OnExit() {
    close();
}

void MainWindow::OnUndo() {
    if (m_document) {
        m_document->Undo();
        RebuildCADEntityCache();
        UpdateUI();
        ScheduleAutoHydro();
    }
}

void MainWindow::OnRedo() {
    if (m_document) {
        m_document->Redo();
        RebuildCADEntityCache();
        UpdateUI();
        ScheduleAutoHydro();
    }
}

// ============================================================
//  SELECT ALL (Ctrl+A)  — #6
// ============================================================
// ============================================================
//  TRIM komutu  (#16) — iki çizginin kesişimine kısalt
// ============================================================
void MainWindow::OnTrim() {
    if (!m_document) return;

    // Adım 1: Sınır entity seç (tek tıkla)
    bool ok;
    QString info = "TRIM: Önce sınır çizgiyi seçin (entity ID girin).\n"
                   "Seçim kutusu ile seçili entity ID'yi kullanın veya ID yazın.";

    // Seçili entity'yi sınır olarak kullan
    cad::EntityId boundId = m_selectedCADEntityId;
    if (m_selectedCADEntityIds.size() == 1) boundId = *m_selectedCADEntityIds.begin();

    if (boundId == 0) {
        QMessageBox::information(this, "TRIM",
            "Önce sınır olarak kullanılacak entity'yi seçin (tek tıklama),\n"
            "sonra TRIM komutunu çalıştırın.");
        return;
    }

    auto boundIt = m_cadEntityCache.find(boundId);
    if (boundIt == m_cadEntityCache.end() || !boundIt->second ||
        boundIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::information(this, "TRIM", "Sınır entity bir Line olmalıdır.");
        return;
    }
    const auto* boundLine = static_cast<const cad::Line*>(boundIt->second);

    // Adım 2: Kısaltılacak entity ID gir
    int trimId = QInputDialog::getInt(this, "TRIM — Kısaltılacak Entity",
        QString("Sınır: Entity #%1 (Line)\nKısaltılacak entity ID:").arg(boundId),
        0, 1, INT_MAX, 1, &ok);
    if (!ok || trimId == 0) return;

    auto trimIt = m_cadEntityCache.find(static_cast<cad::EntityId>(trimId));
    if (trimIt == m_cadEntityCache.end() || !trimIt->second ||
        trimIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::warning(this, "TRIM", "Kısaltılacak entity bir Line olmalıdır ve var olmalıdır.");
        return;
    }
    auto* trimLine = static_cast<cad::Line*>(trimIt->second);

    // Line-Line kesişim noktasını hesapla (2D)
    geom::Vec3 p1 = boundLine->GetStart(), p2 = boundLine->GetEnd();
    geom::Vec3 p3 = trimLine->GetStart(),  p4 = trimLine->GetEnd();

    double d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    double denom = d1x * d2y - d1y * d2x;
    if (std::abs(denom) < 1e-10) {
        QMessageBox::information(this, "TRIM", "Çizgiler paralel — kesişim yok.");
        return;
    }
    double t = ((p3.x - p1.x) * d2y - (p3.y - p1.y) * d2x) / denom;
    double s = ((p3.x - p1.x) * d1y - (p3.y - p1.y) * d1x) / denom;

    if (s < 0.0 || s > 1.0) {
        QMessageBox::information(this, "TRIM", "Kesişim nokta uzantıda — trim uygulanamaz.");
        return;
    }

    geom::Vec3 ix{p3.x + s * d2x, p3.y + s * d2y, p3.z};

    // Kısaltma: hangi tarafı kes? (sınıra göre başlangıca mı yoksa sona mı yakın?)
    QStringList sides = {"Başlangıç tarafını kes (intersection'dan sona doğru kalsın)",
                         "Son tarafını kes (başlangıçtan intersection'a doğru kalsın)"};
    QString side = QInputDialog::getItem(this, "TRIM — Taraf Seçimi",
        QString("Kesişim: (%.1f, %.1f)\nHangi taraf kesilsin?").arg(ix.x).arg(ix.y),
        sides, 0, false, &ok);
    if (!ok) return;

    // Revert + command ile undo stack
    geom::Vec3 origStart = trimLine->GetStart();
    geom::Vec3 origEnd   = trimLine->GetEnd();

    if (sides.indexOf(side) == 0) {
        // Başlangıç kesilir → start = intersection
        trimLine->SetStart(origStart); // revert
        auto cmd = std::make_unique<core::GripEditLineCommand>(trimLine, 0, origStart, ix);
        m_document->ExecuteCommand(std::move(cmd));
    } else {
        // Son kesilir → end = intersection
        trimLine->SetEnd(origEnd); // revert
        auto cmd = std::make_unique<core::GripEditLineCommand>(trimLine, 2, origEnd, ix);
        m_document->ExecuteCommand(std::move(cmd));
    }

    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("TRIM uygulandı — Entity #%1 kesişim noktasına kısaltıldı (Ctrl+Z ile geri alınabilir)").arg(trimId), 3000);
}

// ============================================================
//  EXTEND komutu — çizgiyi sınıra uzat (TRIM'in tersi)
// ============================================================
void MainWindow::OnExtend() {
    if (!m_document) return;

    // Sınır entity seçili olmalı
    cad::EntityId boundId = m_selectedCADEntityId;
    if (m_selectedCADEntityIds.size() == 1) boundId = *m_selectedCADEntityIds.begin();

    if (boundId == 0) {
        QMessageBox::information(this, "EXTEND",
            "Önce sınır olarak kullanılacak entity'yi seçin,\n"
            "sonra EXTEND komutunu çalıştırın.");
        return;
    }

    auto boundIt = m_cadEntityCache.find(boundId);
    if (boundIt == m_cadEntityCache.end() || !boundIt->second ||
        boundIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::information(this, "EXTEND", "Sınır entity bir Line olmalıdır.");
        return;
    }
    const auto* boundLine = static_cast<const cad::Line*>(boundIt->second);

    bool ok;
    int extId = QInputDialog::getInt(this, "EXTEND — Uzatılacak Entity",
        QString("Sınır: Entity #%1 (Line)\nUzatılacak entity ID:").arg(boundId),
        0, 1, INT_MAX, 1, &ok);
    if (!ok || extId == 0) return;

    auto extIt = m_cadEntityCache.find(static_cast<cad::EntityId>(extId));
    if (extIt == m_cadEntityCache.end() || !extIt->second ||
        extIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::warning(this, "EXTEND", "Uzatılacak entity bir Line olmalıdır ve var olmalıdır.");
        return;
    }
    auto* extLine = static_cast<cad::Line*>(extIt->second);

    // Line-Line kesişim hesabı — TRIM'den farklı: s sınırı yok, uzantıda da geçerli
    geom::Vec3 p1 = boundLine->GetStart(), p2 = boundLine->GetEnd();
    geom::Vec3 p3 = extLine->GetStart(),   p4 = extLine->GetEnd();

    double d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    double denom = d1x * d2y - d1y * d2x;
    if (std::abs(denom) < 1e-10) {
        QMessageBox::information(this, "EXTEND", "Çizgiler paralel — kesişim yok.");
        return;
    }
    double s = ((p3.x - p1.x) * d1y - (p3.y - p1.y) * d1x) / denom;

    // EXTEND için s herhangi bir değerde olabilir (uzantı da dahil)
    // ama sınır çizginin kendi uzantısına göre t kontrol et
    double t = ((p3.x - p1.x) * d2y - (p3.y - p1.y) * d2x) / denom;
    if (t < -0.001 || t > 1.001) {
        QMessageBox::information(this, "EXTEND",
            "Uzatılacak çizgi sınırın uzantısında değil.");
        return;
    }

    geom::Vec3 ix{p3.x + s * d2x, p3.y + s * d2y, p3.z};

    // Hangi uç sınıra daha yakın? O ucu uzat
    double distToStart = std::hypot(p3.x - ix.x, p3.y - ix.y);
    double distToEnd   = std::hypot(p4.x - ix.x, p4.y - ix.y);

    geom::Vec3 origStart = extLine->GetStart();
    geom::Vec3 origEnd   = extLine->GetEnd();

    if (distToEnd < distToStart) {
        // Bitiş ucu sınıra uzatılır
        auto cmd = std::make_unique<core::GripEditLineCommand>(extLine, 2, origEnd, ix);
        m_document->ExecuteCommand(std::move(cmd));
    } else {
        // Başlangıç ucu sınıra uzatılır
        auto cmd = std::make_unique<core::GripEditLineCommand>(extLine, 0, origStart, ix);
        m_document->ExecuteCommand(std::move(cmd));
    }

    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("EXTEND uygulandı — Entity #%1 sınıra uzatıldı (Ctrl+Z ile geri alınabilir)").arg(extId), 3000);
}

void MainWindow::OnSelectAll() {
    if (!m_document) return;
    m_selectedCADEntityIds.clear();
    m_selectedCADEntityId = 0;
    for (const auto& e : m_document->GetCADEntities()) {
        if (e && e->IsVisible()) m_selectedCADEntityIds.insert(e->GetId());
    }
    if (!m_selectedCADEntityIds.empty()) {
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
        ComputeGrips();
        statusBar()->showMessage(QString("%1 entity seçildi (Tümü)").arg(m_selectedCADEntityIds.size()), 2000);
    }
}

// ============================================================
//  MIRROR (AYNA)  — #7
// ============================================================
void MainWindow::OnMirror() {
    if (!m_document) return;
    if (m_selectedCADEntityIds.empty() && m_selectedCADEntityId == 0) {
        statusBar()->showMessage("Önce entity seçin, sonra MIRROR komutunu çalıştırın", 2000);
        return;
    }

    // Yansıma ekseni: yatay mı, dikey mi?
    QStringList eksenler = {"Yatay eksen (X ekseni boyunca)", "Dikey eksen (Y ekseni boyunca)",
                            "Seçimin merkezi etrafında (180° döndür)"};
    bool ok;
    QString secim = QInputDialog::getItem(this, "Mirror — Ayna", "Yansıma ekseni:", eksenler, 0, false, &ok);
    if (!ok) return;

    // Seçili entity'leri topla
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);

    // Seçimin BBox merkezi
    double cx = 0, cy = 0;
    int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        auto bb = it->second->GetBounds();
        auto c  = bb.GetCenter();
        cx += c.x; cy += c.y; ++cnt;
    }
    if (cnt > 0) { cx /= cnt; cy /= cnt; }

    int eksNo = eksenler.indexOf(secim);
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;
        if (eksNo == 0)      // yatay: Y koordinatları ters
            e->Mirror({cx - 1, cy, 0}, {cx + 1, cy, 0});
        else if (eksNo == 1) // dikey: X koordinatları ters
            e->Mirror({cx, cy - 1, 0}, {cx, cy + 1, 0});
        else                 // 180° döndür (her ikisi de)
            e->Rotate(180.0);
    }
    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(QString("%1 entity yansıtıldı").arg(ids.size()), 2000);
}

// ============================================================
//  OFFSET — paralel kopya  (#8)
// ============================================================
void MainWindow::OnOffset() {
    if (!m_document) return;

    // Seçili entity kontrolü
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);

    if (ids.empty()) {
        statusBar()->showMessage("Önce entity seçin, sonra OFFSET komutunu çalıştırın", 2000);
        return;
    }

    bool ok;
    double dist = QInputDialog::getDouble(this, "Offset — Paralel Kopya",
        "Offset mesafesi (mm):", 100.0, 0.1, 100000.0, 1, &ok);
    if (!ok) return;

    QStringList yonler = {"Sağa / Yukarıya", "Sola / Aşağıya", "Her iki yöne"};
    QString yon = QInputDialog::getItem(this, "Offset Yönü", "Yön:", yonler, 0, false, &ok);
    if (!ok) return;
    int yonIdx = yonler.indexOf(yon);

    auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
        m_document->GetCADEntities());

    std::vector<std::unique_ptr<cad::Entity>> newEnts;

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;

        auto makeOffset = [&](double d) {
            auto clone = e->Clone();
            if (!clone) return;
            if (e->GetType() == cad::EntityType::Line) {
                const auto* ln = static_cast<const cad::Line*>(e);
                auto* c = static_cast<cad::Line*>(clone.get());
                geom::Vec3 s = ln->GetStart(), en = ln->GetEnd();
                double dx = en.x - s.x, dy = en.y - s.y;
                double len = std::sqrt(dx*dx + dy*dy);
                if (len < 1e-9) return;
                double nx = -dy/len * d, ny = dx/len * d;
                c->SetStart({s.x + nx,  s.y + ny,  s.z});
                c->SetEnd  ({en.x + nx, en.y + ny, en.z});
            } else {
                clone->Move(geom::Vec3(d, 0, 0)); // diğer entity'ler için basit öteleme
            }
            newEnts.push_back(std::move(clone));
        };

        if (yonIdx == 0 || yonIdx == 2) makeOffset( dist);
        if (yonIdx == 1 || yonIdx == 2) makeOffset(-dist);
    }

    for (auto& ne : newEnts)
        entities.push_back(std::move(ne));

    m_document->SetModified(true);
    RebuildCADEntityCache();
    InvalidateRenderer();
    statusBar()->showMessage(
        QString("%1 entity offset edildi (%2 mm)").arg(ids.size() * (yonIdx == 2 ? 2 : 1)).arg(dist), 2000);
}

// ============================================================
//  COPY/PASTE  (#9)
// ============================================================
void MainWindow::OnCopy() {
    if (!m_document) return;
    m_clipboard.clear();
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
    if (ids.empty()) { statusBar()->showMessage("Kopyalanacak entity seçin", 2000); return; }

    // BBox merkezi — paste sırasında ortalama
    double cx = 0, cy = 0; int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it != m_cadEntityCache.end() && it->second) {
            auto c = it->second->GetBounds().GetCenter();
            cx += c.x; cy += c.y; ++cnt;
        }
    }
    if (cnt) { cx /= cnt; cy /= cnt; }
    m_clipboardCenter = {cx, cy, 0};

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it != m_cadEntityCache.end() && it->second) {
            auto clone = it->second->Clone();
            if (clone) m_clipboard.push_back(std::move(clone));
        }
    }
    statusBar()->showMessage(QString("%1 entity kopyalandı — Ctrl+V ile yapıştır").arg(m_clipboard.size()), 2000);
}

void MainWindow::OnPaste() {
    if (!m_document || m_clipboard.empty()) {
        statusBar()->showMessage("Panoda entity yok — önce Ctrl+C ile kopyalayın", 2000); return;
    }

    bool ok;
    double ox = QInputDialog::getDouble(this, "Yapıştır — Offset X",
        "X öteleme (mm, 0=aynı konum):", 50.0, -1e6, 1e6, 0, &ok);
    if (!ok) return;
    double oy = QInputDialog::getDouble(this, "Yapıştır — Offset Y",
        "Y öteleme (mm, 0=aynı konum):", 50.0, -1e6, 1e6, 0, &ok);
    if (!ok) return;

    auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
        m_document->GetCADEntities());

    m_selectedCADEntityIds.clear();
    for (const auto& src : m_clipboard) {
        auto clone = src->Clone();
        if (!clone) continue;
        clone->Move(geom::Vec3(ox, oy, 0));
        m_selectedCADEntityIds.insert(clone->GetId());
        entities.push_back(std::move(clone));
    }
    m_document->SetModified(true);
    RebuildCADEntityCache();
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
    InvalidateRenderer();
    statusBar()->showMessage(
        QString("%1 entity yapıştırıldı (+%2, +%3 mm)").arg(m_clipboard.size()).arg(ox).arg(oy), 2000);
}

void MainWindow::OnDelete() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();

    // Çoklu CAD entity seçimi varsa tümünü sil
    if (!m_selectedCADEntityIds.empty()) {
        auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
            m_document->GetCADEntities());
        size_t before = entities.size();
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [this](const std::unique_ptr<cad::Entity>& e) {
                return e && m_selectedCADEntityIds.count(e->GetId());
            }), entities.end());
        int deleted = static_cast<int>(before - entities.size());
        m_selectedCADEntityIds.clear();
        m_selectedCADEntityId = 0;
        ClearGrips();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds({});
        RebuildCADEntityCache();
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("%1 entity silindi").arg(deleted), 2000);
        return;
    }

    if (m_selectedNodeId != 0) {
        auto cmd = std::make_unique<core::DeleteNodeCommand>(network, m_selectedNodeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Node #%1 silindi").arg(m_selectedNodeId), 2000);
        m_selectedNodeId = 0;
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
        if (m_fixturePanel) m_fixturePanel->Clear();
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else if (m_selectedEdgeId != 0) {
        auto cmd = std::make_unique<core::DeleteEdgeCommand>(network, m_selectedEdgeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Kenar #%1 silindi").arg(m_selectedEdgeId), 2000);
        m_selectedEdgeId = 0;
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else if (m_selectedCADEntityId != 0) {
        // Tek seçili CAD entity
        auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
            m_document->GetCADEntities());
        cad::EntityId eid = m_selectedCADEntityId;
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [eid](const std::unique_ptr<cad::Entity>& e){ return e && e->GetId() == eid; }),
            entities.end());
        m_selectedCADEntityId = 0;
        ClearGrips();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
        RebuildCADEntityCache();
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("Entity #%1 silindi").arg(eid), 2000);
    } else {
        statusBar()->showMessage("Silme: önce bir öğe seçin (Select modu)");
    }
}

void MainWindow::OnDrawPipe() {
    m_currentToolMode = ToolMode::DrawPipe;
    m_drawState = DrawState::WaitingFirstPoint;
    ClearGrips();
    if (m_snapOverlay) m_snapOverlay->SetPickBoxVisible(false);
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

// ============================================================
//  ABOUT & HELP
// ============================================================
void MainWindow::OnAbout() {
    QMessageBox::about(this, "Hakkında — VKT",
        QString("<h2>%1</h2>"
                "<p><b>Sürüm:</b> %2</p>"
                "<p>Vulkan + Qt6 tabanlı profesyonel MEP CAD yazılımı.<br>"
                "4M FINE SANI'nin ötesinde: açık mimari, GPU render, tam standart uyumu.</p>"
                "<p><b>Standartlar:</b> %3</p>"
                "<p>%4</p>")
            .arg(vkt::VKT_APP_NAME)
            .arg(vkt::VKT_VERSION_STRING)
            .arg(vkt::VKT_STANDARDS)
            .arg(vkt::VKT_COPYRIGHT));
}

void MainWindow::OnHelp() {
    // Kullanıcı_kitabı.md dosyasını QTextBrowser'da aç
    QString kitapPath = QString::fromStdString(
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
            .append("Kullan\xc4\xb1\xc4\x9f\xc4\xb1_kitab\xc4\xb1.md")
            .string());

    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle("VKT Kullanıcı Kılavuzu");
    dlg->resize(900, 680);
    auto* layout = new QVBoxLayout(dlg);

    auto* browser = new QTextBrowser(dlg);
    browser->setOpenLinks(false);

    QFile f(kitapPath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString md = QTextStream(&f).readAll();
        // Markdown başlıklarını HTML'e çevir (temel)
        md.replace(QRegularExpression("^### (.+)$", QRegularExpression::MultilineOption),
                   "<h3>\\1</h3>");
        md.replace(QRegularExpression("^## (.+)$",  QRegularExpression::MultilineOption),
                   "<h2>\\1</h2>");
        md.replace(QRegularExpression("^# (.+)$",   QRegularExpression::MultilineOption),
                   "<h1>\\1</h1>");
        md.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        md.replace("\n\n", "<br><br>");
        browser->setHtml("<html><body style='font-family:Arial;font-size:10pt'>" + md + "</body></html>");
    } else {
        browser->setText("Kullanıcı kılavuzu bulunamadı.\n\nBeklenen konum:\n" + kitapPath);
    }

    layout->addWidget(browser);
    auto* btnClose = new QPushButton("Kapat", dlg);
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(btnClose);
    dlg->exec();
    delete dlg;
}

void MainWindow::OnSelectMode() {
    m_currentToolMode  = ToolMode::Select;
    m_drawState        = DrawState::Idle;
    m_firstNodeInGraph = false;
    if (m_snapOverlay) {
        m_snapOverlay->ClearRubberBand();
        m_snapOverlay->SetPickBoxVisible(true);  // Select modunda pick box göster
    }
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

    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty() || network.GetNodeMap().empty()) {
        QMessageBox::warning(this, "Hidrolik Analiz",
            "Ağda boru veya armatür bulunamadı.\n"
            "Önce boru çizin ve armatür ekleyin, ardından analizi tekrar çalıştırın.");
        return;
    }

    m_logList->clear();
    m_logList->addItem("═══════════════════════════════════════");
    m_logList->addItem("  FULL MEP ANALİZ BAŞLIYOR");
    m_logList->addItem("═══════════════════════════════════════");

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

    statusBar()->showMessage("Hidrolik analiz tamamlandı!", 3000);
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
    statusBar()->showMessage(QString("DN boyutlandırma tamamlandı: %1 boru güncellendi").arg(sized), 3000);
}

// ============================================================
//  GERÇEK ZAMANLI HİDROLİK (DEBOUNCED AUTO-DN)
// ============================================================

bool MainWindow::RunPreflight() {
    if (!m_document) return false;
    auto& network = m_document->GetNetwork();
    PreflightCheckDialog dlg(network, m_hydraulicRanRecently, this);
    dlg.exec();
    return dlg.CanProceed();
}

void MainWindow::ScheduleAutoHydro() {
    m_hydraulicRanRecently = false; // ağ değişti → hesap stale
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
    solver.SolveGas();
    solver.SolveHeating();

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
            if (node.type == mep::NodeType::Source || node.type == mep::NodeType::HotSource) {
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

    // Kritik devre edge'lerini renderer'a bildir → turuncu-kırmızı vurgulama
    if (m_vulkanWindow) {
        mep::HydraulicSolver critSolver(network);
        critSolver.Solve();
        auto critResult = critSolver.CalculateCriticalPath();
        m_vulkanWindow->SetCriticalPathEdges(critResult.criticalPath);
        m_vulkanWindow->requestUpdate();
    }

    m_hydraulicRanRecently = true;
}

// ============================================================
//  ESC — ÇİZİM MODU İPTAL
// ============================================================
void MainWindow::keyPressEvent(QKeyEvent* event) {
    // F8 — Ortho modu toggle (yatay/dikey kısıt)
    if (event->key() == Qt::Key_F8) {
        m_orthoMode = !m_orthoMode;
        statusBar()->showMessage(m_orthoMode
            ? "Ortho AÇIK (F8) — sadece yatay/dikey çizim"
            : "Ortho KAPALI (F8)", 2000);
        event->accept();
        return;
    }

    // F11 — tam ekran toggle (AutoCAD Ctrl+0 benzeri)
    if (event->key() == Qt::Key_F11) {
        m_isFullScreen = !m_isFullScreen;
        if (m_isFullScreen)
            showFullScreen();
        else
            showNormal();
        event->accept();
        return;
    }

    // Delete — seçili CAD entity(ler) veya MEP node sil
    if (event->key() == Qt::Key_Delete && m_document) {
        // Çoklu box seçimi (önce kontrol et)
        if (!m_selectedCADEntityIds.empty()) {
            auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
                m_document->GetCADEntities());
            size_t before = entities.size();
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                [this](const std::unique_ptr<cad::Entity>& e) {
                    return e && m_selectedCADEntityIds.count(e->GetId());
                }), entities.end());
            size_t deleted = before - entities.size();
            m_selectedCADEntityIds.clear();
            m_selectedCADEntityId = 0;
            ClearGrips();
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds({});
            m_document->SetModified(true);
            RebuildCADEntityCache();
            InvalidateRenderer();
            UpdateUI();
            statusBar()->showMessage(QString("%1 CAD entity silindi").arg(deleted), 2000);
            event->accept();
            return;
        }
        // Tek tıklama seçimi
        if (m_selectedCADEntityId != 0) {
            auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
                m_document->GetCADEntities());
            auto it = std::find_if(entities.begin(), entities.end(),
                [this](const std::unique_ptr<cad::Entity>& e) {
                    return e && e->GetId() == m_selectedCADEntityId;
                });
            if (it != entities.end()) {
                entities.erase(it);
                m_selectedCADEntityId = 0;
                if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                    m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
                m_document->SetModified(true);
                RebuildCADEntityCache();
                InvalidateRenderer();
                UpdateUI();
                statusBar()->showMessage("CAD entity silindi", 2000);
            }
            event->accept();
            return;
        }
        if (m_selectedNodeId != 0) {
            auto& network = m_document->GetNetwork();
            auto cmd = std::make_unique<core::DeleteNodeCommand>(network, m_selectedNodeId);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            m_selectedNodeId = 0;
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage("Node silindi", 2000);
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Escape) {
        // Grip drag iptal
        if (m_gripDragging) {
            m_gripDragging = false;
            m_activeGrip   = -1;
            // Orijinal vertex'lere geri dön
            if (m_activeGrip >= 0 && m_activeGrip < static_cast<int>(m_grips.size())) {
                auto it = m_cadEntityCache.find(m_grips[m_activeGrip].entityId);
                if (it != m_cadEntityCache.end() && it->second &&
                    it->second->GetType() == cad::EntityType::Line) {
                    auto* ln = static_cast<cad::Line*>(it->second);
                    ln->SetStart(m_gripEntityOrig[0]);
                    ln->SetEnd  (m_gripEntityOrig[2]);
                    InvalidateRenderer();
                }
            }
            ClearGrips();
            statusBar()->showMessage("Grip iptal edildi", 1500);
            event->accept();
            return;
        }
        // Seçim kutusunu veya çoklu seçimi iptal et
        if (m_selBoxActive) {
            m_selBoxActive = false;
            if (m_snapOverlay) m_snapOverlay->ClearSelectionBox();
            event->accept();
            return;
        }
        if (!m_selectedCADEntityIds.empty() || m_selectedCADEntityId != 0) {
            ClearGrips();
            m_selectedCADEntityIds.clear();
            m_selectedCADEntityId = 0;
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds({});
            HighlightLayerInPanel("");
            statusBar()->showMessage("Seçim temizlendi", 1500);
            event->accept();
            return;
        }
        if (m_currentToolMode == ToolMode::DrawPolyArea) {
            m_polyAreaPoints.clear();
            if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }
        }
        // PlaceFixture yön seçimi (2. tıklama) bekleniyorsa sadece 1. adıma dön
        if (m_currentToolMode == ToolMode::PlaceFixture &&
            m_drawState == DrawState::WaitingSecondPoint) {
            m_drawState = DrawState::WaitingFirstPoint;
            if (m_snapOverlay) m_snapOverlay->ClearRubberBand();
            statusBar()->showMessage("Yön seçimi iptal edildi — Yerleştirme noktasını tıklayın", 2000);
            event->accept();
            return;
        }
        if (m_currentToolMode != ToolMode::Select) {
            m_currentToolMode = ToolMode::Select;
            m_drawState       = DrawState::Idle;
            m_firstNodeInGraph = false;
            if (m_snapOverlay) m_snapOverlay->ClearRubberBand();
            if (m_snapOverlay) m_snapOverlay->Hide();
            statusBar()->showMessage("Çizim iptal edildi — Seçim modu");
        }
        // ESC seçimi de temizler
        if (m_selectedCADEntityId != 0) {
            m_selectedCADEntityId = 0;
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
        }
        event->accept();
        return;
    }
    // Enter / Return — DrawPolyArea polygon kapatma
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_currentToolMode == ToolMode::DrawPolyArea) {
            FinishPolyArea();
            event->accept();
            return;
        }
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
        statusBar()->showMessage(QString("Rapor kaydedildi: %1").arg(filePath), 3000);
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

    // 2. Detaylı proje bilgilerini al
    NewProjectDialog dlg(QString::fromStdString(pm.GetProjectsRoot()), this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString name = dlg.ProjectName();
    if (name.isEmpty()) return;

    // 3. Proje klasörünü oluştur
    std::string error;
    if (!pm.CreateProject(name.toStdString(), error)) {
        QMessageBox::critical(this, "Proje Oluşturulamadı", QString::fromStdString(error));
        return;
    }

    // Norm seçimini uygula
    if (dlg.Norm().contains("DIN"))
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
    else if (dlg.Norm().contains("Sarfiyat") || dlg.Norm().contains("825"))
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::SARFIYAT;
    else
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;

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
    auto gn = mep::HydraulicSolver::GlobalNorm();
    QString normLabel = (gn == mep::HydroNorm::DIN1988)   ? "DIN 1988-300"
                      : (gn == mep::HydroNorm::SARFIYAT)  ? "TS 825 Sarfiyat"
                                                           : "EN 806-3";
    setWindowTitle(QString("VKT - FINE SANI++ — %1").arg(displayName));
    statusBar()->showMessage(
        QString("Proje oluşturuldu: %1 | Norm: %2 | Müşteri: %3")
            .arg(QString::fromStdString(pm.GetProjectFolder()))
            .arg(normLabel)
            .arg(dlg.CustomerName().isEmpty() ? "-" : dlg.CustomerName()));

    if (m_logList) {
        m_logList->addItem(QString("Yeni proje: %1").arg(
            QString::fromStdString(pm.GetProjectFolder())));
        if (!dlg.CustomerName().isEmpty())
            m_logList->addItem(QString("  Müşteri: %1  |  Mühendis: %2")
                .arg(dlg.CustomerName()).arg(dlg.EngineerName()));
        m_logList->addItem(QString("  Bina Tipi: %1  |  Norm: %2  |  Tarih: %3")
            .arg(dlg.BuildingType()).arg(normLabel).arg(dlg.ProjectDate()));
    }
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
        RefreshFloorSelector();   // Kat seçici toolbar güncelle
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

void MainWindow::OnFloorAlignment() {
    if (m_floorManager.GetFloors().empty()) {
        QMessageBox::information(this, "3D Hizalama Kontrolü",
            "Henüz kat tanımlanmamış.\n"
            "Önce Mimari → Mimari Belirle (Ctrl+M) ile katları tanımlayın.");
        return;
    }

    if (!m_document) {
        QMessageBox::warning(this, "3D Hizalama Kontrolü", "Aktif belge yok.");
        return;
    }

    FloorAlignmentDialog dlg(m_floorManager, m_document->GetNetwork(), this);
    if (dlg.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Kat yükseklikleri güncellendi.");
        LogCAD("3D hizalama: kat yükseklikleri guncellendi");
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

void MainWindow::OnDrawHotWaterPipe() {
    m_currentPipeType  = mep::EdgeType::HotWater;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Sicak su borusu: Baslangic noktasini tiklayin (sofbenden baslayin) (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Sicak su bas.");
}

void MainWindow::OnPlaceHotSource() {
    if (!m_document) return;
    m_currentToolMode = ToolMode::PlaceJunction; // Junction modunu ödünç al — click koordinatını yakalar
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Sofben / Kazan: Konumunu tiklayin (sicak su baslangic noktasi)");
    if (m_commandBar) m_commandBar->SetPrompt("Sofben konumu");
    // Özel flag: junction yerine HotSource node koysun
    m_drainLabel = "__HOT_SOURCE__";
}

// ═══════════════════════════════════════════════════════════
//  DOĞAL GAZ MODÜLÜ (TS EN 1775)
// ═══════════════════════════════════════════════════════════
void MainWindow::OnDrawGasPipe() {
    m_currentPipeType  = mep::EdgeType::Gas;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Dogalgaz borusu (sari): Baslangic noktasini tiklayin — TS EN 1775 (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Gaz boru bas.");
}

void MainWindow::OnPlaceGasSource() {
    if (!m_document) return;
    m_currentToolMode = ToolMode::PlaceJunction;
    m_drawState       = DrawState::WaitingFirstPoint;
    m_drainLabel      = "__GAS_SOURCE__";
    statusBar()->showMessage("Gaz sayaci / sehir gazi giris noktasi: Konumunu tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Gaz sayaci");
}

void MainWindow::OnPlaceGasAppliance() {
    if (!m_document) return;
    // Cihaz tipi seçimi
    QStringList items = { "Kombi 24kW", "Kombi 28kW", "Kombi 35kW",
                          "Sofben 11L", "Sofben 13L",
                          "Ocak (4 gozlu)", "Ocak + Firin",
                          "Dogalgaz Sobasi", "Endustriyel Ocak",
                          "Merkezi Kazan 100kW", "Merkezi Kazan 200kW" };
    bool ok = false;
    QString sel = QInputDialog::getItem(this, "Gaz Cihazi Sec",
                                        "Cihaz tipi:", items, 0, false, &ok);
    if (!ok || sel.isEmpty()) return;

    mep::GasApplianceData data = mep::Database::Instance().GetGasAppliance(sel.toStdString());
    m_selectedFixtureType = QString("GasAppliance:") + sel;
    m_currentToolMode = ToolMode::PlaceFixture;
    m_drawState       = DrawState::WaitingFirstPoint;
    m_drainLabel = QString("__GAS_APPLIANCE__:") + sel;
    statusBar()->showMessage(
        QString("Gaz cihazi [%1, %.2f m³/h]: Konumunu tiklayin")
            .arg(sel).arg(data.gasFlow_m3h));
    if (m_commandBar) m_commandBar->SetPrompt("Gaz cihazi koy");
}

// ═══════════════════════════════════════════════════════════
//  ISITMA SİSTEMİ (EN 12831)
// ═══════════════════════════════════════════════════════════
void MainWindow::OnDrawHeatingPipe() {
    m_currentPipeType  = mep::EdgeType::Heating;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Isitma gidis borusu (kirmizi-turuncu): Baslangic noktasini tiklayin — EN 12831 (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Isitma gidis");
}

void MainWindow::OnDrawHeatingReturn() {
    m_currentPipeType  = mep::EdgeType::HeatingReturn;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Isitma donus borusu (mavi): Baslangic noktasini tiklayin (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Isitma donus");
}

void MainWindow::OnPlaceBoiler() {
    if (!m_document) return;
    QStringList opts = { "Kombi / Merkezi Kazan", "Elektrikli Kazan", "Gunes + Depo" };
    bool ok = false;
    QString sel = QInputDialog::getItem(this, "Kazan Tipi", "Kazan / Isitma Kaynagi:", opts, 0, false, &ok);
    if (!ok) return;
    m_drainLabel      = QString("__BOILER__:") + sel;
    m_currentToolMode = ToolMode::PlaceJunction;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Kazan konumu: Tiklayin (isitma gidis/donus baslangic noktasi)");
    if (m_commandBar) m_commandBar->SetPrompt("Kazan konumu");
}

void MainWindow::OnPlaceRadiator() {
    if (!m_document) return;
    bool ok = false;
    double power = QInputDialog::getDouble(this, "Radyator Isi Yuku",
                                           "Radyator gucu (kW):", 1.5, 0.1, 20.0, 1, &ok);
    if (!ok) return;
    m_drainLabel      = QString("__RADIATOR__:") + QString::number(power);
    m_currentToolMode = ToolMode::PlaceFixture;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage(
        QString("Radyator [%1 kW]: Konumunu tiklayin").arg(power, 0, 'f', 1));
    if (m_commandBar) m_commandBar->SetPrompt("Radyator koy");
}

// ═══════════════════════════════════════════════════════════
//  YANGIN / SPRİNKLER MODÜLÜ (EN 12845)
// ═══════════════════════════════════════════════════════════
void MainWindow::OnDrawFireLine() {
    m_currentPipeType  = mep::EdgeType::FireLine;
    m_currentToolMode  = ToolMode::DrawPipe;
    m_drawState        = DrawState::WaitingFirstPoint;
    m_firstNodeInGraph = false;
    statusBar()->showMessage("Yangin hatti (koyu kirmizi): Baslangic noktasini tiklayin — EN 12845 (ESC=iptal)");
    if (m_commandBar) m_commandBar->SetPrompt("Yangin hatti");
}

void MainWindow::OnPlaceSprinkler() {
    if (!m_document) return;
    m_drainLabel      = "__SPRINKLER__";
    m_currentToolMode = ToolMode::PlaceDrain;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Sprinkler basligi: Konumunu tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Sprinkler");
}

void MainWindow::OnPlaceFirePump() {
    if (!m_document) return;
    m_drainLabel      = "__FIRE_PUMP__";
    m_currentToolMode = ToolMode::PlaceJunction;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Yangin pompasi / tank giris: Konumunu tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Yangin pompasi");
}

void MainWindow::OnFireHazardClass() {
    if (!m_document) return;
    QStringList hazards = { "LH", "OH1", "OH2", "OH3", "OH4", "HH" };
    bool ok = false;
    QString sel = QInputDialog::getItem(this, "EN 12845 Tehlike Sinifi",
        "Sprinkler Tehlike Sinifi:\n"
        "  LH  = Dusuk tehlike (depo, ofis)\n"
        "  OH1 = Orta tehlike (hastane, okul)\n"
        "  OH2 = Orta tehlike+ (restoran, otel)\n"
        "  OH3 = Orta tehlike++ (boya, kaucuk)\n"
        "  OH4 = Yuksek orta tehlike\n"
        "  HH  = Yuksek tehlike (depo, uretim)",
        hazards, 1, false, &ok);
    if (!ok) return;

    mep::HydraulicSolver solver(m_document->GetNetwork());
    solver.SolveFire(sel.toStdString());
    RefreshTextOverlay();
    if (m_logList) m_logList->addItem(QString("EN 12845 Yangin hesabi: %1 sinifi").arg(sel));
    statusBar()->showMessage(QString("Yangin hesabi tamamlandi [%1]").arg(sel));
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

//  AKILLİ BAĞLANTI NOKTASI — cihaz sembolü yerine sadece artı/yıldız işareti
void MainWindow::OnPlaceSmartPoint() {
    if (!m_document) return;
    m_selectedFixtureType = "SmartPoint";
    m_currentToolMode = ToolMode::PlaceFixture;
    m_drawState       = DrawState::WaitingFirstPoint;
    statusBar()->showMessage("Akilli Baglanti Noktasi: Cihazin pis su cikis noktasini tiklayin (magenta yildiz)");
    if (m_commandBar) m_commandBar->SetPrompt("Akilli baglanti");
}

//  BOŞALTMA NOKTASI — en alttaki Drain'i ana tahliye olarak işaretle
void MainWindow::OnBosaltmaNoktasi() {
    if (!m_document) return;
    const auto& network = m_document->GetNetwork();
    uint32_t lowestId = 0;
    double   lowestZ  = std::numeric_limits<double>::max();
    for (const auto& [nid, node] : network.GetNodeMap()) {
        if (node.type != mep::NodeType::Drain) continue;
        if (node.position.z < lowestZ) {
            lowestZ  = node.position.z;
            lowestId = nid;
        }
    }
    if (lowestId == 0) {
        QMessageBox::information(this, "Bosaltma Noktasi",
            "Hic Drain node bulunamadi. Once Rogar veya Yer Suzgeci ekleyin.");
        return;
    }
    m_mainDrainNodeId = lowestId;
    const auto* n = network.GetNode(lowestId);
    RefreshTextOverlay();
    statusBar()->showMessage(
        QString("Ana Tahliye Noktasi isaretlendi: node #%1 (z=%2m)")
        .arg(lowestId)
        .arg(n ? n->position.z : lowestZ, 0, 'f', 2));
    m_document->SetModified(true);
}

//  UYGULAMA KATMAN GÖRÜNÜRLÜĞܠ— Supply/HotWater/Drainage katmanlarını bağımsız göster/gizle
void MainWindow::OnLayerVisibility() {
    QDialog dlg(this);
    dlg.setWindowTitle("Katman Gorunurlugu");
    dlg.setMinimumWidth(280);

    auto* cb1 = new QCheckBox("Temiz Su (Supply) — mavi boru");
    auto* cb2 = new QCheckBox("Sicak Su (HotWater) — kirmizi boru");
    auto* cb3 = new QCheckBox("Pis Su (Drainage) — kahverengi boru");
    cb1->setChecked(m_showTemizSu);
    cb2->setChecked(m_showSicakSu);
    cb3->setChecked(m_showPisSu);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel("<b>Hangi katmanlar gorunsun?</b>"));
    lay->addWidget(cb1);
    lay->addWidget(cb2);
    lay->addWidget(cb3);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    m_showTemizSu = cb1->isChecked();
    m_showSicakSu = cb2->isChecked();
    m_showPisSu   = cb3->isChecked();

    // Renderer'a bildir — GPU vertex buffer yeniden oluşturulacak
    if (m_vulkanWindow)
        m_vulkanWindow->SetLayerVisibility(m_showTemizSu, m_showSicakSu, m_showPisSu);

    RefreshTextOverlay();
    statusBar()->showMessage(
        QString("Katman gorunurlugu: TemizSu=%1 SicakSu=%2 PisSu=%3")
        .arg(m_showTemizSu ? "ACIK" : "KAPALI")
        .arg(m_showSicakSu ? "ACIK" : "KAPALI")
        .arg(m_showPisSu   ? "ACIK" : "KAPALI"), 3000);
}

void MainWindow::OnActiveFloorChanged(int index) {
    m_activeFloorIndex = index;
    const core::Floor* f = m_floorManager.GetFloor(index);
    if (f) {
        statusBar()->showMessage(
            QString("Aktif kat: %1 (z = %2 m)")
                .arg(QString::fromStdString(f->label))
                .arg(f->elevation_m, 0, 'f', 2), 2000);
    }
}

void MainWindow::RefreshFloorSelector() {
    if (!m_floorSelector) return;
    m_floorSelector->blockSignals(true);
    m_floorSelector->clear();
    const auto& floors = m_floorManager.GetFloors();
    if (floors.empty()) {
        m_floorSelector->addItem("Zemin Kat (z=0.00 m)");
    } else {
        for (int i = 0; i < static_cast<int>(floors.size()); ++i) {
            const auto& f = floors[i];
            m_floorSelector->addItem(
                QString("%1 — %2 (%3 m)")
                    .arg(i)
                    .arg(QString::fromStdString(f.label))
                    .arg(f.elevation_m, 0, 'f', 2));
        }
    }
    // Aktif index'i sınırla
    m_activeFloorIndex = std::min(m_activeFloorIndex,
                                  m_floorSelector->count() - 1);
    m_floorSelector->setCurrentIndex(m_activeFloorIndex);
    m_floorSelector->blockSignals(false);
}

double MainWindow::GetActiveFloorZ() const {
    const core::Floor* f = m_floorManager.GetFloor(m_activeFloorIndex);
    return f ? f->elevation_m : 0.0;
}

void MainWindow::OnTesistatKabul() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // ── 1. Doğrulama ─────────────────────────────────────────
    int errorCount   = 0;
    int warningCount = 0;
    QStringList errors, warnings;

    // Kaynakları bul
    std::vector<uint32_t> coldSources, hotSources;
    for (const auto& [nid, node] : network.GetNodeMap()) {
        if (node.type == mep::NodeType::Source)    coldSources.push_back(nid);
        if (node.type == mep::NodeType::HotSource) hotSources.push_back(nid);
    }

    if (coldSources.empty())
        errors << "Soguk su baslangic noktasi (Source) tanimlanmamis!";
    if (coldSources.size() > 1)
        warnings << QString("Birden fazla soguk su kaynagi var (%1 adet)").arg(coldSources.size());

    // Açık uç kontrolü: sadece Supply/HotWater kenarlara bağlı Junction/Fixture
    for (const auto& [nid, node] : network.GetNodeMap()) {
        if (node.type == mep::NodeType::Source || node.type == mep::NodeType::HotSource
            || node.type == mep::NodeType::Drain) continue;

        auto edges = network.GetConnectedEdges(nid);
        int supplyEdges = 0;
        for (uint32_t eid : edges) {
            const mep::Edge* e = network.GetEdge(eid);
            if (e && (e->type == mep::EdgeType::Supply || e->type == mep::EdgeType::HotWater
                      || e->type == mep::EdgeType::Drainage))
                supplyEdges++;
        }
        if (supplyEdges == 0)
            errors << QString("Baglantisiz dugum ID=%1 (%2)").arg(nid)
                          .arg(QString::fromStdString(node.label));
        else if (supplyEdges == 1 && node.type == mep::NodeType::Junction)
            warnings << QString("Aci uc Junction ID=%1 — boru bagli degil").arg(nid);
    }

    errorCount   = errors.size();
    warningCount = warnings.size();

    // ── 2. Parça numaralandırma ───────────────────────────────
    int pipeNum = 1, nodeNum = 1;
    for (auto& [eid, edge] : network.GetEdgeMap()) {
        // Mevcut DN etiketini koru, sadece numara ekle
        std::string prefix = (edge.type == mep::EdgeType::HotWater) ? "SK-"
                           : (edge.type == mep::EdgeType::Drainage)  ? "PS-"
                                                                      : "P-";
        // Etiket yoksa veya numara eki yoksa ata
        if (edge.label.empty() || edge.label.find(prefix) == std::string::npos) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%s%03d", prefix.c_str(), pipeNum);
            if (!edge.label.empty())
                edge.label = std::string(buf) + " " + edge.label;
            else
                edge.label = buf;
        }
        pipeNum++;
    }
    for (auto& [nid, node] : network.GetNodeMap()) {
        if (node.label == "J" || node.label.empty()) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "N-%03d", nodeNum);
            node.label = buf;
        }
        nodeNum++;
    }

    // ── 3. Otomatik DN hesabı ─────────────────────────────────
    RunAutoHydro();
    RefreshTextOverlay();
    m_document->SetModified(true);

    // ── 4. Özet dialog ───────────────────────────────────────
    QString summary;
    summary += QString("<b>Tesisat Kabul Özeti</b><br><br>");
    summary += QString("Boru sayısı : <b>%1</b><br>").arg(network.GetEdgeCount());
    summary += QString("Düğüm sayısı: <b>%1</b><br>").arg(network.GetNodeCount());
    summary += QString("Soğuk su kaynağı: <b>%1</b><br>").arg(coldSources.size());
    summary += QString("Sıcak su kaynağı: <b>%1</b><br><br>").arg(hotSources.size());

    if (errorCount == 0 && warningCount == 0) {
        summary += "<font color='green'><b>✔ Tesisat hatasız kabul edildi.</b></font><br>";
        summary += "<br>DN boyutlandırma tamamlandı. Hesap modülü hazır.";
    } else {
        if (errorCount > 0) {
            summary += QString("<font color='red'><b>%1 HATA:</b></font><br>").arg(errorCount);
            for (const auto& e : errors)
                summary += "• " + e + "<br>";
            summary += "<br>";
        }
        if (warningCount > 0) {
            summary += QString("<font color='orange'><b>%1 Uyarı:</b></font><br>").arg(warningCount);
            for (const auto& w : warnings)
                summary += "• " + w + "<br>";
        }
        if (errorCount > 0)
            summary += "<br><i>Hataları düzelttikten sonra tekrar 'Tesisatı Kabul Et' yapın.</i>";
    }

    QMessageBox dlg(this);
    dlg.setWindowTitle("Tesisatı Kabul Et");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(summary);
    dlg.setIcon(errorCount > 0 ? QMessageBox::Warning : QMessageBox::Information);
    dlg.exec();

    statusBar()->showMessage(errorCount == 0
        ? QString("Tesisat kabul edildi: %1 boru, %2 dugum numaralandirildi")
              .arg(network.GetEdgeCount()).arg(network.GetNodeCount())
        : QString("Tesisat HATALI: %1 hata duzeltilmeli").arg(errorCount), 5000);
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
        statusBar()->showMessage("Mahal silindi", 2000);
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

// ── CAD entity yakın noktası (pick aperture) ──────────────────────────────────
static double DistToCADEntity(double wx, double wy,
                               const cad::Entity* ent) {
    if (!ent) return 1e18;
    auto bb = ent->GetBounds();
    auto ctr = bb.GetCenter();
    // Line'larda merkez yerine bbox merkezi iyi yeterli (tolerance geniş tutulur)
    double dx = wx - ctr.x, dy = wy - ctr.y;
    return std::sqrt(dx*dx + dy*dy);
}

void MainWindow::HandleMousePress(double worldX, double worldY, Qt::MouseButton button) {
    if (!m_document) return;

    // ── Sağ tık context menüsü ────────────────────────────────────────────────
    if (button == Qt::RightButton) {
        QMenu ctxMenu(this);

        if (m_currentToolMode != ToolMode::Select) {
            // Çizim modundayken: iptal seçeneği
            ctxMenu.addAction(QIcon::fromTheme("process-stop"), "Çizimi İptal Et (ESC)", this, [this]() {
                m_currentToolMode = ToolMode::Select;
                m_drawState = DrawState::Idle;
                m_firstNodeInGraph = false;
                m_polyAreaPoints.clear();
                if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }
                statusBar()->showMessage("Çizim iptal edildi — Seçim modu");
            });
            ctxMenu.exec(QCursor::pos());
            return;
        }

        // Seçim modundayken: yakın MEP node veya CAD entity kontekst menüsü
        auto& network = m_document->GetNetwork();
        const double nodePickR = 3000.0; // 3m world birim (zoom'a göre)

        // En yakın MEP node'unu bul
        uint32_t nearNodeId = 0;
        double nearDist = nodePickR;
        for (const auto& [nid, nd] : network.GetNodeMap()) {
            double dx = nd.position.x - worldX, dy = nd.position.y - worldY;
            double d = std::sqrt(dx*dx + dy*dy);
            if (d < nearDist) { nearDist = d; nearNodeId = nid; }
        }

        if (nearNodeId != 0) {
            const mep::Node& nd = *network.GetNode(nearNodeId);
            ctxMenu.setTitle(QString("Node #%1").arg(nearNodeId));
            ctxMenu.addAction(QString("Özellikler: %1 — %2")
                .arg(QString::fromStdString(nd.fixtureType.empty() ? nd.label : nd.fixtureType))
                .arg(nearNodeId))->setEnabled(false);
            ctxMenu.addSeparator();
            ctxMenu.addAction("Node Sil", this, [this, nearNodeId]() {
                auto& net = m_document->GetNetwork();
                auto cmd = std::make_unique<core::DeleteNodeCommand>(net, nearNodeId);
                m_document->ExecuteCommand(std::move(cmd));
                m_document->SetModified(true);
                m_selectedNodeId = 0;
                UpdateUI();
                ScheduleAutoHydro();
                statusBar()->showMessage(QString("Node #%1 silindi").arg(nearNodeId), 2000);
            });
            ctxMenu.addAction("Seçim Temizle", this, [this]() {
                m_selectedNodeId = 0;
                m_selectedEdgeId = 0;
            });
        } else if (m_selectedCADEntityId != 0) {
            ctxMenu.addAction(QString("Seçili CAD entity #%1").arg(m_selectedCADEntityId))->setEnabled(false);
            ctxMenu.addSeparator();
            ctxMenu.addAction("CAD Entity Sil (Delete)", this, [this]() {
                auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
                    m_document->GetCADEntities());
                auto it = std::find_if(entities.begin(), entities.end(),
                    [this](const std::unique_ptr<cad::Entity>& e) {
                        return e && e->GetId() == m_selectedCADEntityId;
                    });
                if (it != entities.end()) {
                    entities.erase(it);
                    m_selectedCADEntityId = 0;
                    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                        m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
                    m_document->SetModified(true);
                    InvalidateRenderer();
                    UpdateUI();
                }
            });
            ctxMenu.addAction("Seçimi Kaldır (ESC)", this, [this]() {
                m_selectedCADEntityId = 0;
                if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                    m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
            });
        } else {
            // Boş alan — genel menü
            ctxMenu.addAction("Zoom Extents (Tümünü Göster)", this, &MainWindow::OnZoomExtents);
            ctxMenu.addSeparator();
            ctxMenu.addAction("Seçim Modu", this, &MainWindow::OnSelectMode);
            ctxMenu.addAction("Boru Çiz", this, &MainWindow::OnDrawPipe);
            ctxMenu.addAction("Pis Su Borusu", this, &MainWindow::OnDrawDrainPipe);
            ctxMenu.addSeparator();
            ctxMenu.addAction("Geri Al (Ctrl+Z)", this, &MainWindow::OnUndo);
            ctxMenu.addAction("Yinele (Ctrl+Y)", this, &MainWindow::OnRedo);
        }
        ctxMenu.exec(QCursor::pos());
        return;
    }

    if (button != Qt::LeftButton) return;

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
            m_firstClickPos  = geom::Vec3(worldX, worldY, GetActiveFloorZ());
            m_drawFirstPoint = m_firstClickPos; // Ortho modu için referans nokta
            m_firstNodeInGraph = false;
            m_drawState = DrawState::WaitingSecondPoint;
            // Rubber-band başlangıcı için ekran koordinatı
            const auto& vp = m_vulkanWindow->GetViewport();
            geom::Vec3 sp = vp.WorldToScreen(m_firstClickPos);
            m_firstClickScreen = QPoint(static_cast<int>(sp.x), static_cast<int>(sp.y));
            statusBar()->showMessage(QString("Boru: İkinci noktayı tıklayın (ESC = iptal)"));
        } else if (m_drawState == DrawState::WaitingSecondPoint) {
            // Boru malzeme ayarları — özellik panelinden al
            const std::string material = m_propMaterial ? m_propMaterial->currentText().toStdString() : "PVC";
            const double roughness = mep::Database::Instance().GetPipe(material).roughness_mm;
            const double diameter  = 20.0;

            geom::Vec3 secondPos(worldX, worldY, GetActiveFloorZ());
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
        if (m_drawState == DrawState::WaitingFirstPoint || m_drawState == DrawState::Idle) {
            // 1. tıklama — konumu kaydet, yön için ikinci tıklamayı bekle
            m_firstClickPos = geom::Vec3(worldX, worldY, GetActiveFloorZ());
            m_drawState = DrawState::WaitingSecondPoint;
            const auto& vp = m_vulkanWindow->GetViewport();
            geom::Vec3 sp = vp.WorldToScreen(m_firstClickPos);
            m_firstClickScreen = QPoint(static_cast<int>(sp.x), static_cast<int>(sp.y));
            if (m_snapOverlay) m_snapOverlay->show();
            statusBar()->showMessage(QString("Armatür [%1]: Yön için ikinci noktayı tıklayın (veya ESC)")
                .arg(m_selectedFixtureType));
        } else if (m_drawState == DrawState::WaitingSecondPoint) {
            // 2. tıklama — açıyı hesapla ve armatürü ekle
            double dx = worldX - m_firstClickPos.x;
            double dy = worldY - m_firstClickPos.y;
            double angle_deg = (std::abs(dx) > 0.1 || std::abs(dy) > 0.1)
                ? std::atan2(dy, dx) * 180.0 / M_PI : 0.0;

            mep::Node node;
            node.position     = m_firstClickPos;
            node.rotation_deg = angle_deg;

            // Gaz cihazı veya radyatör modu
            if (m_drainLabel.startsWith("__GAS_APPLIANCE__:")) {
                std::string appName = m_drainLabel.mid(18).toStdString();
                node.type        = mep::NodeType::GasAppliance;
                node.fixtureType = appName;
                node.label       = appName;
                auto appData = mep::Database::Instance().GetGasAppliance(appName);
                node.gasPower_kW = appData.power_kW;
                node.gasFlow_m3h = appData.gasFlow_m3h;
                m_drainLabel.clear();
            } else if (m_drainLabel.startsWith("__RADIATOR__:")) {
                double pwr = std::stod(m_drainLabel.mid(13).toStdString());
                node.type        = mep::NodeType::Radiator;
                node.fixtureType = "Radiator";
                node.label       = QString("Rad %1kW").arg(pwr, 0, 'f', 1).toStdString();
                node.heatPower_kW = pwr;
                m_drainLabel.clear();
            } else {
                node.type        = mep::NodeType::Fixture;
                std::string typeName = m_selectedFixtureType.toStdString();
                node.fixtureType = typeName;
                node.label       = typeName;
                auto& db = mep::Database::Instance();
                auto fixture = db.GetFixture(typeName);
                node.loadUnit = fixture.loadUnit;
            }
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            if (m_snapOverlay) m_snapOverlay->ClearRubberBand();
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("Armatür eklendi: %1 (yön: %2°)")
                .arg(m_selectedFixtureType).arg(angle_deg, 0, 'f', 1));
            // Birleşik Mod: anında ConnectFixture moduna geç
            if (m_birleskMod) {
                uint32_t newId = 0;
                double bestD2 = 1e18;
                for (const auto& [nid, nd] : network.GetNodeMap()) {
                    if (nd.type != mep::NodeType::Fixture) continue;
                    double dx = nd.position.x - m_firstClickPos.x;
                    double dy = nd.position.y - m_firstClickPos.y;
                    double d2 = dx*dx + dy*dy;
                    if (d2 < bestD2) { bestD2 = d2; newId = nid; }
                }
                if (newId) {
                    m_batchFixtureIds.clear();
                    m_batchFixtureIds.push_back(newId);
                    m_currentToolMode = ToolMode::ConnectFixture;
                    m_drawState = DrawState::WaitingFirstPoint;
                    statusBar()->showMessage(QString("Birleşik Mod: Boru hattını tıklayın → armatür otomatik bağlanır"));
                    break;
                }
            }
            m_drawState = DrawState::WaitingFirstPoint;
        }
        break;
    }
    case ToolMode::PlaceJunction: {
        mep::Node node;
        node.position = geom::Vec3(worldX, worldY, GetActiveFloorZ());
        if (m_drainLabel == "__HOT_SOURCE__") {
            node.type = mep::NodeType::HotSource;
            m_drainLabel.clear();
            // Kaynak tipi seçimi
            QStringList srcTypes;
            srcTypes << "Doğalgaz Kombi (merkezi)"
                     << "Elektrikli Su Isıtıcısı — 6 kW"
                     << "Elektrikli Su Isıtıcısı — 12 kW"
                     << "Güneş Enerjisi + Depolu";
            bool ok = false;
            QString srcType = QInputDialog::getItem(this,
                "Sıcak Su Kaynağı", "Kaynak tipi:", srcTypes, 0, false, &ok);
            if (!ok) srcType = srcTypes[0];
            // Label: kısa ad
            if (srcType.contains("6 kW"))       node.label = "El.Ist. 6kW";
            else if (srcType.contains("12 kW")) node.label = "El.Ist. 12kW";
            else if (srcType.contains("Güneş")) node.label = "Güneş+Depo";
            else                                node.label = "Kombi";
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("Sicak su kaynagi (Sofben/Kazan) eklendi (x=%1, y=%2)")
                .arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        } else if (m_drainLabel == "__GAS_SOURCE__") {
            node.type  = mep::NodeType::GasSource;
            node.label = "Gaz Sayaci";
            m_drainLabel.clear();
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("Gaz sayaci eklendi (x=%1, y=%2)").arg(worldX,0,'f',2).arg(worldY,0,'f',2));
        } else if (m_drainLabel.startsWith("__BOILER__:")) {
            node.type  = mep::NodeType::Boiler;
            node.label = m_drainLabel.mid(11).toStdString();
            m_drainLabel.clear();
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("Kazan eklendi (x=%1, y=%2)").arg(worldX,0,'f',2).arg(worldY,0,'f',2));
        } else if (m_drainLabel == "__FIRE_PUMP__") {
            node.type  = mep::NodeType::FirePump;
            node.label = "Yangin Pompasi";
            m_drainLabel.clear();
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            UpdateUI();
            ScheduleAutoHydro();
            statusBar()->showMessage(QString("Yangin pompasi eklendi (x=%1, y=%2)").arg(worldX,0,'f',2).arg(worldY,0,'f',2));
        } else {
            node.type  = mep::NodeType::Junction;
            node.label = "J";
            auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
            m_document->ExecuteCommand(std::move(cmd));
            m_document->SetModified(true);
            UpdateUI();
            statusBar()->showMessage(QString("Bağlantı noktası eklendi (x=%1, y=%2)")
                .arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        }
        break;
    }
    case ToolMode::PlaceDrain: {
        mep::Node node;
        node.position = geom::Vec3(worldX, worldY, GetActiveFloorZ());
        QString drainStatusLabel = m_drainLabel;
        if (m_drainLabel == "__SPRINKLER__") {
            node.type  = mep::NodeType::Sprinkler;
            node.label = "Sprinkler";
            drainStatusLabel = "Sprinkler";
            m_drainLabel.clear();
        } else {
            node.type  = mep::NodeType::Drain;
            node.label = m_drainLabel.toStdString();
        }
        auto cmd = std::make_unique<core::AddNodeCommand>(network, node);
        m_document->ExecuteCommand(std::move(cmd));
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
        statusBar()->showMessage(QString("%1 eklendi (x=%2, y=%3)")
            .arg(drainStatusLabel).arg(worldX, 0, 'f', 2).arg(worldY, 0, 'f', 2));
        break;
    }
    case ToolMode::ConnectFixture: {
        // Once armatur secip secmedigimizi kontrol et (SNAP=50mm)
        constexpr double SNAP_FIX = 50.0;
        uint32_t bestFixId = 0;
        double bestFixD2 = SNAP_FIX * SNAP_FIX;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            if (node.type != mep::NodeType::Fixture) continue;
            double dx = node.position.x - worldX;
            double dy = node.position.y - worldY;
            double d2 = dx*dx + dy*dy;
            if (d2 < bestFixD2) { bestFixD2 = d2; bestFixId = nid; }
        }

        if (bestFixId != 0) {
            // Armatur tiklandi — zaten secili degilse listeye ekle
            auto it = std::find(m_batchFixtureIds.begin(), m_batchFixtureIds.end(), bestFixId);
            if (it == m_batchFixtureIds.end()) {
                m_batchFixtureIds.push_back(bestFixId);
            }
            const mep::Node* fn = network.GetNode(bestFixId);
            statusBar()->showMessage(QString("BAGLA: %1 armatur secildi (%2 dahil) — boru hattini tiklayin veya daha fazla armatur secin")
                .arg(m_batchFixtureIds.size())
                .arg(fn ? QString::fromStdString(fn->fixtureType) : "?"));
            if (m_commandBar) m_commandBar->SetPrompt(QString("Boru hatti (%1 armatur)").arg(m_batchFixtureIds.size()).toStdString());
            break;
        }

        // Armatur yok — boru hatti tiklamasi olmali; secili armatur varsa bagla
        if (m_batchFixtureIds.empty()) {
            statusBar()->showMessage("BAGLA: Once armatur(ler)i tiklayin, sonra boru hattini tiklayin");
            break;
        }

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
        const mep::Node* nA_src  = network.GetNode(srcEdge->nodeA);
        const mep::Node* nB_src  = network.GetNode(srcEdge->nodeB);
        if (!nA_src || !nB_src) { m_batchFixtureIds.clear(); break; }

        auto composite = std::make_unique<core::CompositeCommand>();
        int connectedCount = 0;

        for (uint32_t fixId : m_batchFixtureIds) {
            const mep::Node* fixtureNode = network.GetNode(fixId);
            if (!fixtureNode) continue;

            // Dik ayak noktasi boru uzerinde (fixtureNode'dan)
            double ex = nB_src->position.x - nA_src->position.x;
            double ey = nB_src->position.y - nA_src->position.y;
            double len2 = ex*ex + ey*ey;
            double t = 0.5;
            if (len2 > 1e-9) {
                t = ((fixtureNode->position.x - nA_src->position.x)*ex
                   + (fixtureNode->position.y - nA_src->position.y)*ey) / len2;
                t = std::max(0.05, std::min(0.95, t));
            }
            geom::Vec3 footPos{
                nA_src->position.x + t * ex,
                nA_src->position.y + t * ey,
                fixtureNode->position.z
            };

            mep::Node jNode;
            jNode.type     = mep::NodeType::Junction;
            jNode.position = footPos;
            jNode.label    = "J";
            auto addJ = std::make_unique<core::AddNodeCommand>(network, jNode);
            addJ->Execute();
            uint32_t jId = addJ->GetNodeId();
            composite->AddCommand(std::move(addJ));

            mep::Edge branchEdge;
            branchEdge.nodeA        = fixId;
            branchEdge.nodeB        = jId;
            branchEdge.type         = srcEdge->type;
            branchEdge.diameter_mm  = srcEdge->diameter_mm;
            branchEdge.roughness_mm = srcEdge->roughness_mm;
            branchEdge.material     = srcEdge->material;
            double dx = footPos.x - fixtureNode->position.x;
            double dy = footPos.y - fixtureNode->position.y;
            branchEdge.length_m = std::sqrt(dx*dx + dy*dy) / 1000.0;
            if (branchEdge.length_m < 0.001) branchEdge.length_m = 0.001;
            auto addBranch = std::make_unique<core::AddEdgeCommand>(network, branchEdge);
            addBranch->Execute();
            composite->AddCommand(std::move(addBranch));
            ++connectedCount;
        }

        m_document->TrackExecuted(std::move(composite));
        m_document->SetModified(true);
        m_batchFixtureIds.clear();
        m_connectFixtureNodeId = 0;
        UpdateUI();
        ScheduleAutoHydro();
        statusBar()->showMessage(QString("BAGLA: %1 armatur baglandi").arg(connectedCount));
        if (m_commandBar) m_commandBar->SetPrompt("Armatur sec");
        break;
    }
    case ToolMode::Select:
    default: {
        // ── Grip pick: var olan grip'e tıklandı mı? ─────────────────────
        if (!m_grips.empty() && m_vulkanWindow) {
            const int GRIP_PX = 8; // pick yarıçapı (piksel)
            const auto& vp0 = m_vulkanWindow->GetViewport();
            auto screenCur = vp0.WorldToScreen({worldX, worldY, 0});
            QPoint scr(static_cast<int>(screenCur.x), static_cast<int>(screenCur.y));
            for (int gi = 0; gi < static_cast<int>(m_grips.size()); ++gi) {
                QPoint delta = scr - m_grips[gi].screenPos;
                if (std::abs(delta.x()) <= GRIP_PX && std::abs(delta.y()) <= GRIP_PX) {
                    // Grip drag başlat
                    m_gripDragging  = true;
                    m_activeGrip    = gi;
                    m_hotGrip       = gi;
                    m_gripDragStart = {worldX, worldY, 0};
                    // Orijinal entity vertex'lerini sakla (undo için)
                    auto it = m_cadEntityCache.find(m_grips[gi].entityId);
                    if (it != m_cadEntityCache.end() && it->second) {
                        cad::Entity* e = it->second;
                        if (e->GetType() == cad::EntityType::Line) {
                            const auto* ln = static_cast<const cad::Line*>(e);
                            m_gripEntityOrig[0] = ln->GetStart();
                            m_gripEntityOrig[2] = ln->GetEnd();
                            m_gripEntityOrig[1]  = {(m_gripEntityOrig[0].x + m_gripEntityOrig[2].x) / 2,
                                                    (m_gripEntityOrig[0].y + m_gripEntityOrig[2].y) / 2, 0};
                        }
                    }
                    statusBar()->showMessage("Grip sürükleniyor — ESC ile iptal");
                    return;
                }
            }
        }

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
            // Drag taşıma başlat
            const mep::Node* n = network.GetNode(closestNodeId);
            if (n) {
                m_draggingNode = true;
                m_dragNodeId   = closestNodeId;
                m_dragStartPos = n->position;
            }
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
            // MEP hiç seçilmedi — CAD entity pick dene
            m_selectedNodeId = 0;
            m_selectedEdgeId = 0;
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
            if (m_fixturePanel) m_fixturePanel->Clear();

            // CAD entity en yakınını bul — ızgara varsa O(1), yoksa O(n)
            const double CAD_APERTURE_MM = 1000.0;
            cad::EntityId pickedId = 0;
            double pickedDist = CAD_APERTURE_MM;

            // Entity seçilebilir mi? Frozen=seçilemez, Locked=seçilemez
            auto isPickable = [&](const cad::Entity* e) -> bool {
                if (!e || !e->IsVisible()) return false;
                const auto& layers = m_document->GetLayers();
                auto lit = layers.find(e->GetLayerName());
                if (lit != layers.end()) {
                    if (!lit->second.IsDrawable()) return false; // frozen veya gizli
                    if (lit->second.IsLocked())    return false; // kilitli
                }
                return true;
            };

            auto pickFrom = [&](const std::vector<cad::Entity*>& candidates) {
                for (cad::Entity* e : candidates) {
                    if (!isPickable(e)) continue;
                    double d = DistToCADEntity(worldX, worldY, e);
                    if (d < pickedDist) { pickedDist = d; pickedId = e->GetId(); }
                }
            };

            if (m_entityGrid.IsBuilt()) {
                pickFrom(m_entityGrid.QueryNear(worldX, worldY, CAD_APERTURE_MM));
            } else {
                for (const auto& ent : m_document->GetCADEntities()) {
                    if (!ent) continue;
                    double d = DistToCADEntity(worldX, worldY, ent.get());
                    if (d < pickedDist) { pickedDist = d; pickedId = ent->GetId(); }
                }
            }

            if (pickedId != 0) {
                bool shiftHeld = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);

                if (shiftHeld) {
                    // Shift+click: çoklu seçime ekle/çıkar
                    if (m_selectedCADEntityIds.count(pickedId)) {
                        m_selectedCADEntityIds.erase(pickedId);
                    } else {
                        if (m_selectedCADEntityId != 0)
                            m_selectedCADEntityIds.insert(m_selectedCADEntityId);
                        m_selectedCADEntityIds.insert(pickedId);
                        m_selectedCADEntityId = pickedId;
                    }
                    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                        m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
                    statusBar()->showMessage(
                        QString("%1 entity seçili — Delete ile sil, ESC ile iptal")
                        .arg(m_selectedCADEntityIds.size()));
                    ComputeGrips();
                } else {
                    // Normal tık: önceki seçimi temizle, yeni seçim
                    m_selectedCADEntityIds.clear();
                    m_selectedCADEntityId = pickedId;
                    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                        m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(pickedId);
                    // Drag başlat + katman vurgula
                    auto it = m_cadEntityCache.find(pickedId);
                    if (it != m_cadEntityCache.end() && it->second) {
                        cad::Entity* ent = it->second;
                        auto bb = ent->GetBounds();
                        m_cadDragAnchor     = geom::Vec3(worldX, worldY, 0);
                        m_cadDragTotalDelta = geom::Vec3(0, 0, 0); // undo için sıfırla
                        m_cadDragEntityOrigin = bb.GetCenter();
                        m_draggingCADEntity = true;
                        const std::string& layName = ent->GetLayerName();
                        HighlightLayerInPanel(layName);
                        statusBar()->showMessage(
                            QString("Seçili: #%1  Katman: %2  — Sürükle / Grip / Delete")
                            .arg(pickedId).arg(QString::fromStdString(layName)));
                    }
                    ComputeGrips();
                }
            } else {
                // Hiç entity yok — seçim kutusunu başlat (AutoCAD davranışı)
                m_selectedCADEntityId = 0;
                m_selectedCADEntityIds.clear();
                m_draggingCADEntity = false;
                if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                    m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds({});
                HighlightLayerInPanel("");

                // Seçim kutusu state'i başlat
                m_selBoxActive = true;
                m_selBoxStartWorld  = geom::Vec3(worldX, worldY, 0);
                m_selBoxStartScreen = m_vulkanWindow
                    ? QPoint(static_cast<int>(m_vulkanWindow->GetViewport()
                        .WorldToScreen({worldX, worldY, 0}).x),
                             static_cast<int>(m_vulkanWindow->GetViewport()
                        .WorldToScreen({worldX, worldY, 0}).y))
                    : QPoint(0, 0);
                statusBar()->showMessage("Seçim kutusu — sağa: Pencere (tam içinde), sola: Kesişim (dokunan)");
            }
        }
        break;
    }
    case ToolMode::DrawPolyArea: {
        // Her tıklama → polygon noktası ekle
        m_polyAreaPoints.push_back(geom::Vec3(worldX, worldY, GetActiveFloorZ()));
        if (m_snapOverlay) m_snapOverlay->show();
        statusBar()->showMessage(QString("Yağmur alanı: %1 nokta eklendi — Enter ile kapat, ESC ile iptal")
            .arg(m_polyAreaPoints.size()));
        break;
    }
    } // switch
} // HandleMousePress

void MainWindow::HandleMouseRelease(double worldX, double worldY, Qt::MouseButton btn) {
    if (btn != Qt::LeftButton || !m_document) return;

    // ── Grip drag commit — undo stack'e kaydet ──────────────────────────
    if (m_gripDragging) {
        int savedGrip = m_activeGrip;
        m_gripDragging = false;
        m_activeGrip   = -1;
        m_hotGrip      = -1;

        if (savedGrip >= 0 && savedGrip < static_cast<int>(m_grips.size())) {
            auto& gp = m_grips[savedGrip];
            auto it = m_cadEntityCache.find(gp.entityId);
            if (it != m_cadEntityCache.end() && it->second &&
                it->second->GetType() == cad::EntityType::Line) {
                auto* ln = static_cast<cad::Line*>(it->second);
                geom::Vec3 finalPos = (gp.index == 0) ? ln->GetStart() : ln->GetEnd();
                geom::Vec3 origPos  = m_gripEntityOrig[gp.index];
                if (gp.index != 1) { // start/end — endpoint grip
                    // Entity halihazırda finalPos'ta; revert, sonra command ile uygula
                    if (gp.index == 0) ln->SetStart(origPos);
                    else               ln->SetEnd(origPos);
                    auto cmd = std::make_unique<core::GripEditLineCommand>(ln, gp.index, origPos, finalPos);
                    m_document->ExecuteCommand(std::move(cmd));
                } else { // midpoint — entity translate
                    geom::Vec3 delta = {finalPos.x - origPos.x, finalPos.y - origPos.y, 0};
                    ln->SetStart(m_gripEntityOrig[0]);
                    ln->SetEnd  (m_gripEntityOrig[2]);
                    auto cmd = std::make_unique<core::MoveCADEntityCommand>(ln, delta);
                    m_document->ExecuteCommand(std::move(cmd));
                }
            }
        }
        m_document->SetModified(true);
        InvalidateRenderer();
        ComputeGrips();
        statusBar()->showMessage("Grip düzenleme tamamlandı (Ctrl+Z ile geri alınabilir)", 2000);
        return;
    }

    // CAD entity drag bırakma — undo stack'e kaydet
    if (m_draggingCADEntity) {
        m_draggingCADEntity = false;
        geom::Vec3 delta = m_cadDragTotalDelta;
        if (std::abs(delta.x) > 0.1 || std::abs(delta.y) > 0.1) {
            // Entity halihazırda taşınmış; revert + command ile undo stack'e ekle
            auto it = m_cadEntityCache.find(m_selectedCADEntityId);
            if (it != m_cadEntityCache.end() && it->second) {
                cad::Entity* e = it->second;
                e->Move(geom::Vec3(-delta.x, -delta.y, 0)); // orijinal pozisyona dön
                auto cmd = std::make_unique<core::MoveCADEntityCommand>(e, delta);
                m_document->ExecuteCommand(std::move(cmd)); // re-apply + undo stack
            }
        }
        m_document->SetModified(true);
        InvalidateRenderer();
        statusBar()->showMessage(QString("CAD entity #%1 taşındı (Ctrl+Z ile geri alınabilir)")
                                     .arg(m_selectedCADEntityId), 2000);
        return;
    }

    // AutoCAD seçim kutusu tamamlama
    if (m_selBoxActive && m_currentToolMode == ToolMode::Select) {
        m_selBoxActive = false;
        if (m_snapOverlay) m_snapOverlay->ClearSelectionBox();

        // Dünya koordinatlarında seçim dikdörtgeni
        double x0 = m_selBoxStartWorld.x, y0 = m_selBoxStartWorld.y;
        double x1 = worldX,               y1 = worldY;
        double rxMin = std::min(x0, x1), rxMax = std::max(x0, x1);
        double ryMin = std::min(y0, y1), ryMax = std::max(y0, y1);

        // Minimum boyut — tek tıklama değil gerçek sürükleme (5 world unit = ~5mm)
        const double MIN_SEL_SIZE = 5.0;
        if ((rxMax - rxMin) < MIN_SEL_SIZE || (ryMax - ryMin) < MIN_SEL_SIZE) return;

        bool crossing = (worldX < m_selBoxStartWorld.x); // sola sürükleme = crossing

        // Lock/freeze kontrolü için layer map
        const auto& layersRef = m_document->GetLayers();
        auto isBoxPickable = [&](const cad::Entity* e) -> bool {
            if (!e || !e->IsVisible()) return false;
            auto lit = layersRef.find(e->GetLayerName());
            if (lit != layersRef.end()) {
                if (!lit->second.IsDrawable()) return false;
                if (lit->second.IsLocked())    return false;
            }
            return true;
        };

        m_selectedCADEntityIds.clear();
        for (const auto& ent : m_document->GetCADEntities()) {
            if (!isBoxPickable(ent.get())) continue;
            auto bb = ent->GetBounds();
            if (crossing) {
                bool noOverlap = (bb.max.x < rxMin || bb.min.x > rxMax ||
                                  bb.max.y < ryMin || bb.min.y > ryMax);
                if (!noOverlap) m_selectedCADEntityIds.insert(ent->GetId());
            } else {
                if (bb.min.x >= rxMin && bb.max.x <= rxMax &&
                    bb.min.y >= ryMin && bb.max.y <= ryMax)
                    m_selectedCADEntityIds.insert(ent->GetId());
            }
        }

        if (!m_selectedCADEntityIds.empty()) {
            m_selectedCADEntityId = 0; // tek tıklama seçimini temizle
            if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
                m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
            statusBar()->showMessage(
                QString("%1 entity seçildi (%2) — Delete ile sil, ESC ile iptal")
                .arg(m_selectedCADEntityIds.size())
                .arg(crossing ? "Kesişim" : "Pencere"), 4000);
        } else {
            statusBar()->showMessage("Seçim kutusunda entity bulunamadı", 2000);
        }
        return;
    }

    if (!m_draggingNode) return;
    m_draggingNode = false;

    auto& network = m_document->GetNetwork();
    mep::Node* node = network.GetNode(m_dragNodeId);
    if (!node) return;

    geom::Vec3 finalPos = node->position;
    // Anlamlı hareket yoksa (< 1mm) geri al
    double dx = finalPos.x - m_dragStartPos.x;
    double dy = finalPos.y - m_dragStartPos.y;
    if (dx*dx + dy*dy < 1.0) {
        node->position = m_dragStartPos;
        return;
    }

    // Taşımayı undo stack'e kaydet: önce pozisyonu geri al, sonra command ile uygula
    node->position = m_dragStartPos;
    auto cmd = std::make_unique<core::MoveNodeCommand>(network, m_dragNodeId, finalPos);
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    UpdateUI();
    ScheduleAutoHydro();
    statusBar()->showMessage(QString("Node #%1 tasindi").arg(m_dragNodeId), 2000);
}

void MainWindow::HandleMouseMove(double worldX, double worldY) {
    // ── Ortho modu (F8): yatay/dikey kısıt ─────────────────────────────
    if (m_orthoMode && m_drawState == DrawState::WaitingSecondPoint && m_drawFirstPoint.has_value()) {
        double dx = worldX - m_drawFirstPoint->x;
        double dy = worldY - m_drawFirstPoint->y;
        if (std::abs(dx) >= std::abs(dy)) worldY = m_drawFirstPoint->y; // yatay
        else                               worldX = m_drawFirstPoint->x; // dikey
    }

    // ── Grip drag ────────────────────────────────────────────────────────
    if (m_gripDragging && m_activeGrip >= 0 && m_activeGrip < static_cast<int>(m_grips.size())) {
        auto& gp = m_grips[m_activeGrip];
        auto it = m_cadEntityCache.find(gp.entityId);
        if (it != m_cadEntityCache.end() && it->second) {
            cad::Entity* e = it->second;
            geom::Vec3 snapped{worldX, worldY, gp.worldPos.z};
            if (e->GetType() == cad::EntityType::Line) {
                auto* ln = static_cast<cad::Line*>(e);
                if (gp.index == 0)       ln->SetStart(snapped);
                else if (gp.index == 2)  ln->SetEnd(snapped);
                else {
                    double dx = worldX - gp.worldPos.x, dy = worldY - gp.worldPos.y;
                    ln->SetStart({m_gripEntityOrig[0].x+dx, m_gripEntityOrig[0].y+dy, m_gripEntityOrig[0].z});
                    ln->SetEnd  ({m_gripEntityOrig[2].x+dx, m_gripEntityOrig[2].y+dy, m_gripEntityOrig[2].z});
                }
            } else if (e->GetType() == cad::EntityType::Polyline) {
                auto* pl = static_cast<cad::Polyline*>(e);
                if (gp.index >= 0 && gp.index < static_cast<int>(pl->GetVertices().size()))
                    pl->SetVertexPosition(static_cast<size_t>(gp.index), snapped);
            }
            gp.worldPos = snapped;
        }
        UpdateGripScreen();
        if (m_snapOverlay) {
            std::vector<QPoint> pts;
            for (const auto& g : m_grips) pts.push_back(g.screenPos);
            m_snapOverlay->SetGrips(pts, m_activeGrip);
        }
        InvalidateRenderer();
        return;
    }

    // ── Grip hover: imleç bir grip'in üzerinde mi? ───────────────────────
    if (!m_grips.empty() && m_vulkanWindow && !m_gripDragging) {
        const int GRIP_PX = 8;
        const auto& vp0 = m_vulkanWindow->GetViewport();
        auto sc = vp0.WorldToScreen({worldX, worldY, 0});
        QPoint scr(static_cast<int>(sc.x), static_cast<int>(sc.y));
        int newHot = -1;
        for (int gi = 0; gi < static_cast<int>(m_grips.size()); ++gi) {
            QPoint d = scr - m_grips[gi].screenPos;
            if (std::abs(d.x()) <= GRIP_PX && std::abs(d.y()) <= GRIP_PX) { newHot = gi; break; }
        }
        if (newHot != m_hotGrip) {
            m_hotGrip = newHot;
            std::vector<QPoint> pts;
            for (const auto& g : m_grips) pts.push_back(g.screenPos);
            if (m_snapOverlay) m_snapOverlay->SetGrips(pts, m_hotGrip);
        }
    }

    // CAD entity drag
    if (m_draggingCADEntity && m_document && m_selectedCADEntityId != 0) {
        double ddx = worldX - m_cadDragAnchor.x;
        double ddy = worldY - m_cadDragAnchor.y;
        auto it = m_cadEntityCache.find(m_selectedCADEntityId);
        if (it != m_cadEntityCache.end() && it->second)
            it->second->Move(geom::Vec3(ddx, ddy, 0));
        m_cadDragAnchor     = geom::Vec3(worldX, worldY, 0);
        m_cadDragTotalDelta.x += ddx; // undo için kümülatif topla
        m_cadDragTotalDelta.y += ddy;
        InvalidateRenderer();
        return;
    }

    // MEP node drag
    if (m_draggingNode && m_document && m_currentToolMode == ToolMode::Select) {
        auto& network = m_document->GetNetwork();
        mep::Node* node = network.GetNode(m_dragNodeId);
        if (node) {
            node->position.x = worldX;
            node->position.y = worldY;
            // Z değişmez (kat sabit)
            UpdateUI();
            RefreshTextOverlay();
        }
        return;
    }

    statusBar()->showMessage(QString("x=%1  y=%2")
        .arg(worldX, 0, 'f', 3).arg(worldY, 0, 'f', 3));

    if (!m_snapOverlay) return;

    // Overlay boyutunu container ile senkronize tut
    if (m_vulkanContainer &&
        m_snapOverlay->size() != m_vulkanContainer->size()) {
        m_snapOverlay->resize(m_vulkanContainer->size());
    }

    // Select modu: seçim kutusu aktifse overlay'i güncelle, değilse gizle
    if (m_currentToolMode == ToolMode::Select) {
        if (m_selBoxActive && m_vulkanWindow) {
            const auto& vp2 = m_vulkanWindow->GetViewport();
            geom::Vec3 curScr = vp2.WorldToScreen({worldX, worldY, 0.0});
            QPoint curPt(static_cast<int>(curScr.x), static_cast<int>(curScr.y));
            // Sağa sürükleme = Window (mavi), sola = Crossing (yeşil)
            bool crossing = (curPt.x() < m_selBoxStartScreen.x());
            m_snapOverlay->show();
            m_snapOverlay->SetSelectionBox(m_selBoxStartScreen, curPt, crossing, true);
        } else {
            m_snapOverlay->ClearSelectionBox();
            m_snapOverlay->Hide();
        }
        return;
    }

    // World → screen koordinat dönüşümü
    const auto& vp = m_vulkanWindow->GetViewport();
    geom::Vec3 screenPt = vp.WorldToScreen({worldX, worldY, 0.0});
    QPoint screenPos(static_cast<int>(screenPt.x), static_cast<int>(screenPt.y));

    // Snap hesabı — cached flat pointer list (no per-frame allocation)
    cad::SnapResult snap;
    if (m_document && !m_snapEntityCache.empty())
        snap = m_snapManager.FindSnapPoint({worldX, worldY, 0.0},
                                           m_snapEntityCache,
                                           vp.GetZoom());

    m_snapOverlay->Update(screenPos, snap);

    // Snap tipi belirlendiyse status bar koordinatını zenginleştir
    if (snap.IsValid()) {
        static const char* snapNames[] = {
            "", "Endpoint", "Midpoint", "Center",
            "Intersection", "Perpendicular", "Nearest", "Grid"
        };
        int idx = static_cast<int>(snap.type);
        const char* name = (idx > 0 && idx <= 7) ? snapNames[idx] : "";
        statusBar()->showMessage(
            QString("x=%1  y=%2  [%3]")
            .arg(snap.point.x, 0, 'f', 3)
            .arg(snap.point.y, 0, 'f', 3)
            .arg(name));
    }

    // Rubber-band: ikinci nokta beklenen modlarda çizgi göster
    if (m_drawState == DrawState::WaitingSecondPoint &&
        (m_currentToolMode == ToolMode::DrawPipe ||
         m_currentToolMode == ToolMode::PlaceFixture)) {
        m_snapOverlay->SetRubberBand(m_firstClickScreen, screenPos, true);
    } else if (m_currentToolMode == ToolMode::DrawPolyArea &&
               !m_polyAreaPoints.empty()) {
        // Poligon çizimi: son noktadan imlece çizgi
        const auto& vp2 = m_vulkanWindow->GetViewport();
        geom::Vec3 lastSp = vp2.WorldToScreen(m_polyAreaPoints.back());
        QPoint lastScreen(static_cast<int>(lastSp.x), static_cast<int>(lastSp.y));
        m_snapOverlay->SetRubberBand(lastScreen, screenPos, true);
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
            m_logList->addItem("Soguk Su: LINE/PIPE  FIXTURE  JUNCTION  SOURCE  DRAIN");
            m_logList->addItem("Sicak Su: SICAK-SU  SOFBEN (sicak su kaynagi)");
            m_logList->addItem("Gaz     : GAZ  GAZ-SAYAC  GAZ-CIHAZ/KOMBI/OCAK");
            m_logList->addItem("Isitma  : ISITMA  DONUS  KAZAN/BOILER  RADYATOR");
            m_logList->addItem("Yangin  : YANGIN  SPRINKLER  YANGIN-POMPA  EN12845");
            m_logList->addItem("Baglama : BAGLA/CONNECT  (armaturu boru hattina bagla)");
            m_logList->addItem("Pis Su  : PIS-SU  YER-SUZGECI  ROGAR  AKILLI(-BAGLANTI)  BOSALTMA");
            m_logList->addItem("Kontrol : KABUL/ACCEPT  (tesisati dogrula+numaralandir)");
            m_logList->addItem("Gorunum : ZOOM-EXTENTS  VIEW-PLAN  VIEW-ISO  KATMAN(-VIS)");
            m_logList->addItem("Analiz  : HYDRAULICS  HIDROFOR  NORM  YAGMUR  BOM  RISER");
            m_logList->addItem("Hesap   : DN-OVERRIDE  KESIF  GUNCELLE  FOSEPTIK  PIS-HESAP  EMDIRME  PIS-CUKUR  PIS-POMPA  GENLESIM  YAGMUR-ALAN  NORM-KARSILASTIR  HESAP-KARARI  BIRLESIK-MOD");
            m_logList->addItem("Duzen   : TUMU(Ctrl+A)  MIRROR/AYNA  ORTHO(F8)  UNDO  REDO");
            m_logList->addItem("Diger   : SAVE  EXPORT-DXF  KAT-DXF  UZAKLIK  MIMARI  HIZALAMA  KOLON  PAFTA  ABOUT");
        }
    } else if (c == "BAGLA" || c == "CONNECT") {
        OnConnectFixture();
    } else if (c == "HIDROFOR" || c == "PUMP-SIZE") {
        OnHidrofor();
    } else if (c == "NORM" || c == "NORM-SEC") {
        OnNormSelection();
    } else if (c == "SARFIYAT" || c == "MUSLUK-BIRIM" || c == "TS825") {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::SARFIYAT;
        statusBar()->showMessage("Hesap normu: TS 825 Sarfiyat / Musluk Birimi");
        if (m_logList) m_logList->addItem("Norm: TS 825 Sarfiyat aktif");
        ScheduleAutoHydro();
    } else if (c == "DEVRE" || c == "DEVRE-SEC") {
        OnDevreSecenekleri();
    } else if (c == "BASKI" || c == "BASKI-ICERIGI") {
        OnBaskiIcerigi();
    } else if (c == "BASINC" || c == "PARCALAR" || c == "BASINC-KAYBI") {
        OnBaskiKaybi();
    } else if (c == "WORD" || c == "HTML-RAPOR" || c == "RAPOR-WORD") {
        OnWordRapor();
    } else if (c == "YAGMUR" || c == "RAINWATER") {
        OnYagmurSuyu();
    } else if (c == "BOM" || c == "KESIF") {
        OnBOM();
    } else if (c == "RISER" || c == "KOLON-SEMA") {
        OnRiserDiagram();
    } else if (c == "DN-OVERRIDE" || c == "DN-DEGISTIR") {
        OnDNOverride();
    } else if (c == "GUNCELLE" || c == "CIZIMI-GUNCELLE" || c == "CIZIM-GUNCELLE") {
        OnCizimiGuncelle();
    } else if (c == "FOSEPTIK" || c == "KAPALI-CUKUR" || c == "SEPTIK") {
        OnFoseptik();
    } else if (c == "PIS-HESAP" || c == "PIS-SU-HESAP") {
        OnPisSuHesapFoyu();
    } else if (c == "EMDIRME" || c == "EMDIRME-CUKURU") {
        OnEmdirmeCukuru();
    } else if (c == "PIS-CUKUR" || c == "PIS-SU-CUKUR") {
        OnPisSuCukuru();
    } else if (c == "PIS-POMPA" || c == "PIS-SU-POMPA" || c == "FOSSEPTIK-POMPA") {
        OnPisSuPompasi();
    } else if (c == "GENLESIM" || c == "GENLESIM-TANKI" || c == "EXPANSION-TANK") {
        OnGenlesimTanki();
    } else if (c == "YAGMUR-ALAN" || c == "ALAN-CIZIM" || c == "POLY-ALAN") {
        OnYagmurAlani();
    } else if (c == "NORM-KARSILASTIR" || c == "KARSILASTIR" || c == "EN-DIN") {
        OnNormKarsilastirma();
    } else if (c == "HESAP-KARARI" || c == "NEDEN-CAP" || c == "CAP-KARAR") {
        OnHesapKarari();
    } else if (c == "BIRLESIK-MOD" || c == "BIRLESIK" || c == "AUTO-BAGLA") {
        OnBirleskMod();
    } else if (c == "HIZALAMA" || c == "FLOOR-ALIGN" || c == "3D-KONTROL") {
        OnFloorAlignment();
    } else if (c == "KOLON" || c == "COLUMN" || c == "DIKEY-BORU") {
        OnDrawColumn();
    } else if (c == "PAFTA" || c == "PRINT" || c == "YAZDIR") {
        OnPrintLayout();
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
    } else if (c == "SICAK-SU" || c == "HOT-WATER" || c == "HOT-PIPE") {
        OnDrawHotWaterPipe();
    } else if (c == "SOFBEN" || c == "HOT-SOURCE" || c == "SICAK-KAYNAK") {
        OnPlaceHotSource();
    // Doğal Gaz
    } else if (c == "GAZ" || c == "GAZ-BORU" || c == "GAS-PIPE" || c == "DOGALGAZ") {
        OnDrawGasPipe();
    } else if (c == "GAZ-SAYAC" || c == "GAS-SOURCE" || c == "GAZ-KAYNAK") {
        OnPlaceGasSource();
    } else if (c == "GAZ-CIHAZ" || c == "GAS-APPLIANCE" || c == "KOMBI" || c == "OCAK") {
        OnPlaceGasAppliance();
    // Isıtma
    } else if (c == "ISITMA" || c == "HEAT-PIPE" || c == "ISITMA-GIDIS") {
        OnDrawHeatingPipe();
    } else if (c == "DONUS" || c == "HEAT-RETURN" || c == "ISITMA-DONUS") {
        OnDrawHeatingReturn();
    } else if (c == "KAZAN" || c == "BOILER" || c == "ISITMA-KAYNAK") {
        OnPlaceBoiler();
    } else if (c == "RADYATOR" || c == "RADIATOR") {
        OnPlaceRadiator();
    // Yangın
    } else if (c == "YANGIN" || c == "FIRE-LINE" || c == "YANGIN-HATTI") {
        OnDrawFireLine();
    } else if (c == "SPRINKLER") {
        OnPlaceSprinkler();
    } else if (c == "YANGIN-POMPA" || c == "FIRE-PUMP") {
        OnPlaceFirePump();
    } else if (c == "YANGIN-SINIF" || c == "HAZARD-CLASS" || c == "EN12845") {
        OnFireHazardClass();
    // Otomatik ölçülendirme
    } else if (c == "AUTO-OLCULENDIR" || c == "OLCULENDIR" || c == "DIM-AUTO" || c == "BOYUTLA") {
        OnAutoOlculendir();
    // Basınç bölgesi
    } else if (c == "BASINC-BOLGESI" || c == "BOLGE" || c == "PRESSURE-ZONE") {
        OnBaskiBolgesi();
    // Üretici katalog
    } else if (c == "KATALOG" || c == "URETICI" || c == "MANUFACTURER") {
        OnUreticiKatalog();
    } else if (c == "KABUL" || c == "ACCEPT" || c == "TESISAT-KABUL") {
        OnTesistatKabul();
    } else if (c == "PIS-SU" || c == "DRAINAGE-PIPE" || c == "DRAIN-PIPE") {
        OnDrawDrainPipe();
    } else if (c == "YER-SUZGECI" || c == "FLOOR-DRAIN") {
        OnPlaceYerSuzgeci();
    } else if (c == "ROGAR") {
        OnPlaceRogar();
    } else if (c == "AKILLI-BAGLANTI" || c == "SMART-POINT" || c == "AKILLI") {
        OnPlaceSmartPoint();
    } else if (c == "BOSALTMA" || c == "ANA-TAHLIYE") {
        OnBosaltmaNoktasi();
    } else if (c == "KATMAN" || c == "LAYER-VIS" || c == "KATMAN-VIS") {
        OnLayerVisibility();
    } else if (c == "LAYISO" || c == "KATMAN-IZOLE" || c == "IZOLE") {
        OnLayIso();
    } else if (c == "LAYUNISO" || c == "KATMAN-TUM" || c == "IZOLE-KALDIR") {
        OnLayUnIso();
    } else if (c == "PURGE" || c == "TEMIZLE" || c == "KULLANILMAYAN") {
        OnPurge();
    } else if (c == "LAYLCK" || c == "KILITLE-TUMU") {
        OnLayLockAll();
    } else if (c == "LAYULK" || c == "KIL-KALDIR-TUMU") {
        OnLayUnlockAll();
    } else if (c == "TASIYE" || c == "KATMANA-TASI" || c == "MOVE-TO-LAYER") {
        OnMoveToLayer();
    } else if (c == "LAYERSTATE" || c == "KATMAN-KAYDET" || c == "LS-SAVE") {
        OnLayerStateSave();
    } else if (c == "LAYERSTATE-YUKLE" || c == "KATMAN-YUKLE" || c == "LS-LOAD") {
        OnLayerStateRestore();
    } else if (c == "LAYERSTATE-LISTE" || c == "LS-LIST") {
        OnLayerStateList();
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
    } else if (c == "SABLON" || c == "TEMPLATE" || c == "YENI-SABLON") {
        OnNewFromTemplate();
    } else if (c == "OPEN") {
        OnOpen();
    } else if (c == "EXPORT-DXF" || c == "CIKTI-DXF") {
        OnExportDXF();
    } else if (c == "KAT-DXF" || c == "FLOOR-DXF" || c == "EKRAN-CIZIMI") {
        OnExportFloorDXF();
    } else if (c == "EXPORT-PDF") {
        OnExportReport();
    } else if (c == "TUMU" || c == "SELECTALL" || c == "TUMUNU-SEC") {
        OnSelectAll();
    } else if (c == "MIRROR" || c == "AYNA" || c == "YANSI") {
        OnMirror();
    } else if (c == "OFFSET" || c == "PARALEL" || c == "OFSET") {
        OnOffset();
    } else if (c == "COPY" || c == "KOPYALA" || c == "CP") {
        OnCopy();
    } else if (c == "PASTE" || c == "YAPISTIR") {
        OnPaste();
    } else if (c == "TRIM" || c == "KISALT" || c == "TR") {
        OnTrim();
    } else if (c == "EXTEND" || c == "UZAT" || c == "EX") {
        OnExtend();
    } else if (c == "ORTHO" || c == "F8") {
        m_orthoMode = !m_orthoMode;
        statusBar()->showMessage(m_orthoMode ? "Ortho AÇIK" : "Ortho KAPALI", 2000);
    } else if (c == "HAKKINDA" || c == "ABOUT" || c == "VERSION" || c == "VERSIYON") {
        OnAbout();
    } else {
        statusBar()->showMessage(QString("Bilinmeyen komut: %1  (HELP yazın)").arg(cmd));
        if (m_commandBar) m_commandBar->SetPrompt("Komut");
    }
}

void MainWindow::RefreshTextOverlay() {
    if (!m_snapOverlay || !m_document || !m_vulkanWindow) return;

    m_vulkanWindow->InvalidateNetworkData(); // MEP ağı değişmiş olabilir — vertex buffer yenile

    const cad::Viewport& vp = m_vulkanWindow->GetViewport();
    const auto& layerMap    = m_document->GetLayers();

    std::vector<SnapOverlay::TextLabel> labels;
    labels.reserve(128);

    // ── CAD Text / MText entity'leri (DXF/DWG import) ────────
    for (const auto& ent : m_document->GetCADEntities()) {
        if (!ent) continue;
        if (ent->GetType() != cad::EntityType::Text &&
            ent->GetType() != cad::EntityType::MText) continue;

        // Layer görünürlük kontrolü
        {
            auto layIt = layerMap.find(ent->GetLayerName());
            if (layIt != layerMap.end() && !layIt->second.IsDrawable()) continue;
        }

        const auto* txt = static_cast<const cad::Text*>(ent.get());
        const std::string& content = txt->GetText();
        if (content.empty()) continue;

        geom::Vec3 sp = vp.WorldToScreen(txt->GetEffectiveInsertPoint());
        if (sp.x < -200 || sp.x > vp.GetWidth() + 200 ||
            sp.y < -200 || sp.y > vp.GetHeight() + 200) continue;

        double heightPx = txt->GetHeight() * vp.GetZoom();
        // Minimum 7px'in altında render etme — ama çok küçük kalınca min boyut uygula
        if (heightPx < 1.5) continue;
        if (heightPx < 7.0) heightPx = 7.0;  // minimum okunabilir boy

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
        // Katman görünürlük filtresi (overlay labels)
        if (edge.type == mep::EdgeType::Supply   && !m_showTemizSu) continue;
        if (edge.type == mep::EdgeType::HotWater && !m_showSicakSu) continue;
        if (edge.type == mep::EdgeType::Drainage && !m_showPisSu)   continue;

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

        const bool isCol = network.IsColumnEdge(edge.id);
        QColor edgeColor;
        if (isCol) {
            switch (edge.type) {
                case mep::EdgeType::Supply:   edgeColor = QColor(0, 220, 255);   break; // cyan
                case mep::EdgeType::HotWater: edgeColor = QColor(255, 100, 60);  break; // turuncu-kırmızı
                case mep::EdgeType::Drainage: edgeColor = QColor(255, 140, 30);  break; // turuncu
                default:                      edgeColor = QColor(180, 180, 180); break;
            }
        } else {
            switch (edge.type) {
                case mep::EdgeType::Supply:        edgeColor = QColor(100, 180, 255); break; // mavi
                case mep::EdgeType::HotWater:      edgeColor = QColor(255,  80,  60); break; // kırmızı
                case mep::EdgeType::Drainage:      edgeColor = QColor(200, 140,  70); break; // kahverengi
                case mep::EdgeType::Gas:           edgeColor = QColor(255, 215,   0); break; // sarı
                case mep::EdgeType::Heating:       edgeColor = QColor(255,  80,  15); break; // kırmızı-turuncu
                case mep::EdgeType::HeatingReturn: edgeColor = QColor( 60, 100, 255); break; // mavi
                case mep::EdgeType::FireLine:      edgeColor = QColor(200,   0,   0); break; // koyu kırmızı
                default:                           edgeColor = QColor(160, 160, 160); break;
            }
        }

        // Baskı İçeriği filtresi — kullanıcı hangi bileşenleri seçtiyse göster
        QStringList parts;
        if (m_labelShowDN && !edge.label.empty())
            parts << QString::fromStdString(edge.label);
        if (m_labelShowFlow && edge.flowRate_m3s > 0.0)
            parts << QString("Q=%1L/s").arg(edge.flowRate_m3s * 1000.0, 0, 'f', 2);
        if (m_labelShowLength && edge.length_m > 0.0)
            parts << QString("L=%1m").arg(edge.length_m, 0, 'f', 1);
        if (m_labelShowVelocity && edge.velocity_ms > 0.0)
            parts << QString("v=%1").arg(edge.velocity_ms, 0, 'f', 2);
        if (m_labelShowHeadLoss && edge.headLoss_m > 0.0)
            parts << QString("dH=%1").arg(edge.headLoss_m, 0, 'f', 3);
        if (m_labelShowSlope && edge.type == mep::EdgeType::Drainage)
            parts << QString("i=%1%").arg(edge.slope * 100.0, 0, 'f', 1);
        if (m_labelShowFillRate && edge.type == mep::EdgeType::Drainage && edge.fillRate > 0.0)
            parts << QString("h/d=%1%").arg(edge.fillRate * 100.0, 0, 'f', 0);

        if (parts.isEmpty()) continue;

        QString labelText = parts.join(" ");
        if (isCol) labelText = "[K] " + labelText;

        SnapOverlay::TextLabel lbl;
        lbl.pos    = QPointF(sp.x + 4, sp.y - 4);
        lbl.text   = labelText;
        lbl.pixelH = 11;
        lbl.color  = edgeColor;
        lbl.rotDeg = 0.0;
        lbl.hAlign = 0;
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

            // SmartPoint: magenta label "⊕ Baglanti"
            const bool isSmartPoint = (node.type == mep::NodeType::Fixture &&
                                       node.fixtureType == "SmartPoint");
            // Ana Tahliye işareti
            const bool isMainDrain = (nid == m_mainDrainNodeId && m_mainDrainNodeId != 0);

            QColor qcol;
            if (isSmartPoint) {
                qcol = QColor(230, 80, 220);   // magenta
            } else {
                switch (node.type) {
                    case mep::NodeType::Fixture:   qcol = QColor(100, 255, 120); break;
                    case mep::NodeType::Source:    qcol = QColor(100, 160, 255); break;
                    case mep::NodeType::HotSource: qcol = QColor(255, 100,  60); break;
                    case mep::NodeType::Drain:     qcol = QColor(210, 140,  80); break;
                    case mep::NodeType::Pump:      qcol = QColor(255, 220,  50); break;
                    default:                       qcol = Qt::white;              break;
                }
            }

            constexpr int kLblH = 10;
            constexpr int kLblW = 60;
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

            QString labelText;
            if (isSmartPoint)
                labelText = "+ Baglanti";
            else if (isMainDrain)
                labelText = "[ANA TAHLIYE] " + QString::fromStdString(node.label);
            else
                labelText = QString::fromStdString(node.label);

            SnapOverlay::TextLabel lbl;
            lbl.pos    = QPointF(r.left(), r.bottom() - 2);
            lbl.text   = labelText;
            lbl.pixelH = isMainDrain ? 12 : kLblH;
            lbl.color  = isMainDrain ? QColor(255, 120, 0) : qcol;
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

        // ── Reactive validation: ağ durumu status bar'a yaz ──────────
        if (!network.GetNodeMap().empty()) {
            int openCount  = static_cast<int>(openEndPts.size());
            bool hasSource = false;
            for (const auto& [nid, node] : network.GetNodeMap()) {
                if (node.type == mep::NodeType::Source ||
                    node.type == mep::NodeType::HotSource) { hasSource = true; break; }
            }
            QString valMsg;
            if (!hasSource)
                valMsg = "⚠ Kaynak (SOURCE) eksik";
            else if (openCount > 0)
                valMsg = QString("⚠ %1 açık uç — bağlantı eksik").arg(openCount);
            else
                valMsg = QString("✓ %1 node · %2 boru — ağ tam bağlı")
                             .arg(network.GetNodeMap().size())
                             .arg(network.GetEdgeMap().size());
            // Yalnızca seçim modu ve çizim modu dışındayken göster
            if (m_currentToolMode == ToolMode::Select)
                statusBar()->showMessage(valMsg);
        }
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
    m_batchFixtureIds.clear();
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
    m_batchFixtureIds.clear();
    statusBar()->showMessage("BAGLA: Armaturleri tiklayin (birden fazla secebilirsiniz), sonra boru hattini tiklayin");
    if (m_commandBar) m_commandBar->SetPrompt("Armatur sec");
}

// ============================================================
//  KOLON BAĞLANTI ASİSTANI — dikey boru (Z ekseni)
// ============================================================
void MainWindow::OnDrawColumn() {
    if (!m_document) {
        QMessageBox::warning(this, "Kolon Boru", "Aktif belge yok.");
        return;
    }
    const auto& floors = m_floorManager.GetFloors();
    if (floors.size() < 2) {
        QMessageBox::information(this, "Kolon Boru Ciz",
            "Kolon cizmek icin en az 2 kat tanimlanmis olmali.\n"
            "Once Mimari → Mimari Belirle (Ctrl+M) ile katlari ekleyin.");
        return;
    }

    auto& network = m_document->GetNetwork();
    const auto& nodeMap = network.GetNodeMap();
    if (nodeMap.empty()) {
        QMessageBox::information(this, "Kolon Boru Ciz",
            "Once en az bir node (boru bitti, armatur veya kavsis) cizin.");
        return;
    }

    // ── Kaynak Node Seçimi ──────────────────────────────────
    // Kullanıcıya mevcut node'ları listele (ID + pozisyon + kat)
    QStringList nodeItems;
    std::vector<uint32_t> nodeIds;
    for (const auto& [id, nd] : nodeMap) {
        int fi = m_floorManager.GetFloorIndexAtElevation(nd.position.z);
        QString katLabel = (fi == -999) ? QString("z=%1m").arg(nd.position.z, 0, 'f', 2)
                                        : QString("Kat %1").arg(fi);
        QString typeStr;
        switch (nd.type) {
            case mep::NodeType::Junction: typeStr = "Kavsis"; break;
            case mep::NodeType::Fixture:  typeStr = "Armatur"; break;
            case mep::NodeType::Source:   typeStr = "Kaynak"; break;
            case mep::NodeType::Drain:    typeStr = "Gider"; break;
            default:                      typeStr = "Node"; break;
        }
        nodeItems << QString("[%1] %2 @ %3 (X=%4, Y=%5)")
            .arg(id).arg(typeStr).arg(katLabel)
            .arg(nd.position.x, 0, 'f', 0)
            .arg(nd.position.y, 0, 'f', 0);
        nodeIds.push_back(id);
    }

    bool ok = false;
    QString chosen = QInputDialog::getItem(this,
        "Kolon Boru Ciz — Kaynak Node",
        "Kolon baslasin/bitsin diye hangi mevcut node'u kullanmak istiyorsunuz?",
        nodeItems, 0, false, &ok);
    if (!ok) return;

    int selIdx = nodeItems.indexOf(chosen);
    if (selIdx < 0 || selIdx >= (int)nodeIds.size()) return;
    uint32_t srcNodeId = nodeIds[selIdx];
    const mep::Node* srcNode = network.GetNode(srcNodeId);
    if (!srcNode) return;

    // ── Hedef Kat Seçimi ───────────────────────────────────
    QStringList floorItems;
    int srcFloor = m_floorManager.GetFloorIndexAtElevation(srcNode->position.z);
    for (const auto& fl : floors) {
        if (fl.index == srcFloor) continue; // aynı kat — kolon olmaz
        floorItems << QString("Kat %1: %2 (z=%3 m)")
            .arg(fl.index)
            .arg(QString::fromStdString(fl.label))
            .arg(fl.elevation_m, 0, 'f', 2);
    }
    if (floorItems.isEmpty()) {
        QMessageBox::information(this, "Kolon Boru Ciz",
            "Tum katlar secili node ile ayni kat (farkli z yok).");
        return;
    }

    QString chosenFloor = QInputDialog::getItem(this,
        "Kolon Boru Ciz — Hedef Kat",
        "Kolonun uzanacagi kati secin:",
        floorItems, 0, false, &ok);
    if (!ok) return;

    int flIdx = floorItems.indexOf(chosenFloor);
    // Map back to actual floor (skipping srcFloor)
    int actualFloorIdx = 0;
    int mapped = 0;
    for (const auto& fl : floors) {
        if (fl.index == srcFloor) continue;
        if (mapped == flIdx) { actualFloorIdx = fl.index; break; }
        ++mapped;
    }
    const core::Floor* targetFloor = m_floorManager.GetFloor(actualFloorIdx);
    if (!targetFloor) return;

    // ── Kolon Boru Oluştur ─────────────────────────────────
    // Node Z değerleri elevation_m ile aynı ölçektedir (metre)
    double targetZ = targetFloor->elevation_m;

    auto composite = std::make_unique<core::CompositeCommand>();

    // Hedef katta aynı XY'de node var mı? Yoksa oluştur.
    uint32_t dstNodeId = 0;
    bool foundExisting = false;
    constexpr double XY_TOL = 50.0;  // mm tolerans (X,Y düzlemi)
    constexpr double Z_TOL  = 0.15;  // m tolerans  (Z düzlemi — elevation_m ile aynı birim)
    for (const auto& [id, nd] : nodeMap) {
        if (std::abs(nd.position.x - srcNode->position.x) < XY_TOL &&
            std::abs(nd.position.y - srcNode->position.y) < XY_TOL &&
            std::abs(nd.position.z - targetZ)             < Z_TOL) {
            dstNodeId = id;
            foundExisting = true;
            break;
        }
    }
    if (!foundExisting) {
        mep::Node dstNode;
        dstNode.type     = mep::NodeType::Junction;
        dstNode.position = geom::Vec3(srcNode->position.x, srcNode->position.y, targetZ);
        dstNode.label    = "J";
        auto addDst = std::make_unique<core::AddNodeCommand>(network, dstNode);
        addDst->Execute();
        dstNodeId = addDst->GetNodeId();
        composite->AddCommand(std::move(addDst));
    }

    // Dikey boru uzunluğu — Z farkından hesapla (her ikisi de metre)
    double dz_m = std::abs(targetFloor->elevation_m - srcNode->position.z);
    if (dz_m < 0.01) dz_m = 3.0;

    mep::Edge edge;
    const std::string colMaterial = m_propMaterial ? m_propMaterial->currentText().toStdString() : "PVC";
    edge.nodeA       = srcNodeId;
    edge.nodeB       = dstNodeId;
    edge.type        = m_currentPipeType;
    edge.diameter_mm = 25.0;
    edge.roughness_mm = mep::Database::Instance().GetPipe(colMaterial).roughness_mm;
    edge.material    = colMaterial;
    edge.length_m    = dz_m;
    auto addEdge = std::make_unique<core::AddEdgeCommand>(network, edge);
    addEdge->Execute();
    composite->AddCommand(std::move(addEdge));

    m_document->TrackExecuted(std::move(composite));
    m_document->SetModified(true);
    UpdateUI();
    ScheduleAutoHydro();

    QString msg = QString("Kolon boru eklendi: Kat %1 → Kat %2 (%3 m, Ø25 mm)")
        .arg(srcFloor).arg(actualFloorIdx).arg(dz_m, 0, 'f', 2);
    statusBar()->showMessage(msg);
    if (m_logList) m_logList->addItem(msg);
}

// ============================================================
//  PDF PAFTA DÜZENİ
// ============================================================
void MainWindow::OnPrintLayout() {
    if (!m_document) {
        QMessageBox::warning(this, "Pafta", "Aktif belge yok.");
        return;
    }
    if (!RunPreflight()) return;

    // PrintLayout'u çizim verisiyle doldur
    PrintLayout layout;
    layout.SetEntities(&m_document->GetCADEntities());
    layout.SetNetwork(&m_document->GetNetwork());
    layout.SetAutoScale(true);
    layout.SetPaperSize(PaperSize::A3_Landscape);

    // Proje kök klasörü ve adı
    auto& pm = core::ProjectManager::Instance();
    std::string projectsRoot = pm.GetProjectFolder().empty()
        ? pm.GetProjectsRoot()
        : pm.GetProjectFolder();

    // Başlık bloğunu proje bilgileriyle önceden doldur
    TitleBlock tb;
    tb.projectName  = pm.GetProjectName();
    tb.drawingTitle = "Tesisat Paftasi";
    tb.drawingNumber = "P-001";
    tb.revision      = "A";
    tb.date          = QDate::currentDate().toString("yyyy-MM-dd").toStdString();
    tb.standard      = "TS EN 806-3 / EN 12056-2";
    layout.SetTitleBlock(tb);

    PrintLayoutDialog dlg(layout, projectsRoot, this);
    dlg.SetInitialTitleBlock(tb);
    dlg.exec();

    if (!dlg.LastSavedPath().isEmpty()) {
        LogCAD("Pafta kaydedildi: " + dlg.LastSavedPath().toStdString());
        statusBar()->showMessage("Pafta kaydedildi: " + dlg.LastSavedPath());
    }
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
          << "DIN 1988-300  (Alman standardi — esszamanlilik katsayili)"
          << "TS 825 Sarfiyat  (Musluk Birimi — Turkiye kamu ihalelerinde kullanilir)";

    auto cur = mep::HydraulicSolver::GlobalNorm();
    int current = (cur == mep::HydroNorm::DIN1988) ? 1
                : (cur == mep::HydroNorm::SARFIYAT) ? 2 : 0;

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
    } else if (chosen.startsWith("TS 825")) {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::SARFIYAT;
        statusBar()->showMessage("Hesap normu: TS 825 Sarfiyat / Musluk Birimi");
        if (m_logList) m_logList->addItem("Norm degistirildi: TS 825 Sarfiyat (Musluk Birimi)");
    } else {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;
        statusBar()->showMessage("Hesap normu: TS EN 806-3");
        if (m_logList) m_logList->addItem("Norm degistirildi: TS EN 806-3");
    }
    ScheduleAutoHydro();
}

// ============================================================
//  DEVRE SEÇENEKLERİ (FineSANI eşdeğeri)
// ============================================================
void MainWindow::OnDevreSecenekleri() {
    DevreSecenekleriDialog dlg(m_devreParams, this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_devreParams = dlg.GetParams();

    // Norm senkronizasyonu
    if (m_devreParams.norm == "DIN 1988-300")
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
    else
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;

    // Pürüzlülüğü tüm Supply/HotWater edgelere uygula (mevcut ağa)
    if (m_document) {
        auto& network = m_document->GetNetwork();
        for (auto& [eid, edge] : network.GetEdgeMap()) {
            if (edge.type == mep::EdgeType::Supply || edge.type == mep::EdgeType::HotWater) {
                edge.roughness_mm = m_devreParams.roughness_mm;
                edge.material     = m_devreParams.mainPipeMat.toStdString();
            }
        }
        m_document->SetModified(true);
        ScheduleAutoHydro();
    }

    QString msg = QString("Devre Seçenekleri uygulandı — %1, %2, pürüzlülük=%3 mm, max hız=%4 m/s")
        .arg(m_devreParams.norm)
        .arg(m_devreParams.mainPipeMat)
        .arg(m_devreParams.roughness_mm, 0, 'f', 4)
        .arg(m_devreParams.maxVelocity_ms, 0, 'f', 1);
    statusBar()->showMessage(msg, 3000);
    if (m_logList) m_logList->addItem(msg);
}

// ============================================================
//  BASKI İÇERİĞİ — çizimde hangi etiketler görünsün
// ============================================================
void MainWindow::OnBaskiIcerigi() {
    QDialog dlg(this);
    dlg.setWindowTitle("Baskı İçeriği — Çizimde Görünecek Değerler");
    dlg.setMinimumWidth(320);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("<b>Boru etiketi bileşenleri:</b>"));

    auto* cbDN       = new QCheckBox("DN (çap, örn: DN32)");              cbDN->setChecked(m_labelShowDN);
    auto* cbFlow     = new QCheckBox("Debi Q (L/s)");                     cbFlow->setChecked(m_labelShowFlow);
    auto* cbLen      = new QCheckBox("Uzunluk L (m)");                    cbLen->setChecked(m_labelShowLength);
    auto* cbVel      = new QCheckBox("Hız v (m/s)  [Temiz Su]");          cbVel->setChecked(m_labelShowVelocity);
    auto* cbHL       = new QCheckBox("Basınç kaybı ΔH (m)  [Temiz Su]"); cbHL->setChecked(m_labelShowHeadLoss);

    layout->addWidget(cbDN);
    layout->addWidget(cbFlow);
    layout->addWidget(cbLen);
    layout->addWidget(cbVel);
    layout->addWidget(cbHL);

    layout->addWidget(new QLabel("<b>Pis Su ek etiketleri:</b>"));

    auto* cbSlope    = new QCheckBox("Eğim i (%)  [Pis Su]");             cbSlope->setChecked(m_labelShowSlope);
    auto* cbFillRate = new QCheckBox("Doluluk h/d (%)  [Pis Su]");        cbFillRate->setChecked(m_labelShowFillRate);

    layout->addWidget(cbSlope);
    layout->addWidget(cbFillRate);

    auto* note = new QLabel("<small><i>Seçimler anlık etiket overlay'ini etkiler.</i></small>");
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    m_labelShowDN       = cbDN->isChecked();
    m_labelShowFlow     = cbFlow->isChecked();
    m_labelShowLength   = cbLen->isChecked();
    m_labelShowVelocity = cbVel->isChecked();
    m_labelShowHeadLoss = cbHL->isChecked();
    m_labelShowSlope    = cbSlope->isChecked();
    m_labelShowFillRate = cbFillRate->isChecked();

    RefreshTextOverlay();
    statusBar()->showMessage("Baskı İçeriği güncellendi — etiketler yenilendi", 2000);
}

// ============================================================
//  PARÇALARIN BASINÇ KAYBI — tüm devreler detay tablosu
// ============================================================
void MainWindow::OnBaskiKaybi() {
    if (!m_document) return;

    // Önce hesapla
    auto& network = m_document->GetNetwork();
    mep::HydraulicSolver solver(network);
    solver.Solve();

    QString html;
    html += "<html><body style='font-family:Arial;font-size:11px'>";
    html += "<h3>Parçaların Basınç Kaybı Tablosu</h3>";
    html += "<table border='1' cellpadding='4' cellspacing='0' width='100%'>";
    html += "<tr style='background:#2255aa;color:white'>"
            "<th>Devre No</th><th>Tip</th><th>Malzeme</th>"
            "<th>DN (mm)</th><th>L (m)</th>"
            "<th>Q_nom (L/s)</th><th>Q_hes (L/s)</th>"
            "<th>v (m/s)</th><th>ΔH_surt (m)</th><th>ΔH_lokal (m)</th><th>ΔH_top (m)</th><th>Durum</th></tr>";

    // Kritik devreyi bul
    mep::HydraulicSolver critSolver(network);
    auto critResult = critSolver.CalculateCriticalPath();
    std::set<uint32_t> critSet(critResult.criticalPath.begin(), critResult.criticalPath.end());

    int row = 0;
    double totalHL = 0.0;
    uint32_t criticalEdgeId = 0;
    double   maxHL = 0.0;

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type == mep::EdgeType::Drainage || edge.type == mep::EdgeType::Vent) continue;

        bool isCrit = (critSet.count(eid) > 0);
        QString bg  = isCrit ? "#fff3cd" : (row % 2 == 0 ? "#f5f8ff" : "white");

        QString typeStr;
        switch (edge.type) {
            case mep::EdgeType::Supply:   typeStr = "Soğuk Su"; break;
            case mep::EdgeType::HotWater: typeStr = "Sıcak Su"; break;
            default:                       typeStr = "Diğer";    break;
        }

        double qNom = (edge.nominalFlow_Ls > 0.0) ? edge.nominalFlow_Ls : edge.flowRate_m3s * 1000.0;
        double qHes = edge.flowRate_m3s * 1000.0;
        QString qNomStr = QString("%1").arg(qNom, 0, 'f', 3);
        // Eğer DIN simultaneity uygulandıysa nominal > hesap → vurgula
        QString qHesStr = (qNom > qHes + 0.001)
            ? QString("<font color='#00a'>%1</font>").arg(qHes, 0, 'f', 3)
            : QString("%1").arg(qHes, 0, 'f', 3);

        double localLoss_m = edge.localLoss_Pa / (1000.0 * 9.81); // Pa → m
        double frictLoss_m = edge.headLoss_m - localLoss_m;       // sürtünme kaybı
        if (frictLoss_m < 0.0) frictLoss_m = edge.headLoss_m;

        html += QString("<tr style='background:%1'>"
                        "<td>%2</td><td>%3</td><td>%4</td>"
                        "<td>%5</td><td>%6</td>"
                        "<td>%7</td><td>%8</td>"
                        "<td>%9</td><td>%10</td><td>%11</td><td>%12</td><td>%13</td></tr>")
            .arg(bg)
            .arg(edge.label.empty() ? QString("E%1").arg(eid) : QString::fromStdString(edge.label))
            .arg(typeStr)
            .arg(QString::fromStdString(edge.material))
            .arg(edge.diameter_mm, 0, 'f', 0)
            .arg(edge.length_m, 0, 'f', 2)
            .arg(qNomStr)
            .arg(qHesStr)
            .arg(edge.velocity_ms, 0, 'f', 2)
            .arg(frictLoss_m,      0, 'f', 4)
            .arg(localLoss_m,      0, 'f', 4)
            .arg(edge.headLoss_m,  0, 'f', 4)
            .arg(isCrit ? "<b style='color:#c00'>KRİTİK</b>" : "OK");

        totalHL += edge.headLoss_m;
        if (edge.headLoss_m > maxHL) { maxHL = edge.headLoss_m; criticalEdgeId = eid; }
        ++row;
    }

    html += "</table>";
    html += QString("<p><b>Toplam kayıp (supply ağı):</b> %1 m &nbsp;&nbsp;"
                    "<b>Kritik devre toplam:</b> <span style='color:#c00'>%2 m</span></p>")
        .arg(totalHL, 0, 'f', 3)
        .arg(critResult.totalHeadLoss_m, 0, 'f', 3);
    html += "</body></html>";

    QDialog dlg(this);
    dlg.setWindowTitle("Parçaların Basınç Kaybı");
    dlg.resize(860, 520);
    auto* browser = new QTextBrowser(&dlg);
    browser->setHtml(html);
    auto* savBtn = new QPushButton("PDF Kaydet", &dlg);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addButton(savBtn, QDialogButtonBox::ActionRole);
    auto* vl = new QVBoxLayout(&dlg);
    vl->addWidget(browser);
    vl->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    connect(savBtn, &QPushButton::clicked, [&](){
        QString path = QFileDialog::getSaveFileName(&dlg, "PDF Kaydet", "", "PDF (*.pdf)");
        if (path.isEmpty()) return;
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
                              QPageLayout::Landscape, QMarginsF(15,10,15,10)));
        browser->print(&printer);
    });
    dlg.exec();
}

// ============================================================
//  WORD DOCX RAPOR EXPORT (FineSANI Word dosyası eşdeğeri — OOXML .docx)
// ============================================================
void MainWindow::OnWordRapor() {
    if (!m_document) return;
    if (!RunPreflight()) return;

    auto& network = m_document->GetNetwork();
    mep::HydraulicSolver solver(network);
    solver.Solve();
    auto critResult = solver.CalculateCriticalPath();

    auto& pm       = core::ProjectManager::Instance();
    QString projName = QString::fromStdString(pm.GetProjectName());
    QString today    = QDate::currentDate().toString("dd.MM.yyyy");

    // Dosya yolu
    QString defaultPath;
    if (!pm.GetProjectFolder().empty())
        defaultPath = QString::fromStdString(pm.GetProjectFolder()) + "/rapor/";
    defaultPath += (projName.isEmpty() ? "rapor" : projName) + "_hesap_foyu.docx";

    QString path = QFileDialog::getSaveFileName(this, "Word Rapor Kaydet (.docx)",
                                                defaultPath,
                                                "Word Belgesi (*.docx);;Tum Dosyalar (*)");
    if (path.isEmpty()) return;

    core::DocxReportParams params;
    params.projectName    = projName.toStdString();
    params.norm           = m_devreParams.norm.toStdString();
    params.mainMaterial   = m_devreParams.mainPipeMat.toStdString();
    params.branchMaterial = m_devreParams.branchPipeMat.toStdString();
    params.buildingType   = m_devreParams.buildingType.toStdString();
    params.roughness_mm   = m_devreParams.roughness_mm;
    params.maxVelocity_ms = m_devreParams.maxVelocity_ms;
    params.date           = today.toStdString();

    if (core::DocxWriter::Write(path.toStdString(), network, critResult, params)) {
        if (m_logList) m_logList->addItem("Word .docx rapor kaydedildi: " + path);
        statusBar()->showMessage("Word rapor kaydedildi: " + path, 4000);
        QMessageBox::information(this, "Rapor Kaydedildi",
            QString("Word belgesi kaydedildi:\n%1\n\nMicrosoft Word ile acilabiilir.").arg(path));
    } else {
        QMessageBox::warning(this, "Hata", "Dosya yazilimadi: " + path);
    }
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
    // Maliyet tahmini — malzeme bazlı birim fiyat
    auto& db = mep::Database::Instance();
    std::string mainMat = m_devreParams.mainPipeMat.toStdString();
    double unitPrice = db.GetPipe(mainMat).unitPrice_TL;
    // Çap katsayısı: DN'e göre fiyat orantılı artış (DN20=1x, DN25=1.3x, DN32=1.7x, vb.)
    auto dnPriceFactor = [](int dn) -> double {
        if (dn <= 16) return 0.8;
        if (dn <= 20) return 1.0;
        if (dn <= 25) return 1.3;
        if (dn <= 32) return 1.7;
        if (dn <= 40) return 2.3;
        if (dn <= 50) return 3.0;
        if (dn <= 63) return 4.0;
        if (dn <= 75) return 5.5;
        if (dn <= 90) return 7.0;
        return 10.0;
    };

    msg += "<b>Boru Metrajlari ve Maliyet Tahmini:</b><br>";
    msg += QString("<small>Malzeme: %1 — Birim fiyat (DN20): %2 TL/m</small><br>")
               .arg(QString::fromStdString(mainMat)).arg(unitPrice, 0, 'f', 0);
    msg += "<table border='1' cellspacing='0' cellpadding='3'>";
    msg += "<tr><th>DN (mm)</th><th>Uzunluk (m)</th><th>Parca</th><th>Birim Fiyat (TL/m)</th><th>Tutar (TL)</th></tr>";

    double totalLength = 0.0, totalCost = 0.0;
    for (const auto& [dn, len] : pipeLength_m) {
        double price  = unitPrice * dnPriceFactor(dn);
        double cost   = len * price;
        totalCost    += cost;
        msg += QString("<tr><td>DN%1</td><td>%2</td><td>%3</td><td>%4</td><td><b>%5</b></td></tr>")
                   .arg(dn).arg(len, 0, 'f', 2).arg(pipeCount[dn])
                   .arg(price, 0, 'f', 0).arg(cost, 0, 'f', 0);
        totalLength += len;
    }
    msg += QString("<tr style='background:#e8f4e8'><td><b>TOPLAM</b></td><td><b>%1 m</b></td>"
                   "<td><b>%2</b></td><td>—</td><td><b>%3 TL</b></td></tr>")
               .arg(totalLength, 0, 'f', 2).arg((int)network.GetEdgeCount())
               .arg(totalCost, 0, 'f', 0);
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

// ============================================================
//  KOLON ŞEMASI (RISER DIAGRAM)
// ============================================================
void MainWindow::OnRiserDiagram() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Kolon Semasi",
            "Once tesisat sebekesi cizin (boru + kaynak + armaturler).");
        return;
    }
    if (!RunPreflight()) return;

    mep::RiserDiagram riser(network, m_floorManager);
    auto data = riser.Generate();
    std::string svgContent = riser.ToSVG(data);
    std::string txtContent = riser.ToText(data);

    QDialog dlg(this);
    dlg.setWindowTitle("Kolon Semasi (Riser Diagram)");
    dlg.resize(720, 520);

    auto* layout   = new QVBoxLayout(&dlg);
    auto* browser  = new QTextBrowser(&dlg);
    browser->setOpenLinks(false);
    // SVG'yi HTML iframe olarak goster
    QString svgHtml = QString(
        "<html><body style='margin:0;background:#1a1a2e;'>"
        "<div style='padding:8px;'>%1</div>"
        "<pre style='color:#ccc;font-size:11px;padding:8px;'>%2</pre>"
        "</body></html>"
    ).arg(QString::fromStdString(svgContent))
     .arg(QString::fromStdString(txtContent).toHtmlEscaped());

    browser->setHtml(svgHtml);
    layout->addWidget(browser);

    auto* btnRow    = new QHBoxLayout();
    auto* btnSvg    = new QPushButton("SVG Kaydet...", &dlg);
    auto* btnPdf    = new QPushButton("PDF Kaydet...", &dlg);
    auto* btnDxf    = new QPushButton("DXF Kaydet...", &dlg);
    auto* btnClose  = new QPushButton("Kapat", &dlg);
    btnRow->addWidget(btnSvg);
    btnRow->addWidget(btnPdf);
    btnRow->addWidget(btnDxf);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    layout->addLayout(btnRow);

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    // SVG kaydet
    connect(btnSvg, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — SVG Kaydet", startDir, "SVG Dosyasi (*.svg)");
        if (path.isEmpty()) return;
        std::ofstream f(path.toStdString());
        if (f.is_open()) {
            f << svgContent;
            statusBar()->showMessage(QString("SVG kaydedildi: %1").arg(path), 3000);
        } else {
            QMessageBox::critical(&dlg, "Hata", "Dosya yazılamadi!");
        }
    });

    // PDF kaydet (QPrinter + QSvgRenderer)
    connect(btnPdf, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — PDF Kaydet", startDir, "PDF Dosyasi (*.pdf)");
        if (path.isEmpty()) return;

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageSize(QPageSize(QPageSize::A3));
        printer.setPageOrientation(QPageLayout::Landscape);

        QSvgRenderer renderer(QByteArray::fromStdString(svgContent));
        if (!renderer.isValid()) {
            QMessageBox::critical(&dlg, "PDF Hatasi", "SVG gecersiz — PDF olusturulamadi.");
            return;
        }

        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::critical(&dlg, "PDF Hatasi", "PDF yazici baslatılamadi.");
            return;
        }
        QRectF pageRect = printer.pageLayout().paintRectPixels(printer.resolution());
        renderer.render(&painter, pageRect);
        painter.end();

        statusBar()->showMessage(QString("PDF kaydedildi: %1").arg(path), 3000);
    });

    // DXF kaydet — R12 format, LINE + TEXT entity'leri
    connect(btnDxf, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — DXF Kaydet", startDir, "DXF Dosyasi (*.dxf)");
        if (path.isEmpty()) return;

        std::ofstream f(path.toStdString());
        if (!f.is_open()) {
            QMessageBox::critical(&dlg, "Hata", "DXF dosyasi yazilamadi!");
            return;
        }

        // DXF R12 minimal format
        f << "0\nSECTION\n2\nHEADER\n0\nENDSEC\n";
        f << "0\nSECTION\n2\nENTITIES\n";

        for (const auto& ln : data.lines) {
            f << "0\nLINE\n8\n0\n";
            f << "10\n" << ln.a.x << "\n20\n" << -ln.a.y << "\n30\n0.0\n";
            f << "11\n" << ln.b.x << "\n21\n" << -ln.b.y << "\n31\n0.0\n";
        }
        for (const auto& lb : data.labels) {
            f << "0\nTEXT\n8\n0\n";
            f << "10\n" << lb.x << "\n20\n" << -lb.y << "\n30\n0.0\n";
            f << "40\n" << lb.fontSize << "\n";
            f << "1\n" << lb.text << "\n";
        }

        f << "0\nENDSEC\n0\nEOF\n";
        statusBar()->showMessage(QString("DXF kaydedildi: %1").arg(path), 3000);
    });

    if (m_logList)
        m_logList->addItem(QString("Kolon semasi: %1 kolon, %2 kat")
            .arg((int)data.columns.size()).arg((int)m_floorManager.GetFloorCount()));

    dlg.exec();
}

// ============================================================
//  HESAP FÖYÜ — DN MANUEL OVERRIDE
// ============================================================
void MainWindow::OnDNOverride() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "DN Override",
            "Oncelikle tesisat sebekesi cizin.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Hesap Foyu — DN Manuel Override");
    dlg.resize(650, 400);

    auto* layout = new QVBoxLayout(&dlg);
    auto* info   = new QLabel(
        "<b>Boru caplarinizi asagidaki tablodan duzenleyebilirsiniz.</b><br>"
        "Tamam'a basinca secilen degerler aninda uygulanir ve DN etiketleri guncellenir.", &dlg);
    info->setWordWrap(true);
    layout->addWidget(info);

    // Tablo: Boru ID | Tip | Malzeme | Mevcut DN | Yeni DN (editable)
    auto* table = new QTableWidget(&dlg);
    static const QStringList kDNList = {
        "16","20","25","32","40","50","63","75","90","110","125","160","200"
    };

    // Edge'leri sabit sirayla al
    std::vector<std::pair<uint32_t, const mep::Edge*>> edges;
    for (const auto& [eid, edge] : network.GetEdgeMap())
        edges.emplace_back(eid, &edge);
    std::sort(edges.begin(), edges.end(),
        [](const auto& a, const auto& b){ return a.first < b.first; });

    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"Boru ID","Tip","Malzeme","Mevcut DN","Yeni DN"});
    table->setRowCount(static_cast<int>(edges.size()));
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    for (int row = 0; row < static_cast<int>(edges.size()); ++row) {
        auto [eid, edge] = edges[row];
        // Boru ID
        auto* idItem = new QTableWidgetItem(QString::number(eid));
        idItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 0, idItem);
        // Tip
        QString tipStr = (edge->type == mep::EdgeType::Supply) ? "Temiz Su"
                       : (edge->type == mep::EdgeType::Drainage) ? "Pis Su" : "Hava";
        auto* tipItem = new QTableWidgetItem(tipStr);
        tipItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 1, tipItem);
        // Malzeme
        auto* matItem = new QTableWidgetItem(QString::fromStdString(edge->material));
        matItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 2, matItem);
        // Mevcut DN
        int curDN = static_cast<int>(std::round(edge->diameter_mm));
        auto* curItem = new QTableWidgetItem(QString("DN%1").arg(curDN));
        curItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 3, curItem);
        // Yeni DN: ComboBox
        auto* combo = new QComboBox(&dlg);
        combo->addItems(kDNList);
        // Mevcut secimi bul
        int curIdx = kDNList.indexOf(QString::number(curDN));
        if (curIdx < 0) curIdx = 0;
        combo->setCurrentIndex(curIdx);
        table->setCellWidget(row, 4, combo);
    }
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->resizeColumnsToContents();
    layout->addWidget(table);

    // Toplu DN değiştirme araç çubuğu
    auto* batchRow   = new QHBoxLayout();
    auto* batchLabel = new QLabel("<b>Seçili satırlara toplu uygula:</b>", &dlg);
    auto* batchCombo = new QComboBox(&dlg);
    batchCombo->addItems(kDNList);
    auto* batchBtn   = new QPushButton("Seçilenlere Uygula", &dlg);
    batchBtn->setToolTip("Tabloda işaretlenen tüm satırların DN değerini aynı anda değiştirir");
    batchRow->addWidget(batchLabel);
    batchRow->addWidget(batchCombo);
    batchRow->addWidget(batchBtn);
    batchRow->addStretch();
    layout->addLayout(batchRow);

    connect(batchBtn, &QPushButton::clicked, [&]() {
        const QString targetDN = batchCombo->currentText();
        const auto selected = table->selectionModel()->selectedRows();
        if (selected.isEmpty()) {
            QMessageBox::information(&dlg, "Toplu DN",
                "Lütfen önce tabloda satır seçin (Ctrl+Tık veya Shift+Tık).");
            return;
        }
        for (const auto& idx : selected) {
            auto* combo = qobject_cast<QComboBox*>(table->cellWidget(idx.row(), 4));
            if (combo) combo->setCurrentText(targetDN);
        }
        statusBar()->showMessage(
            QString("%1 satira DN%2 atandi").arg(selected.size()).arg(targetDN), 2000);
    });

    auto* btnRow    = new QHBoxLayout();
    auto* btnExport = new QPushButton("XLS Olarak Kaydet...", &dlg);
    auto* btnApply  = new QPushButton("Tamam (Uygula)", &dlg);
    auto* btnCancel = new QPushButton("Iptal", &dlg);
    btnRow->addWidget(btnExport);
    btnRow->addStretch();
    btnRow->addWidget(btnApply);
    btnRow->addWidget(btnCancel);
    layout->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    // XLS export — mevcut ag durumunu yaz (solver once calistirilmali)
    connect(btnExport, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Hesap Foyu Kaydet", startDir, "Excel Dosyasi (*.xls)");
        if (path.isEmpty()) return;

        // Solver'ı çalıştır (güncel flowRate/velocity/headLoss ile)
        mep::HydraulicSolver solver(network);
        solver.Solve();
        solver.SolveDrainage();

        if (mep::XLSXWriter::ExportCalculationSheet(path.toStdString(), network)) {
            statusBar()->showMessage(QString("Hesap foyu kaydedildi: %1").arg(path), 3000);
            if (m_logList) m_logList->addItem(QString("Hesap foyu XLS: %1").arg(path));
        } else {
            QMessageBox::critical(&dlg, "Hata", "XLS dosyasi yazılamadi!");
        }
    });

    connect(btnApply, &QPushButton::clicked, [&]() {
        int changed = 0;
        for (int row = 0; row < table->rowCount(); ++row) {
            uint32_t eid = table->item(row, 0)->text().toUInt();
            auto* combo  = qobject_cast<QComboBox*>(table->cellWidget(row, 4));
            if (!combo) continue;
            double newDN = combo->currentText().toDouble();
            mep::Edge* edge = network.GetEdge(eid);
            if (edge && std::abs(edge->diameter_mm - newDN) > 0.5) {
                edge->diameter_mm = newDN;
                edge->label = "DN" + std::to_string(static_cast<int>(newDN));
                ++changed;
            }
        }
        if (changed > 0) {
            m_document->SetModified(true);
            RefreshTextOverlay();
            if (m_vulkanWindow) m_vulkanWindow->requestUpdate();
            if (m_logList)
                m_logList->addItem(QString("DN Override: %1 boru guncellendi").arg(changed));
            statusBar()->showMessage(
                QString("DN Override: %1 boru capl degistirildi").arg(changed));
        }
        dlg.accept();
    });

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

// ============================================================
//  CİZİMİ GUNCELLE — hesap sonuclarini cizime yaz
// ============================================================
// ============================================================
//  LAYISO / LAYUNISO / PURGE / SelectByLayer / MoveToLayer
// ============================================================
void MainWindow::OnLayIso() {
    if (!m_document) return;

    // Hangi katmanı izole edeceğiz? Seçili entity'den al, yoksa kullanıcıya sor
    std::string targetLayer;
    if (m_selectedCADEntityId != 0) {
        auto it = m_cadEntityCache.find(m_selectedCADEntityId);
        if (it != m_cadEntityCache.end() && it->second)
            targetLayer = it->second->GetLayerName();
    }
    if (targetLayer.empty() && !m_selectedCADEntityIds.empty()) {
        auto it = m_cadEntityCache.find(*m_selectedCADEntityIds.begin());
        if (it != m_cadEntityCache.end() && it->second)
            targetLayer = it->second->GetLayerName();
    }
    if (targetLayer.empty()) {
        // Katman adını kullanıcıdan sor
        QStringList layerNames;
        for (const auto& [n, _] : m_document->GetLayers())
            layerNames << QString::fromStdString(n);
        layerNames.sort();
        bool ok = false;
        QString chosen = QInputDialog::getItem(this, "LAYISO", "İzole edilecek katman:", layerNames, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        targetLayer = chosen.toStdString();
    }

    // Mevcut görünürlük durumunu kaydet
    m_preIsoVisibility.clear();
    for (auto& [name, layer] : m_document->GetLayersMutable()) {
        m_preIsoVisibility[name] = layer.IsVisible();
        layer.SetVisible(name == targetLayer);
    }
    m_layisoActive = true;

    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage(QString("LAYISO: '%1' katmanı izole edildi — LAYUNISO ile geri al")
        .arg(QString::fromStdString(targetLayer)), 4000);
}

void MainWindow::OnLayUnIso() {
    if (!m_document) return;
    if (!m_layisoActive) {
        statusBar()->showMessage("LAYUNISO: Aktif izolasyon yok", 2000);
        return;
    }
    for (auto& [name, layer] : m_document->GetLayersMutable()) {
        auto it = m_preIsoVisibility.find(name);
        layer.SetVisible(it != m_preIsoVisibility.end() ? it->second : true);
    }
    m_layisoActive = false;
    m_preIsoVisibility.clear();
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage("LAYUNISO: Tüm katmanlar eski haline döndürüldü", 2000);
}

void MainWindow::OnLayLockAll() {
    if (!m_document) return;
    for (auto& [name, layer] : m_document->GetLayersMutable())
        layer.SetLocked(true);
    RefreshLayerPanel();
    statusBar()->showMessage("Tüm katmanlar kilitlendi", 2000);
}

void MainWindow::OnLayUnlockAll() {
    if (!m_document) return;
    for (auto& [name, layer] : m_document->GetLayersMutable())
        layer.SetLocked(false);
    RefreshLayerPanel();
    statusBar()->showMessage("Tüm katman kilitleri kaldırıldı", 2000);
}

void MainWindow::OnPurge() {
    if (!m_document) return;

    // Hangi layer adları entity'lerde kullanılıyor?
    std::unordered_set<std::string> usedLayers;
    for (const auto& e : m_document->GetCADEntities())
        if (e) usedLayers.insert(e->GetLayerName());
    // MEP node/edge de layer kullanmaz, "0" her zaman korunur
    usedLayers.insert("0");

    auto& layers = m_document->GetLayersMutable();
    std::vector<std::string> toRemove;
    for (const auto& [name, _] : layers)
        if (!usedLayers.count(name)) toRemove.push_back(name);

    if (toRemove.empty()) {
        QMessageBox::information(this, "PURGE", "Kullanılmayan katman veya blok bulunamadı.");
        return;
    }

    QString msg = QString("%1 kullanılmayan katman bulundu:\n").arg(toRemove.size());
    for (const auto& n : toRemove) msg += QString("  • %1\n").arg(QString::fromStdString(n));
    msg += "\nSilmek istiyor musunuz?";
    if (QMessageBox::question(this, "PURGE", msg) != QMessageBox::Yes) return;

    for (const auto& n : toRemove) layers.erase(n);
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    m_document->SetModified(true);
    statusBar()->showMessage(QString("PURGE: %1 katman silindi").arg(toRemove.size()), 3000);
}

void MainWindow::OnSelectByLayer(const QString& layerName) {
    if (!m_document) return;
    const std::string layStd = layerName.toStdString();

    m_selectedCADEntityIds.clear();
    m_selectedCADEntityId = 0;

    for (const auto& e : m_document->GetCADEntities()) {
        if (!e || !e->IsVisible()) continue;
        const auto& layers = m_document->GetLayers();
        auto lit = layers.find(e->GetLayerName());
        if (lit != layers.end() && (lit->second.IsLocked() || !lit->second.IsDrawable())) continue;
        if (e->GetLayerName() == layStd)
            m_selectedCADEntityIds.insert(e->GetId());
    }

    if (!m_selectedCADEntityIds.empty()) {
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
        statusBar()->showMessage(
            QString("'%1' katmanında %2 entity seçildi").arg(layerName).arg(m_selectedCADEntityIds.size()), 4000);
    } else {
        statusBar()->showMessage(QString("'%1' katmanında seçilebilir entity bulunamadı").arg(layerName), 2000);
    }
}

void MainWindow::OnMoveToLayer() {
    if (!m_document) return;

    // Hedef katman adı: m_moveToLayerTarget dolu değilse kullanıcıdan sor
    if (m_moveToLayerTarget.isEmpty()) {
        QStringList layerNames;
        for (const auto& [n, _] : m_document->GetLayers())
            layerNames << QString::fromStdString(n);
        layerNames.sort();
        bool ok = false;
        QString chosen = QInputDialog::getItem(this, "Katmana Taşı", "Hedef katman:", layerNames, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        m_moveToLayerTarget = chosen;
    }

    const std::string targetStd = m_moveToLayerTarget.toStdString();
    m_moveToLayerTarget.clear();

    // Hedef katman yoksa oluştur
    auto& layers = m_document->GetLayersMutable();
    if (!layers.count(targetStd))
        layers[targetStd] = cad::Layer(targetStd);

    // Seçili entity'leri taşı
    auto moveIds = m_selectedCADEntityIds;
    if (moveIds.empty() && m_selectedCADEntityId != 0) moveIds.insert(m_selectedCADEntityId);

    int moved = 0;
    for (const auto& e : m_document->GetCADEntities()) {
        if (!e || !moveIds.count(e->GetId())) continue;
        e->SetLayerName(targetStd);
        ++moved;
    }

    if (moved > 0) {
        m_document->SetModified(true);
        RebuildCADEntityCache();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetLayerMap(layers);
        InvalidateRenderer();
        RefreshLayerPanel();
        statusBar()->showMessage(
            QString("%1 entity '%2' katmanına taşındı").arg(moved).arg(QString::fromStdString(targetStd)), 3000);
    }
}

// ═══════════════════════════════════════════════════════════
//  LAYERSTATE — adlı katman anlık görüntüsü
// ═══════════════════════════════════════════════════════════
void MainWindow::OnLayerStateSave() {
    if (!m_document) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, "Katman Durumu Kaydet",
        "Anlık görüntü adı:", QLineEdit::Normal,
        QString("Durum_%1").arg(m_layerStates.size() + 1), &ok);
    if (!ok || name.isEmpty()) return;

    LayerSnapshot snap;
    snap.name = name.toStdString();
    for (const auto& [n, layer] : m_document->GetLayers()) {
        snap.visibility[n] = layer.IsVisible();
        snap.locked[n]     = layer.IsLocked();
        snap.frozen[n]     = layer.IsFrozen();
    }
    // Aynı isimli varsa güncelle
    for (auto& s : m_layerStates) {
        if (s.name == snap.name) { s = snap; goto saved; }
    }
    m_layerStates.push_back(snap);
saved:
    statusBar()->showMessage(QString("Katman durumu kaydedildi: %1").arg(name), 3000);
    if (m_logList) m_logList->addItem(QString("LAYERSTATE: '%1' kaydedildi (%2 katman)")
        .arg(name).arg(snap.visibility.size()));
}

void MainWindow::OnLayerStateRestore() {
    if (!m_document || m_layerStates.empty()) {
        statusBar()->showMessage("Kayıtlı katman durumu yok — önce LAYERSTATE ile kaydedin", 3000);
        return;
    }
    QStringList names;
    for (const auto& s : m_layerStates) names << QString::fromStdString(s.name);
    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Katman Durumu Yükle",
        "Hangi anlık görüntü?", names, 0, false, &ok);
    if (!ok) return;

    const LayerSnapshot* snap = nullptr;
    for (const auto& s : m_layerStates)
        if (s.name == chosen.toStdString()) { snap = &s; break; }
    if (!snap) return;

    auto& layers = m_document->GetLayersMutable();
    for (auto& [n, layer] : layers) {
        auto visIt = snap->visibility.find(n);
        if (visIt != snap->visibility.end()) layer.SetVisible(visIt->second);
        auto lckIt = snap->locked.find(n);
        if (lckIt != snap->locked.end()) layer.SetLocked(lckIt->second);
        auto frzIt = snap->frozen.find(n);
        if (frzIt != snap->frozen.end()) layer.SetFrozen(frzIt->second);
    }
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(layers);
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage(QString("Katman durumu yüklendi: %1").arg(chosen), 3000);
}

void MainWindow::OnLayerStateList() {
    if (m_layerStates.empty()) {
        statusBar()->showMessage("Kayıtlı katman durumu yok", 2000);
        return;
    }
    QStringList info;
    for (const auto& s : m_layerStates)
        info << QString("  • %1 (%2 katman)").arg(QString::fromStdString(s.name)).arg(s.visibility.size());
    QMessageBox::information(this, "Kayıtlı Katman Durumları",
        "LAYERSTATE anlık görüntüleri:\n" + info.join("\n"));
}

void MainWindow::OnCizimiGuncelle() {
    if (!m_document) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Cizimi Guncelle — Hesap Sonuclari");
    dlg.setMinimumWidth(340);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel("<b>Cizime yazilacak degerler:</b>"));

    auto* cbDN       = new QCheckBox("Boru capi (DN / mm)");         cbDN->setChecked(true);
    auto* cbLen      = new QCheckBox("Boru boyu (L)");               cbLen->setChecked(true);
    auto* cbSlope    = new QCheckBox("Egim i (%) — Pis Su boru");    cbSlope->setChecked(true);
    auto* cbFillRate = new QCheckBox("Doluluk h/d (%) — Pis Su");    cbFillRate->setChecked(true);
    lay->addWidget(cbDN);
    lay->addWidget(cbLen);
    lay->addWidget(cbSlope);
    lay->addWidget(cbFillRate);

    lay->addWidget(new QLabel(
        "<small><i>Cizimi Guncelle: Auto-Hydro yeniden kosturulur,<br>"
        "secilen degerler overlay etiketlerinde gosterilir.</i></small>"));

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    // Seçilen etiket flaglerini güncelle
    if (cbDN->isChecked())       m_labelShowDN     = true;
    if (cbLen->isChecked())      m_labelShowLength  = true;
    if (cbSlope->isChecked())    m_labelShowSlope   = true;
    if (cbFillRate->isChecked()) m_labelShowFillRate = true;

    // Hesabı yeniden çalıştır + overlay yenile
    ScheduleAutoHydro();
    statusBar()->showMessage("Cizim guncelleniyor — hesap yeniden kosturuluyor...", 3000);
}

// ============================================================
//  KAPALI CUKUR / FOSEPTIK HESABI (TS 822 / EN 12566-1)
// ============================================================
void MainWindow::OnFoseptik() {
    QDialog dlg(this);
    dlg.setWindowTitle("Kapali Cukur / Foseptik Hesabi — TS 822 / EN 12566-1");
    dlg.setMinimumWidth(440);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Foseptik Hacim Hesabi</b><br>"
        "<small>Standart: TS 822 + EN 12566-1</small>"));

    auto* form = new QFormLayout();

    auto* spKisi     = new QSpinBox();   spKisi->setRange(1, 10000);  spKisi->setValue(10);  spKisi->setSuffix(" kisi");
    auto* spGunluk   = new QDoubleSpinBox(); spGunluk->setRange(0.05, 10.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spBekleme  = new QSpinBox();   spBekleme->setRange(1, 90);   spBekleme->setValue(3); spBekleme->setSuffix(" gun (bekleme suresi)");
    auto* cbCift     = new QCheckBox("Cift odacikli tasarim (EN 12566-1 Madde 5.3)");

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk su tuketimi:", spGunluk);
    form->addRow("Bekleme suresi:", spBekleme);
    form->addRow("", cbCift);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0f8ff; border:1px solid #99b; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi    = spKisi->value();
        double gunluk  = spGunluk->value();
        int    bekleme = spBekleme->value();

        // TS 822: V_total = kisi × gunluk × bekleme (min 1.5 m³)
        double V_brut = kisi * gunluk * bekleme;
        double V_net  = std::max(V_brut, 1.5);

        // Çamur hacmi — EN 12566-1: ek %30
        double V_camur = V_net * 0.30;
        double V_toplam = V_net + V_camur;

        QString html;
        html += QString("<b>Hesap Sonuclari (TS 822 / EN 12566-1)</b><br>");
        html += QString("Kisi sayisi : %1<br>").arg(kisi);
        html += QString("Gunluk debi : %1 m³/kisi/gun<br>").arg(gunluk, 0, 'f', 3);
        html += QString("Bekleme     : %1 gun<br><br>").arg(bekleme);
        html += QString("<b>Bekleme hacmi (V_net) : %1 m³</b><br>").arg(V_net, 0, 'f', 2);
        html += QString("Camur hacmi (+30%%)    : %1 m³<br>").arg(V_camur, 0, 'f', 2);
        html += QString("<b style='color:red'>TOPLAM HACIM          : %1 m³</b><br>").arg(V_toplam, 0, 'f', 2);

        if (cbCift->isChecked()) {
            double V1 = V_toplam * 0.67;
            double V2 = V_toplam * 0.33;
            html += QString("<br>Cift odacikli: 1. odacik %1 m³ / 2. odacik %2 m³<br>")
                        .arg(V1, 0, 'f', 2).arg(V2, 0, 'f', 2);
        }

        // Tahliye frekansı
        double gunler = (V_toplam / (kisi * gunluk));
        html += QString("<br>Tahliye sikligI : her ~%1 gunde bir").arg((int)gunler);

        lblResult->setText(html);
    };

    connect(spKisi,    QOverload<int>::of(&QSpinBox::valueChanged),    [&](int){ calcResult(); });
    connect(spGunluk,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spBekleme, QOverload<int>::of(&QSpinBox::valueChanged),    [&](int){ calcResult(); });
    connect(cbCift,    &QCheckBox::toggled,                            [&](bool){ calcResult(); });

    calcResult(); // İlk hesap

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);

    dlg.exec();
}

// ============================================================
//  PIS SU HESAP FOYU — drenaj devresi detay tablosu
// ============================================================
void MainWindow::OnPisSuHesapFoyu() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // Önce drenaj hesabını çalıştır
    mep::HydraulicSolver solver(network);
    solver.Solve();

    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Hesap Foyu — EN 12056-2");
    dlg.resize(820, 500);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel("<b>Pis Su / Atik Su Devresi Hesap Foyu</b> &nbsp; <small>EN 12056-2 Manning</small>"));

    auto* tbl = new QTableWidget(0, 9, &dlg);
    tbl->setHorizontalHeaderLabels({
        "Boru No", "Boru Cinsi", "Malzeme", "DN (mm)", "L (m)",
        "DU", "Q (L/s)", "Egim i (%)", "Doluluk h/d (%)"
    });
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setAlternatingRowColors(true);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);

    int row = 0;
    for (auto& [id, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Drainage) continue;
        tbl->insertRow(row);

        double Q_Ls = edge.flowRate_m3s * 1000.0;
        double slopePct = edge.slope * 100.0;
        double fillPct  = edge.fillRate * 100.0;
        bool isCol = network.IsColumnEdge(edge.id);

        tbl->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(edge.label.empty() ? ("B-" + std::to_string(edge.id)) : edge.label)));
        tbl->setItem(row, 1, new QTableWidgetItem(isCol ? "Kolon" : "Yatay"));
        tbl->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(edge.material)));
        tbl->setItem(row, 3, new QTableWidgetItem(QString::number(edge.diameter_mm, 'f', 0)));
        tbl->setItem(row, 4, new QTableWidgetItem(QString::number(edge.length_m, 'f', 2)));
        tbl->setItem(row, 5, new QTableWidgetItem(QString::number(edge.cumulativeDU, 'f', 1)));
        tbl->setItem(row, 6, new QTableWidgetItem(QString::number(Q_Ls, 'f', 3)));
        tbl->setItem(row, 7, new QTableWidgetItem(QString::number(slopePct, 'f', 1)));

        auto* fillItem = new QTableWidgetItem(QString::number(fillPct, 'f', 0) + "%");
        if (edge.fillRate > 0.50)  // EN 12056: max %50
            fillItem->setBackground(QColor(255, 180, 180));
        else if (edge.fillRate > 0.40)
            fillItem->setBackground(QColor(255, 240, 180));
        tbl->setItem(row, 8, fillItem);

        ++row;
    }

    tbl->resizeColumnsToContents();
    lay->addWidget(tbl, 1);

    lay->addWidget(new QLabel(
        "<small>Kirmizi = EN 12056 siniri asilmis (h/d > 50%). "
        "Sari = uyari bolgesi (h/d 40-50%).</small>"));

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    dlg.exec();
}

// ============================================================
//  DXF EXPORT — tam proje (tüm katlar + MEP şebekesi)
// ============================================================
void MainWindow::OnExportDXF() {
    if (!m_document) return;
    if (!RunPreflight()) return;

    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder()) : "";

    QString path = QFileDialog::getSaveFileName(this,
        "Tam Proje — DXF Olarak Kaydet", startDir,
        "DXF Dosyasi (*.dxf)");
    if (path.isEmpty()) return;

    cad::DXFWriter writer;
    QString projName = QString::fromStdString(
        pm.HasActiveProject() ? pm.GetProjectName() : "VKT Projesi");

    bool ok = writer.Write(path.toStdString(),
                           m_document->GetCADEntities(),
                           m_document->GetNetwork(),
                           projName.toStdString(),
                           &m_document->GetBlockRegistry());

    if (ok) {
        auto blockCount = m_document->GetBlockRegistry().Size();
        QString msg = QString("DXF kaydedildi: %1").arg(path);
        if (blockCount > 0)
            msg += QString(" (%1 blok tanimi dahil)").arg(blockCount);
        statusBar()->showMessage(msg, 4000);
        if (m_logList)
            m_logList->addItem(QString("DXF Export: %1").arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::critical(this, "DXF Export Hatasi", "Dosya yazılamadi!");
    }
}

// ============================================================
//  DXF EXPORT — aktif kat filtreli (FineSANI "ekran çizimi" eşdeğeri)
// ============================================================
void MainWindow::OnExportFloorDXF() {
    if (!m_document) return;

    const auto& floors = m_floorManager.GetFloors();
    if (floors.empty()) {
        QMessageBox::information(this, "Kat DXF Export",
            "Once Mimari Belirle (Ctrl+M) ile kat tanimlayin.\n"
            "Kat tanimsizsa tam proje DXF icin: Dosya → Tam Proje DXF");
        return;
    }

    // Aktif kat seçici
    QStringList katlar;
    for (const auto& f : floors)
        katlar << QString("[%1] %2 (kot=%3m)")
                     .arg(f.index).arg(QString::fromStdString(f.label))
                     .arg(f.elevation_m, 0, 'f', 2);

    bool ok2;
    QString secilen = QInputDialog::getItem(this, "Kat Secimi",
        "DXF'e aktarilacak kati secin:", katlar, m_activeFloorIndex, false, &ok2);
    if (!ok2 || secilen.isEmpty()) return;

    int katIdx = katlar.indexOf(secilen);
    if (katIdx < 0 || katIdx >= (int)floors.size()) return;
    const core::Floor& kat = floors[katIdx];

    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder()) : "";

    // Varsayılan dosya adı: proje + kat etiketi
    QString defaultName = QString::fromStdString(
        pm.HasActiveProject() ? pm.GetProjectName() : "proje");
    defaultName += "_" + QString::fromStdString(kat.label)
                             .replace(" ", "_").replace(".", "");

    QString path = QFileDialog::getSaveFileName(this,
        "Kat DXF Kaydet — " + QString::fromStdString(kat.label),
        startDir + "/" + defaultName + ".dxf",
        "DXF Dosyasi (*.dxf)");
    if (path.isEmpty()) return;

    // Kat Z aralığı: elevation_m ± tolerans (kat yüksekliğinin yarısı)
    double zLo = kat.elevation_m - 0.3;
    double zHi = kat.elevation_m + kat.height_m + 0.3;

    // MEP: aktif kata ait node + edge'leri filtrele
    mep::NetworkGraph floorNet;
    std::unordered_map<uint32_t, uint32_t> nodeRemap; // eski id → yeni id

    for (const auto& [nid, node] : m_document->GetNetwork().GetNodeMap()) {
        if (node.position.z >= zLo && node.position.z <= zHi) {
            mep::Node copy = node;
            uint32_t newId = floorNet.AddNode(copy);
            nodeRemap[nid] = newId;
        }
    }
    for (const auto& [eid, edge] : m_document->GetNetwork().GetEdgeMap()) {
        auto itA = nodeRemap.find(edge.nodeA);
        auto itB = nodeRemap.find(edge.nodeB);
        if (itA == nodeRemap.end() || itB == nodeRemap.end()) continue;
        mep::Edge copy = edge;
        copy.nodeA = itA->second;
        copy.nodeB = itB->second;
        floorNet.AddEdge(copy);
    }

    // CAD arka plan: Z aralığında olan entity'leri seç
    // (import sırasında Z offset uygulandığı için kat koordinatları biliniyor)
    std::vector<std::unique_ptr<cad::Entity>> floorEntities;
    for (const auto& e : m_document->GetCADEntities()) {
        if (!e) continue;
        auto bb = e->GetBounds();
        double ez = (bb.min.z + bb.max.z) / 2.0;
        if (ez >= zLo && ez <= zHi)
            floorEntities.push_back(e->Clone());
    }

    cad::DXFWriter writer;
    bool ok3 = writer.Write(path.toStdString(),
                            floorEntities,
                            floorNet,
                            kat.label,
                            &m_document->GetBlockRegistry());
    if (ok3) {
        statusBar()->showMessage(
            QString("Kat DXF kaydedildi: %1 (%2 entity, %3 boru)")
                .arg(QFileInfo(path).fileName())
                .arg((int)floorEntities.size())
                .arg((int)floorNet.GetEdgeCount()), 4000);
        if (m_logList)
            m_logList->addItem(QString("Kat DXF: %1 → %2")
                .arg(QString::fromStdString(kat.label))
                .arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::critical(this, "Hata", "DXF dosyasi yazılamadi!");
    }
}

// ============================================================
//  EMDİRME ÇUKURU HESABI (toprak emme kapasitesi)
// ============================================================
void MainWindow::OnEmdirmeCukuru() {
    QDialog dlg(this);
    dlg.setWindowTitle("Emdirme Cukuru Hesabi — Toprak Emme Kapasitesi");
    dlg.setMinimumWidth(460);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Emdirme Cukuru Boyutlandirma</b><br>"
        "<small>TS 822 Ek-B — Perkolasyon testine gore toprak emme kapasitesi</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();   spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spGunluk  = new QDoubleSpinBox(); spGunluk->setRange(0.05, 5.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spPerko   = new QDoubleSpinBox(); spPerko->setRange(1, 120); spPerko->setValue(10); spPerko->setSuffix(" dk/cm (perkolasyon testi)");
    auto* cmbToprak = new QComboBox();
    cmbToprak->addItems({"Kum-cakil (20 L/m²/gun)", "Kumlu-kil (15 L/m²/gun)",
                         "Killi (10 L/m²/gun)", "Agir kil (5 L/m²/gun)"});

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk atiksu:", spGunluk);
    form->addRow("Toprak tipi:", cmbToprak);
    form->addRow("Perkolasyon suresi:", spPerko);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#fff8e7; border:1px solid #cc9; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi   = spKisi->value();
        double gun    = spGunluk->value();
        double perko  = spPerko->value();
        int    idx    = cmbToprak->currentIndex();
        static const double caps[] = {20.0, 15.0, 10.0, 5.0}; // L/m²/gün
        double cap_m3 = caps[idx] / 1000.0; // m³/m²/gün

        double Q_gun   = kisi * gun; // m³/gün toplam atıksu
        double A_gerek = Q_gun / cap_m3; // gerekli yüzey alanı (m²)

        // Perkolasyon testine göre düzeltme (perko > 30 dk/cm → azalt)
        double duzeltme = 1.0;
        if (perko > 30) duzeltme = 1.5;
        else if (perko > 15) duzeltme = 1.25;
        A_gerek *= duzeltme;

        // Derinlik 1.5m varsayım → hacim
        double V_gerek = A_gerek * 1.5;

        QString html;
        html += QString("<b>Emdirme Cukuru Hesap Sonuclari</b><br>");
        html += QString("Toplam atiksu : %1 m³/gun<br>").arg(Q_gun, 0, 'f', 3);
        html += QString("Emme kap.     : %1 L/m²/gun<br>").arg(caps[idx], 0, 'f', 0);
        html += QString("Duzeltme fak. : x%1<br><br>").arg(duzeltme, 0, 'f', 2);
        html += QString("<b>Gerekli alan  : %1 m²</b><br>").arg(A_gerek, 0, 'f', 1);
        html += QString("<b>Gerekli hacim : %1 m³</b> (derinlik 1.5m)<br>").arg(V_gerek, 0, 'f', 2);
        html += QString("<br><i>Not: Perkolasyon testi sahada yapilarak dogrulanmalidir.</i>");
        lblResult->setText(html);
    };

    connect(spKisi,   QOverload<int>::of(&QSpinBox::valueChanged),         [&](int){ calcResult(); });
    connect(spGunluk, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spPerko,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(cmbToprak, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  PİS SU ÇUKURU HESABI (geçirimsiz depolama tankı)
// ============================================================
void MainWindow::OnPisSuCukuru() {
    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Cukuru Hesabi — Gecirimsiz Depolama Tanki");
    dlg.setMinimumWidth(460);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Pis Su Cukuru (Gecirimsiz Depolama)</b><br>"
        "<small>Kanalizasyona baglanamadiginda gecici depolama; tanker ile tahliye edilir.<br>"
        "TS 822 Md. 5 — foseptikten farki: aritma yapilmaz, sizmaz tank.</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();   spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spGunluk  = new QDoubleSpinBox(); spGunluk->setRange(0.05, 5.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spAralik  = new QSpinBox();   spAralik->setRange(1, 90); spAralik->setValue(14); spAralik->setSuffix(" gun (tahliye araligi)");
    auto* spEmniyet = new QDoubleSpinBox(); spEmniyet->setRange(1.0, 2.0); spEmniyet->setValue(1.25); spEmniyet->setSingleStep(0.05); spEmniyet->setDecimals(2);

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk su tuketimi:", spGunluk);
    form->addRow("Tanker tahliye araligi:", spAralik);
    form->addRow("Emniyet katsayisi:", spEmniyet);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0fff0; border:1px solid #9b9; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi    = spKisi->value();
        double gun     = spGunluk->value();
        int    aralik  = spAralik->value();
        double emniyet = spEmniyet->value();

        double V_net   = kisi * gun * aralik;      // m³ — net depolama hacmi
        double V_toplam = V_net * emniyet;          // emniyet pay dahil
        double V_min   = std::max(V_toplam, 3.0);  // TS 822: minimum 3 m³

        // Tavsiye: silindir tank D=2m → yükseklik
        double h_sil = V_min / (3.14159 * 1.0 * 1.0); // D=2m → r=1m

        QString html;
        html += QString("<b>Pis Su Cukuru Hesap Sonuclari</b><br>");
        html += QString("Gunluk atiksu : %1 m³<br>").arg(kisi * gun, 0, 'f', 3);
        html += QString("Tahliye araligi: %1 gun<br>").arg(aralik);
        html += QString("Net hacim     : %1 m³<br>").arg(V_net, 0, 'f', 2);
        html += QString("Emniyet paylI : x%1<br>").arg(emniyet, 0, 'f', 2);
        html += QString("<b style='color:red'>HESAP HACMi   : %1 m³</b><br>").arg(V_min, 0, 'f', 2);
        html += QString("<br>Silindir tank (D=2m): yukseklik ≈ %1 m").arg(h_sil, 0, 'f', 2);
        lblResult->setText(html);
    };

    connect(spKisi,    QOverload<int>::of(&QSpinBox::valueChanged),        [&](int){ calcResult(); });
    connect(spGunluk,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),[&](double){ calcResult(); });
    connect(spAralik,  QOverload<int>::of(&QSpinBox::valueChanged),        [&](int){ calcResult(); });
    connect(spEmniyet, QOverload<double>::of(&QDoubleSpinBox::valueChanged),[&](double){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  PİS SU POMPASI BOYUTLANDIRMA
// ============================================================
void MainWindow::OnPisSuPompasi() {
    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Pompasi Boyutlandirma");
    dlg.setMinimumWidth(480);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Pis Su Lift Istasyonu / Pompa Boyutlandirma</b><br>"
        "<small>DIN EN 12050-1 — fosseptik/cukur tahliye pompasi debi + manometrik yukseklik + guc</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();       spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spStatik  = new QDoubleSpinBox(); spStatik->setRange(0, 50); spStatik->setValue(5.0); spStatik->setSuffix(" m (statik yukseklik)");
    auto* spBoru_m  = new QDoubleSpinBox(); spBoru_m->setRange(1, 500); spBoru_m->setValue(30); spBoru_m->setSuffix(" m (boru boyu)");
    auto* spDN      = new QComboBox();
    spDN->addItems({"DN40", "DN50", "DN65", "DN80", "DN100"});
    spDN->setCurrentIndex(1); // DN50
    auto* spEta     = new QDoubleSpinBox(); spEta->setRange(0.3, 0.95); spEta->setValue(0.70); spEta->setDecimals(2); spEta->setSuffix(" (pompa verimi)");

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Statik yukseklik:", spStatik);
    form->addRow("Boru boyu:", spBoru_m);
    form->addRow("Boru capi:", spDN);
    form->addRow("Pompa verimi η:", spEta);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0f0ff; border:1px solid #99b; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi   = spKisi->value();
        double H_stat = spStatik->value();
        double L_boru = spBoru_m->value();
        double eta    = spEta->value();

        // Debi: EN 12050-1 — 0.15 L/s per kişi pikde
        double Q_Ls  = kisi * 0.15; // L/s
        double Q_m3h = Q_Ls * 3.6;  // m³/h

        // Boru çapı (mm)
        static const double dns[] = {40, 50, 65, 80, 100};
        double D_mm = dns[spDN->currentIndex()];
        double D_m  = D_mm / 1000.0;
        double A    = 3.14159 * D_m * D_m / 4.0;
        double v    = (Q_Ls / 1000.0) / A; // m/s

        // Darcy-Weisbach tahmini kayıp: f=0.025, pis su λ≈0.03
        double f    = 0.03;
        double hf   = f * (L_boru / D_m) * (v * v) / (2 * 9.81);

        // Manometrik yükseklik
        double H_man = H_stat + hf * 1.15; // %15 lokal kayıp payı

        // Güç: P = ρ × g × Q × H / η
        double P_kW = (1000.0 * 9.81 * (Q_Ls / 1000.0) * H_man) / (eta * 1000.0);

        // Standart motor seçimi (küçük üstü)
        double P_motor = 0.37;
        for (double s : {0.37, 0.55, 0.75, 1.1, 1.5, 2.2, 3.0, 4.0, 5.5, 7.5}) {
            if (s >= P_kW) { P_motor = s; break; }
        }

        QString html;
        html += QString("<b>Pompa Hesap Sonuclari</b><br>");
        html += QString("Debi Q      : %1 L/s = %2 m³/h<br>").arg(Q_Ls, 0, 'f', 2).arg(Q_m3h, 0, 'f', 2);
        html += QString("Hiz v       : %1 m/s<br>").arg(v, 0, 'f', 2);
        html += QString("Boru kaybi  : %1 m<br>").arg(hf, 0, 'f', 2);
        html += QString("<b>Manometrik H: %1 m</b><br>").arg(H_man, 0, 'f', 2);
        html += QString("Hesap gucu  : %1 kW<br>").arg(P_kW, 0, 'f', 3);
        html += QString("<b style='color:blue'>Secilen motor: %1 kW</b>").arg(P_motor, 0, 'f', 2);
        lblResult->setText(html);
    };

    connect(spKisi,   QOverload<int>::of(&QSpinBox::valueChanged),         [&](int){ calcResult(); });
    connect(spStatik, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spBoru_m, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spDN,     QOverload<int>::of(&QComboBox::currentIndexChanged),  [&](int){ calcResult(); });
    connect(spEta,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  MEMBRANLΙ GENLEŞİM TANKI BOYUTLANDIRMA (EN 12828)
// ============================================================
void MainWindow::OnGenlesimTanki() {
    QDialog dlg(this);
    dlg.setWindowTitle("Membranlı Genleşme Tankı — EN 12828");
    dlg.resize(480, 440);
    auto* lay  = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();

    // Sistem hacmini boru ağından otomatik hesapla
    double autoVol = 0.0;
    if (m_document) {
        for (const auto& [id, edge] : m_document->GetNetwork().GetEdgeMap()) {
            if (edge.type == mep::EdgeType::HotWater || edge.type == mep::EdgeType::Supply) {
                double D_m = edge.diameter_mm / 1000.0;
                double A   = 3.14159265 * (D_m / 2.0) * (D_m / 2.0);
                autoVol   += A * edge.length_m * 1000.0; // m³ → L
            }
        }
    }

    auto* spVsys   = new QDoubleSpinBox(); spVsys->setRange(1, 50000); spVsys->setDecimals(1); spVsys->setSuffix(" L"); spVsys->setValue(std::max(autoVol, 10.0));
    auto* spTcold  = new QDoubleSpinBox(); spTcold->setRange(0, 30);   spTcold->setDecimals(0); spTcold->setSuffix(" °C"); spTcold->setValue(10.0);
    auto* spThot   = new QDoubleSpinBox(); spThot->setRange(40, 100);  spThot->setDecimals(0); spThot->setSuffix(" °C"); spThot->setValue(60.0);
    auto* spHstat  = new QDoubleSpinBox(); spHstat->setRange(0, 100);  spHstat->setDecimals(1); spHstat->setSuffix(" m (statik yük)"); spHstat->setValue(10.0);
    auto* spPsafety= new QDoubleSpinBox(); spPsafety->setRange(2, 10); spPsafety->setDecimals(1); spPsafety->setSuffix(" bar (emniyet ventili)"); spPsafety->setValue(3.0);

    if (autoVol > 0.1)
        spVsys->setToolTip(QString("Ağdan otomatik hesap: %1 L").arg(autoVol, 0, 'f', 2));

    form->addRow("Sistem hacmi V_sys:", spVsys);
    form->addRow("Dolum sıcaklığı T_cold:", spTcold);
    form->addRow("Maks. çalışma sıcaklığı T_hot:", spThot);
    form->addRow("Statik yük H:", spHstat);
    form->addRow("Emniyet ventili basıncı P_S:", spPsafety);
    lay->addLayout(form);

    auto* lblRes = new QLabel();
    lblRes->setWordWrap(true);
    lblRes->setStyleSheet("QLabel{background:#f0fff4;border:1px solid #8c8;padding:8px;font-family:monospace;}");
    lay->addWidget(lblRes);

    // EN 12828 su genleşme katsayısı tablosu (10°C referans)
    auto expansionCoeff = [](double Tcold, double Thot) -> double {
        // Doğrusal interpolasyon: ρ_water(T) ≈ 1/(1 + e_v(T)) baz alındı
        // e_v: genleşme oranı (10°C → T_hot)
        static const double T[] = {10, 20, 40, 60, 70, 80, 90};
        static const double e[] = {0.0, 0.0002, 0.0078, 0.0170, 0.0225, 0.0286, 0.0356};
        constexpr int N = 7;
        // ΔT etkin: dolum sıcaklığını referans al
        double eHot = 0.0, eCold = 0.0;
        for (int i = 1; i < N; ++i) {
            if (Thot <= T[i]) {
                double f = (Thot - T[i-1]) / (T[i] - T[i-1]);
                eHot = e[i-1] + f * (e[i] - e[i-1]);
                break;
            } else if (i == N-1) eHot = e[N-1];
        }
        for (int i = 1; i < N; ++i) {
            if (Tcold <= T[i]) {
                double f = (Tcold - T[i-1]) / (T[i] - T[i-1]);
                eCold = e[i-1] + f * (e[i] - e[i-1]);
                break;
            }
        }
        return std::max(eHot - eCold, 0.001);
    };

    auto calcTank = [&]() {
        double V_sys    = spVsys->value();
        double T_cold   = spTcold->value();
        double T_hot    = spThot->value();
        double H_stat   = spHstat->value();
        double P_safety = spPsafety->value();

        double e_v  = expansionCoeff(T_cold, T_hot);
        // Basınçlar (bar mano.)
        double P_min = H_stat / 10.0 + 0.3; // dolum basıncı = statik + 0.3 bar ön şarj toleransı
        double P_max = P_safety - 0.5;        // güvenlik marjı
        if (P_max <= P_min) P_max = P_min + 0.5;
        // EN 12828: V_tank = V_sys × e_v × (P_max + 1) / (P_max - P_min)   [bar mutlak]
        double V_tank = V_sys * e_v * (P_max + 1.0) / (P_max - P_min);
        double V_nominal = V_tank * 1.25; // %25 emniyet

        // Standart tank boyutları (L)
        static const double std_sizes[] = {2,4,8,12,18,25,35,50,80,100,150,200,300,500};
        double sel = 500;
        for (double s : std_sizes) { if (s >= V_nominal) { sel = s; break; } }

        QString html;
        html += QString("<b>Genleşme Katsayısı e_v:</b> %1 (%2 → %3°C)<br>").arg(e_v, 0, 'f', 4).arg(T_cold, 0, 'f', 0).arg(T_hot, 0, 'f', 0);
        html += QString("<b>P_min (dolum):</b> %1 bar — <b>P_max (çalışma):</b> %2 bar<br>").arg(P_min, 0, 'f', 2).arg(P_max, 0, 'f', 2);
        html += QString("<b>Hesap hacmi V_n:</b> %1 L<br>").arg(V_tank, 0, 'f', 1);
        html += QString("<b>%25 marjlı nominál:</b> %1 L<br>").arg(V_nominal, 0, 'f', 1);
        html += QString("<b style='color:#006600'>Seçilen standart tank: <big>%1 L</big></b><br>").arg(sel, 0, 'f', 0);
        html += QString("Ön şarj basıncı: %1 bar").arg(P_min, 0, 'f', 2);
        lblRes->setText(html);
    };

    connect(spVsys,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spTcold,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spThot,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spHstat,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spPsafety, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    calcTank();

    lay->addWidget(new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, &dlg));
    connect(lay->itemAt(lay->count()-1)->widget(), SIGNAL(accepted()), &dlg, SLOT(accept()));
    dlg.exec();

    if (m_logList)
        m_logList->addItem("Genleşme tankı hesabı yapıldı (EN 12828).");
}

// ============================================================
//  YAĞMUR DÜŞME ALANI — çizimden poligon seçimi
// ============================================================
void MainWindow::OnYagmurAlani() {
    m_polyAreaPoints.clear();
    m_currentToolMode = ToolMode::DrawPolyArea;
    m_drawState       = DrawState::WaitingFirstPoint;
    if (m_snapOverlay) m_snapOverlay->show();
    statusBar()->showMessage("Yağmur alanı: Köşe noktalarını tıklayın — Enter ile kapat, ESC iptal");
    if (m_commandBar) m_commandBar->SetPrompt("Köşe noktası");
}

void MainWindow::FinishPolyArea() {
    if (m_polyAreaPoints.size() < 3) {
        statusBar()->showMessage("En az 3 nokta gerekli.");
        return;
    }
    // Shoelace alanı (mm² → m²)
    double area_mm2 = 0.0;
    int n = static_cast<int>(m_polyAreaPoints.size());
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area_mm2 += m_polyAreaPoints[i].x * m_polyAreaPoints[j].y;
        area_mm2 -= m_polyAreaPoints[j].x * m_polyAreaPoints[i].y;
    }
    double area_m2 = std::abs(area_mm2) / 2.0 / 1e6; // mm² → m²

    m_polyAreaPoints.clear();
    m_currentToolMode = ToolMode::Select;
    m_drawState = DrawState::Idle;
    if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }

    // Yağmur hesabı — yüzey tipi seçimi
    QStringList surfaceTypes;
    surfaceTypes << "Çatı / Çatı kaplaması (C=1.0)"
                 << "Beton / Asfalt zemin (C=0.9)"
                 << "Yeşil çatı (C=0.5)"
                 << "Çakıllı geçirimli zemin (C=0.6)";
    bool ok = false;
    QString surface = QInputDialog::getItem(this,
        "Yüzey Tipi", QString("Ölçülen alan: <b>%1 m²</b><br>Yüzey tipi:").arg(area_m2, 0, 'f', 2),
        surfaceTypes, 0, false, &ok);
    if (!ok) return;

    double C = 1.0;
    if (surface.contains("Beton")) C = 0.9;
    else if (surface.contains("Yeşil")) C = 0.5;
    else if (surface.contains("akıllı") || surface.contains("Çakıl")) C = 0.6;

    double rD = 0.03;
    double Q_ls = C * area_m2 * rD;

    static const struct { double dn; double cap_ls; } kTable[] = {
        {50,1.2},{75,3.5},{100,8.0},{125,15.0},{150,25.0},{200,50.0},{0,0}
    };
    double dnPerPipe = 200.0;
    double numPipes = 1;
    if (Q_ls > 50.0) numPipes = std::ceil(Q_ls / 50.0);
    double qPer = Q_ls / numPipes;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= qPer) { dnPerPipe = kTable[i].dn; break; }
    }

    QString msg;
    msg += QString("<h3>Yağmur Suyu Tahliye — Çizimden Ölçüm (EN 12056-3)</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td>Ölçülen alan:</td><td><b>%1 m²</b></td></tr>").arg(area_m2, 0, 'f', 2);
    msg += QString("<tr><td>Yüzey katsayısı (C):</td><td>%1</td></tr>").arg(C, 0, 'f', 2);
    msg += QString("<tr><td>r_D:</td><td>0.030 l/(s·m²)</td></tr>");
    msg += QString("<tr><td><b>Tasarım debisi Q:</b></td><td><font color='red'><b>%1 l/s</b></font></td></tr>").arg(Q_ls, 0, 'f', 2);
    msg += QString("<tr><td>Önerilen boru:</td><td><b>%1× DN%2</b></td></tr>").arg((int)numPipes).arg((int)dnPerPipe);
    msg += QString("</table>");

    QMessageBox dlg2(this);
    dlg2.setWindowTitle("Yağmur Suyu — Poligon Alanı");
    dlg2.setTextFormat(Qt::RichText);
    dlg2.setText(msg);
    dlg2.setIcon(QMessageBox::Information);
    dlg2.exec();

    if (m_logList)
        m_logList->addItem(QString("Yağmur (poligon): A=%1m², Q=%2l/s → %3×DN%4")
            .arg(area_m2,0,'f',1).arg(Q_ls,0,'f',2).arg((int)numPipes).arg((int)dnPerPipe));
    statusBar()->showMessage(QString("Yağmur alanı: %1 m² → Q=%2 l/s")
        .arg(area_m2,0,'f',2).arg(Q_ls,0,'f',2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Norm Karşılaştırma — EN 806-3 ile DIN 1988-300 yan yana
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnNormKarsilastirma() {
    auto* doc = m_document;
    if (!doc) { QMessageBox::information(this, "Norm Karşılaştırma", "Açık proje yok."); return; }
    auto& network = doc->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Norm Karşılaştırma", "Ağda boru bulunamadı.");
        return;
    }

    // EN 806-3 hesabı
    mep::HydroNorm savedNorm = mep::HydraulicSolver::GlobalNorm();
    mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;
    mep::HydraulicSolver solverEN(network);
    solverEN.SetNorm(mep::HydroNorm::EN806_3);
    solverEN.Solve();

    struct EdgeResult {
        uint32_t id;
        double dn_en, q_nom_en, q_hes_en, v_en, dh_en;
        double dn_din, q_nom_din, q_hes_din, v_din, dh_din;
    };
    std::vector<EdgeResult> rows;
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
        EdgeResult r;
        r.id      = eid;
        r.dn_en   = edge.diameter_mm;
        r.q_nom_en = edge.nominalFlow_Ls;
        r.q_hes_en = edge.flowRate_m3s * 1000.0;
        r.v_en    = edge.velocity_ms;
        r.dh_en   = edge.headLoss_m;
        rows.push_back(r);
    }

    // DIN 1988-300 hesabı — aynı ağ üzerinde (field'ları geçici yazar)
    mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
    mep::HydraulicSolver solverDIN(network);
    solverDIN.SetNorm(mep::HydroNorm::DIN1988);
    solverDIN.Solve();

    for (auto& r : rows) {
        auto it = network.GetEdgeMap().find(r.id);
        if (it == network.GetEdgeMap().end()) continue;
        const auto& e = it->second;
        r.dn_din   = e.diameter_mm;
        r.q_nom_din = e.nominalFlow_Ls;
        r.q_hes_din = e.flowRate_m3s * 1000.0;
        r.v_din    = e.velocity_ms;
        r.dh_din   = e.headLoss_m;
    }

    // Aktif normu geri yükle
    mep::HydraulicSolver::GlobalNorm() = savedNorm;

    // HTML tablo oluştur
    QString html;
    html += "<h3>Norm Karşılaştırma — EN 806-3 vs DIN 1988-300</h3>";
    html += "<table border='1' cellspacing='0' cellpadding='4'>";
    html += "<tr style='background:#ddeeff'>"
            "<th>Boru ID</th>"
            "<th colspan='4'>EN 806-3</th>"
            "<th colspan='4'>DIN 1988-300</th>"
            "<th>DN Fark</th></tr>";
    html += "<tr style='background:#eef4ff'>"
            "<th></th>"
            "<th>DN</th><th>Q_nom (l/s)</th><th>Q_hes (l/s)</th><th>v (m/s)</th><th>ΔH (m)</th>"
            "<th>DN</th><th>Q_nom (l/s)</th><th>Q_hes (l/s)</th><th>v (m/s)</th><th>ΔH (m)</th>"
            "<th></th></tr>";

    int upCount = 0, downCount = 0, sameCount = 0;
    for (const auto& r : rows) {
        bool bigger  = r.dn_din > r.dn_en;
        bool smaller = r.dn_din < r.dn_en;
        if (bigger) ++upCount; else if (smaller) ++downCount; else ++sameCount;
        QString rowColor = bigger ? "#fff8e1" : (smaller ? "#e8f5e9" : "white");
        QString diffStr  = bigger ? QString("+%1mm").arg(r.dn_din - r.dn_en, 0, 'f', 0)
                         : smaller ? QString("-%1mm").arg(r.dn_en - r.dn_din, 0, 'f', 0)
                         : "=";
        html += QString("<tr style='background:%1'>"
                        "<td>%2</td>"
                        "<td>DN%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td>"
                        "<td>DN%8</td><td>%9</td><td>%10</td><td>%11</td><td>%12</td>"
                        "<td><b>%13</b></td></tr>")
            .arg(rowColor)
            .arg(r.id)
            .arg(r.dn_en, 0, 'f', 0)
            .arg(r.q_nom_en, 0, 'f', 3)
            .arg(r.q_hes_en, 0, 'f', 3)
            .arg(r.v_en, 0, 'f', 2)
            .arg(r.dh_en, 0, 'f', 3)
            .arg(r.dn_din, 0, 'f', 0)
            .arg(r.q_nom_din, 0, 'f', 3)
            .arg(r.q_hes_din, 0, 'f', 3)
            .arg(r.v_din, 0, 'f', 2)
            .arg(r.dh_din, 0, 'f', 3)
            .arg(diffStr);
    }
    html += "</table>";
    html += QString("<br><b>Özet:</b> DIN vs EN — Büyük çap: <font color='orange'>%1</font> boru | "
                    "Küçük çap: <font color='green'>%2</font> boru | "
                    "Aynı: %3 boru").arg(upCount).arg(downCount).arg(sameCount);
    html += "<br><small>Sarı satır: DIN daha büyük DN seçti | Yeşil satır: DIN daha küçük DN seçti</small>";

    QDialog dlg(this);
    dlg.setWindowTitle("Norm Karşılaştırma — EN 806-3 vs DIN 1988-300");
    dlg.resize(960, 560);
    auto* lay = new QVBoxLayout(&dlg);
    auto* tb  = new QTextBrowser(&dlg);
    tb->setHtml(html);
    lay->addWidget(tb);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(btn);
    dlg.exec();

    if (m_logList)
        m_logList->addItem(QString("Norm karş.: %1 boru — DIN büyük:%2 küçük:%3 aynı:%4")
            .arg(rows.size()).arg(upCount).arg(downCount).arg(sameCount));
}

// ─────────────────────────────────────────────────────────────────────────────
// Hesap Kararı — "Neden bu çap?" per-pipe gerekçe
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnHesapKarari() {
    auto* doc = m_document;
    if (!doc) { QMessageBox::information(this, "Hesap Kararı", "Açık proje yok."); return; }
    auto& network = doc->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Hesap Kararı", "Ağda boru bulunamadı.");
        return;
    }

    // Güncel hesabı çalıştır
    mep::HydraulicSolver solver(network);
    solver.Solve();

    // DN serisi (mm)
    static const double kDNSeries[] = {10,15,20,25,32,40,50,65,80,100,125,150,200,0};
    auto nextDN = [](double dn) -> double {
        for (int i = 0; kDNSeries[i] > 0; ++i)
            if (kDNSeries[i] > dn) return kDNSeries[i];
        return dn;
    };
    auto prevDN = [](double dn) -> double {
        double prev = 0;
        for (int i = 0; kDNSeries[i] > 0; ++i) {
            if (kDNSeries[i] >= dn) return (prev > 0 ? prev : dn);
            prev = kDNSeries[i];
        }
        return dn;
    };

    QDialog dlg(this);
    dlg.setWindowTitle("Hesap Kararı — Neden Bu Çap?");
    dlg.resize(820, 480);
    auto* lay = new QVBoxLayout(&dlg);
    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({
        "Boru ID", "DN (mm)", "Q (l/s)", "v (m/s)",
        "DN-1 → v", "DN+1 → v", "Gerekçe"
    });
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    const double V_MAX = 3.0;  // EN 806-3 max hız m/s
    const double V_MIN = 0.5;  // minimum akış hızı m/s

    int row = 0;
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
        table->insertRow(row);

        double q_m3s = edge.flowRate_m3s;
        double dn    = edge.diameter_mm;
        double v_cur = edge.velocity_ms;

        // Hız hesabı için yardımcı lambda
        auto velForDN = [&](double d_mm) -> double {
            if (d_mm <= 0) return 0;
            double r = d_mm / 2000.0;
            double area = 3.14159265 * r * r;
            return area > 0 ? q_m3s / area : 0;
        };

        double dn_prev = prevDN(dn);
        double dn_next = nextDN(dn);
        double v_prev  = (dn_prev != dn) ? velForDN(dn_prev) : -1.0;
        double v_next  = (dn_next != dn) ? velForDN(dn_next) : -1.0;

        // Gerekçe belirle
        QString reason;
        if (v_prev > 0 && v_prev <= V_MAX && v_prev >= V_MIN)
            reason = "Bir küçük DN hız limiti içinde → Ekonomi";
        else if (v_cur > V_MAX)
            reason = "Mevcut hız max sınırı aşıyor!";
        else if (v_next > 0 && v_next < V_MIN)
            reason = "Bir büyük DN min hızı sağlamıyor";
        else if (dn_prev == dn)
            reason = "Serinin en küçük boyutu";
        else
            reason = "EN 806-3: hız + basınç dengesi";

        auto makeItem = [](const QString& txt, Qt::AlignmentFlag align = Qt::AlignHCenter) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(align | Qt::AlignVCenter);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };

        table->setItem(row, 0, makeItem(QString::number(eid)));
        table->setItem(row, 1, makeItem(QString("DN%1").arg(dn, 0, 'f', 0)));
        table->setItem(row, 2, makeItem(QString("%1").arg(q_m3s * 1000.0, 0, 'f', 3)));
        table->setItem(row, 3, makeItem(QString("%1").arg(v_cur, 0, 'f', 2)));

        QString prevStr = (dn_prev != dn && v_prev >= 0)
            ? QString("DN%1→%2 m/s").arg(dn_prev, 0, 'f', 0).arg(v_prev, 0, 'f', 2)
            : "—";
        QString nextStr = (dn_next != dn && v_next >= 0)
            ? QString("DN%1→%2 m/s").arg(dn_next, 0, 'f', 0).arg(v_next, 0, 'f', 2)
            : "—";

        table->setItem(row, 4, makeItem(prevStr));
        table->setItem(row, 5, makeItem(nextStr));
        auto* reasonItem = makeItem(reason, Qt::AlignLeft);
        if (v_cur > V_MAX)
            reasonItem->setBackground(QColor(255, 200, 200));
        else if (reason.contains("Ekonomi"))
            reasonItem->setBackground(QColor(255, 255, 180));
        table->setItem(row, 6, reasonItem);

        ++row;
    }

    lay->addWidget(table);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(btn);
    dlg.exec();

    if (m_logList)
        m_logList->addItem(QString("Hesap kararı: %1 boru için gerekçe gösterildi").arg(row));
}

// ═══════════════════════════════════════════════════════════
//  OTOMATİK ÖLÇÜLENDİRME — paralel boru stacking + anti-overlap
// ═══════════════════════════════════════════════════════════
void MainWindow::OnAutoOlculendir() {
    if (!m_document) return;

    const auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        statusBar()->showMessage("Ölçülendirilecek MEP borusu yok", 2000);
        return;
    }

    // Mevcut VKT-DIM ölçülerini temizle mi?
    bool hasDims = false;
    for (const auto& ent : m_document->GetCADEntities()) {
        if (ent && ent->GetLayerName() == "VKT-DIM") { hasDims = true; break; }
    }
    if (hasDims) {
        auto btn = QMessageBox::question(this, "Otomatik Ölçülendirme",
            "Mevcut VKT-DIM ölçüleri bulundu.\n"
            "Sil ve yeniden oluştur?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) return;
        if (btn == QMessageBox::Yes) {
            m_document->RemoveCADEntitiesByLayer("VKT-DIM");
        }
    }

    // Ölçülendirilecek her kenar için hesap bilgisi
    struct DimInfo {
        uint32_t  edgeId;
        double    midX, midY;
        double    nx, ny;      // birim normal (boruya dik)
        int       angleSlot;   // 15° adımlı quantize
        int       offsetLevel; // kaçıncı offset sırası (0=1500, 1=3000, ...)
    };

    static constexpr double BASE_OFFSET  = 1500.0;  // ilk ölçü ofseti (mm)
    static constexpr double NEARBY_PIPE  = 3000.0;  // bu mesafedeki paralel borular stack
    static constexpr double PI_L = 3.14159265358979323846;

    std::vector<DimInfo> infos;
    infos.reserve(network.GetEdgeMap().size());

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply   &&
            edge.type != mep::EdgeType::HotWater &&
            edge.type != mep::EdgeType::Gas       &&
            edge.type != mep::EdgeType::Heating   &&
            edge.type != mep::EdgeType::FireLine) continue;

        const mep::Node* nA = network.GetNode(edge.nodeA);
        const mep::Node* nB = network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        double dx = nB->position.x - nA->position.x;
        double dy = nB->position.y - nA->position.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < 1.0) continue;

        // Boru açısını [0,180) aralığına normalize et (karşılıklı borular aynı slot)
        double ang = std::atan2(dy, dx) * 180.0 / PI_L;
        if (ang < 0) ang += 180.0;

        DimInfo di;
        di.edgeId = eid;
        di.midX = (nA->position.x + nB->position.x) / 2.0;
        di.midY = (nA->position.y + nB->position.y) / 2.0;
        di.nx   = -dy / len;   // normal (sola dik)
        di.ny   =  dx / len;
        di.angleSlot  = static_cast<int>(std::round(ang / 15.0)) % 12;
        di.offsetLevel = 0;
        infos.push_back(di);
    }

    // Anti-overlap: paralel ve yakın borulara artan offset seviyesi ata
    for (size_t i = 0; i < infos.size(); ++i) {
        int level = 0;
        bool conflict = true;
        while (conflict) {
            conflict = false;
            for (size_t j = 0; j < i; ++j) {
                if (infos[j].angleSlot   != infos[i].angleSlot)   continue;
                if (infos[j].offsetLevel != level)                 continue;
                double ddx = infos[j].midX - infos[i].midX;
                double ddy = infos[j].midY - infos[i].midY;
                // Normal eksen boyunca mesafe (perpendikler ayrım)
                double projN = std::abs(ddx * infos[i].nx + ddy * infos[i].ny);
                // Boru ekseni boyunca mesafe
                double projP = std::abs(ddx * infos[i].ny + ddy * (-infos[i].nx));
                if (projN < 2 * BASE_OFFSET && projP < NEARBY_PIPE) {
                    conflict = true;
                    ++level;
                    break;
                }
            }
        }
        infos[i].offsetLevel = level;
    }

    // Dimension entity'leri oluştur
    int added = 0;
    for (const auto& di : infos) {
        const auto& edge = network.GetEdgeMap().at(di.edgeId);
        const mep::Node* nA = network.GetNode(edge.nodeA);
        const mep::Node* nB = network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        double offset = (di.offsetLevel + 1) * BASE_OFFSET;
        geom::Vec3 dimLinePos{ di.midX + di.nx * offset,
                               di.midY + di.ny * offset, 0.0 };

        std::string txt = "DN" + std::to_string(static_cast<int>(edge.diameter_mm));
        if (edge.length_m > 0.0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " / %.2fm", edge.length_m);
            txt += buf;
        }
        // Debi varsa ekle (m³/s → l/s dönüşümü)
        if (edge.flowRate_m3s > 0.0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " | %.2fL/s", edge.flowRate_m3s * 1000.0);
            txt += buf;
        }

        auto dim = std::make_unique<cad::Dimension>(
            nA->position, nB->position, dimLinePos, cad::DimensionType::Aligned);
        dim->SetOverrideText(txt);
        dim->SetTextHeight(300.0);
        dim->SetLayerName("VKT-DIM");
        m_document->AddCADEntity(std::move(dim));
        ++added;
    }

    if (added > 0) {
        auto& layers = m_document->GetLayersMutable();
        if (!layers.count("VKT-DIM")) {
            cad::Layer dimLayer("VKT-DIM");
            dimLayer.SetColor({0, 200, 200, 255}); // cyan
            layers["VKT-DIM"] = dimLayer;
        }
        RebuildCADEntityCache();
        InvalidateRenderer();
        m_document->SetModified(true);
        int maxLevel = 0;
        for (const auto& di : infos) maxLevel = std::max(maxLevel, di.offsetLevel);
        statusBar()->showMessage(
            QString("%1 boru ölçüsü eklendi (maks. %2 offset seviyesi — VKT-DIM)")
                .arg(added).arg(maxLevel + 1));
        if (m_logList) m_logList->addItem(
            QString("Otomatik ölçülendirme: %1 Dimension, %2 stacking seviyesi")
                .arg(added).arg(maxLevel + 1));
    } else {
        statusBar()->showMessage("Ölçülendirilecek boru bulunamadı", 2000);
    }
}

// ═══════════════════════════════════════════════════════════
//  BASINÇ BÖLGESİ YÖNETİMİ
// ═══════════════════════════════════════════════════════════
void MainWindow::OnBaskiBolgesi() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // Kaç kaynak (Source) var?
    std::vector<uint32_t> sources;
    for (const auto& [nid, node] : network.GetNodeMap())
        if (node.type == mep::NodeType::Source)
            sources.push_back(nid);

    if (sources.empty()) {
        QMessageBox::warning(this, "Basınç Bölgesi",
            "Şebekede kaynak (Source) bulunamadı.\n"
            "Önce en az bir su kaynağı yerleştirin.");
        return;
    }

    // Bilgi diyaloğu: kaynak sayısı, bölge bilgisi
    QString info;
    info += QString("<b>%1 kaynak (basınç bölgesi) tespit edildi:</b><br><br>").arg(sources.size());

    mep::HydraulicSolver solver(network);
    solver.SetNorm(mep::HydraulicSolver::GlobalNorm());
    solver.Solve();

    for (uint32_t sid : sources) {
        const mep::Node* src = network.GetNode(sid);
        if (!src) continue;

        // Bu kaynağa bağlı edge'leri bul ve toplam LU hesapla
        double totalLU = 0.0;
        int pipeCount  = 0;
        for (const auto& [eid, edge] : network.GetEdgeMap()) {
            if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
            totalLU += edge.flowRate_m3s * 1000.0; // l/s gösterg
            ++pipeCount;
        }

        // Kritik yol başlangıç noktasından kritik kayıp
        auto critRes = solver.CalculateCriticalPath();
        double critLoss = critRes.totalHeadLoss_m;

        info += QString("• Kaynak #%1 (<i>%2</i>)<br>")
                    .arg(sid).arg(QString::fromStdString(src->label));
        info += QString("&nbsp;&nbsp;Toplam boru: %1 | Kritik kayıp: %2 m su sütunu<br>")
                    .arg(pipeCount).arg(critLoss, 0, 'f', 2);
        if (critRes.requiredPumpHead_m > 0) {
            info += QString("&nbsp;&nbsp;⚠ Pompa gerekli: %1 m / %2 m³/h<br>")
                        .arg(critRes.requiredPumpHead_m, 0, 'f', 1)
                        .arg(critRes.requiredFlow_m3h, 0, 'f', 2);
            info += QString("&nbsp;&nbsp;Öneri: <b>%1</b><br>")
                        .arg(QString::fromStdString(critRes.suggestedPumpModel));
        } else {
            info += QString("&nbsp;&nbsp;✅ Şebeke basıncı yeterli<br>");
        }
        info += "<br>";
        break; // Şimdilik tek kaynak modeli
    }

    if (sources.size() > 1) {
        info += "<i>Not: Çoklu kaynaklı sistemde her bölge ayrı hidrolik çevre oluşturur.<br>"
                "Bölge sınırını belirlemek için her kaynağa ait NODE'ları farklı katmanlara taşıyın.</i>";
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Basınç Bölgesi Analizi");
    dlg.setMinimumWidth(480);
    auto* vl  = new QVBoxLayout(&dlg);
    auto* lbl = new QLabel(info);
    lbl->setWordWrap(true);
    vl->addWidget(lbl);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

// ═══════════════════════════════════════════════════════════
//  ÜRETİCİ KATALOG — Wavin / Valsir / Henco boru veritabanı
// ═══════════════════════════════════════════════════════════
void MainWindow::OnUreticiKatalog() {
    // Katalog diyaloğu — boru malzeme ve fiyat bilgisi
    QDialog dlg(this);
    dlg.setWindowTitle("Üretici Katalog — Boru Seçimi");
    dlg.setMinimumWidth(620);
    dlg.setMinimumHeight(420);
    auto* vl = new QVBoxLayout(&dlg);
    vl->addWidget(new QLabel("<b>Boru Üreticisi Seçimi (Wavin / Valsir / Henco)</b>"));

    struct CatalogEntry {
        QString uretici;
        QString malzeme;
        QString seri;
        QVector<int> dnliste;
        double fiyat_m;   // TL/m DN20 referans
        double roughness; // mm
    };

    QVector<CatalogEntry> catalog = {
        {"Wavin",  "PVC-U",  "Ekoplastik",     {16,20,25,32,40,50,63,75,90,110},  42.0, 0.0015},
        {"Wavin",  "PP-R",   "Tigris M",        {16,20,25,32,40,50,63},            58.0, 0.0015},
        {"Wavin",  "PE-Xc",  "Tigris Blue",     {16,20,25,32},                     75.0, 0.0015},
        {"Valsir", "PP",     "Triplus 3",       {32,40,50,75,90,110,125,160},      38.0, 0.0015},
        {"Valsir", "PVC-U",  "Pushing PVC",     {32,40,50,75,90,110,125,160,200},  35.0, 0.0015},
        {"Valsir", "HDPE",   "Silere",          {75,90,110,125,160,200},           65.0, 0.0010},
        {"Henco",  "PE-RT",  "Henco Pert II",   {16,20,25,32},                     82.0, 0.0015},
        {"Henco",  "Bakır",  "Henco Copper",    {15,18,22,28,35,42,54},           235.0, 0.0010},
        {"Henco",  "Çelik",  "Isıtma Çeliği",  {15,20,25,32,40,50,65,80,100},     98.0, 0.0460},
    };

    auto* tbl = new QTableWidget(catalog.size(), 6, &dlg);
    tbl->setHorizontalHeaderLabels({"Üretici","Malzeme","Seri","DN Aralığı","Fiyat TL/m","ε (mm)"});
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int r = 0; r < catalog.size(); ++r) {
        const auto& e = catalog[r];
        QStringList dns;
        for (int d : e.dnliste) dns << QString("DN%1").arg(d);
        tbl->setItem(r, 0, new QTableWidgetItem(e.uretici));
        tbl->setItem(r, 1, new QTableWidgetItem(e.malzeme));
        tbl->setItem(r, 2, new QTableWidgetItem(e.seri));
        tbl->setItem(r, 3, new QTableWidgetItem(dns.join(", ")));
        tbl->setItem(r, 4, new QTableWidgetItem(QString::number(e.fiyat_m, 'f', 0)));
        tbl->setItem(r, 5, new QTableWidgetItem(QString::number(e.roughness, 'g', 4)));
    }
    tbl->resizeColumnsToContents();
    vl->addWidget(tbl);

    vl->addWidget(new QLabel("<i>Seçili satırı projeye ekle → malzeme panelinde kullanılabilir</i>"));

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, [&]() {
        // Seçili satırı Database'e ekle (mevcut malzeme listesine)
        auto rows = tbl->selectionModel()->selectedRows();
        if (rows.isEmpty()) { dlg.reject(); return; }
        int r = rows.first().row();
        const auto& e = catalog[r];
        mep::PipeData pd;
        pd.material    = e.malzeme.toStdString();
        pd.roughness_mm= e.roughness;
        pd.unitPrice_TL= e.fiyat_m;
        for (int d : e.dnliste) pd.availableDiameters_mm.push_back(d);
        // Database read-only singleton — yeni malzeme bilgisini log'a yaz
        if (m_logList) m_logList->addItem(
            QString("Katalog: %1 %2 (%3) eklendi — %.4f mm pürüzlülük, %4 TL/m")
                .arg(e.uretici).arg(e.malzeme).arg(e.seri)
                .arg(e.roughness).arg(e.fiyat_m, 0, 'f', 0));
        statusBar()->showMessage(
            QString("%1 %2 seçildi — özellik panelinden 'Malzeme' olarak kullanın")
                .arg(e.uretici).arg(e.malzeme), 5000);
        dlg.accept();
    });
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(bb);
    dlg.exec();
}

// ─────────────────────────────────────────────────────────────────────────────
// Birleşik Yerleştirme Modu toggle
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnBirleskMod() {
    m_birleskMod = !m_birleskMod;
    if (m_actBirleskMod)
        m_actBirleskMod->setChecked(m_birleskMod);
    QString msg = m_birleskMod
        ? "Birleşik Mod AÇIK — Armatür yerleştirince otomatik boru hattına bağlanır"
        : "Birleşik Mod KAPALI";
    statusBar()->showMessage(msg);
    if (m_logList) m_logList->addItem(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Renderer + text overlay yenile (tekrar eden 3-satır bloğun tek noktası)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::InvalidateRenderer() {
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer() && m_document) {
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
        m_vulkanWindow->GetRenderer()->InvalidateCADData();
    }
    RefreshTextOverlay();
}

// ─────────────────────────────────────────────────────────────────────────────
// Katman Yöneticisi panelini güncelle
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::RefreshLayerPanel() {
    if (!m_layerList) return;
    m_layerList->blockSignals(true);
    m_layerList->clear();
    if (!m_document) {
        m_layerList->blockSignals(false);
        return;
    }
    const auto& layers = m_document->GetLayers();
    std::vector<std::string> names;
    names.reserve(layers.size());
    for (const auto& [name, _] : layers) names.push_back(name);
    std::sort(names.begin(), names.end());

    for (const auto& name : names) {
        const cad::Layer& layer = layers.at(name);
        auto* item = new QTreeWidgetItem(m_layerList);

        // Sütun 0 — Görünür (G)
        item->setText(0, layer.IsVisible() ? "●" : "○");
        item->setForeground(0, layer.IsVisible() ? QColor(80, 200, 80) : QColor(120, 120, 120));
        item->setToolTip(0, layer.IsVisible() ? "Görünür — tıkla: gizle" : "Gizli — tıkla: göster");

        // Sütun 1 — Kilitli (K)
        item->setText(1, layer.IsLocked() ? "🔒" : "🔓");
        item->setToolTip(1, layer.IsLocked() ? "Kilitli (seçilemez) — tıkla: kilidi kaldır" : "Serbest — tıkla: kilitle");

        // Sütun 2 — Dondurulmuş (D)
        item->setText(2, layer.IsFrozen() ? "❄" : "·");
        item->setForeground(2, layer.IsFrozen() ? QColor(100, 180, 255) : QColor(80, 80, 80));
        item->setToolTip(2, layer.IsFrozen() ? "Dondurulmuş (render'a girmiyor) — tıkla: çöz" : "Aktif — tıkla: dondur");

        // Sütun 3 — Katman adı (renkli)
        item->setText(3, QString::fromStdString(name));
        cad::Color col = layer.GetColor();
        QColor qcol(col.r, col.g, col.b);
        // Koyu arka plan varsa açık rengi beyaza çek
        if (col.r < 30 && col.g < 30 && col.b < 30) qcol = QColor(180, 180, 180);
        item->setForeground(3, qcol);

        // Donmuş veya kilitli satırları soluk göster
        if (layer.IsFrozen() || !layer.IsVisible()) {
            for (int c = 0; c < 4; ++c)
                item->setForeground(c, QColor(70, 70, 70));
            item->setForeground(3, QColor(90, 90, 90));
        }
    }
    m_layerList->blockSignals(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Seçili entity'nin katmanını panelde vurgula (AutoCAD layer dropdown eşdeğeri)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::HighlightLayerInPanel(const std::string& layerName) {
    if (!m_layerList) return;
    const QString qName = QString::fromStdString(layerName);
    static const QColor kActiveBg(60, 55, 20);
    static const QColor kDefaultBg(30, 30, 30);
    for (int i = 0; i < m_layerList->topLevelItemCount(); ++i) {
        auto* item = m_layerList->topLevelItem(i);
        if (!item) continue;
        const bool isActive = (item->text(3) == qName);
        QFont f = item->font(3);
        if (f.bold() != isActive) {
            f.setBold(isActive);
            item->setFont(3, f);
        }
        const QColor needed = isActive ? kActiveBg : kDefaultBg;
        if (item->background(3).color() != needed)
            item->setBackground(3, needed);
        if (isActive)
            m_layerList->scrollToItem(item);
    }
    if (m_layerPanel) {
        m_layerPanel->setWindowTitle(qName.isEmpty()
            ? "Katmanlar"
            : QString("Katmanlar  [%1]").arg(qName));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CAD entity pointer cache — O(1) drag lookup + snap vektörü
// ─────────────────────────────────────────────────────────────────────────────
// ============================================================
//  GRIP EDITING — ComputeGrips / ClearGrips / UpdateGripScreen
// ============================================================
void MainWindow::ComputeGrips() {
    m_grips.clear();
    if (!m_vulkanWindow || !m_document) return;

    auto addGrip = [&](cad::EntityId id, int idx, const geom::Vec3& wp) {
        GripPoint gp;
        gp.entityId = id;
        gp.index    = idx;
        gp.worldPos = wp;
        gp.screenPos = QPoint(0, 0); // UpdateGripScreen ile doldurulacak
        m_grips.push_back(gp);
    };

    // Tüm seçili entity'lerin grip'lerini hesapla
    auto processEntity = [&](cad::EntityId eid) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) return;
        cad::Entity* e = it->second;

        switch (e->GetType()) {
            case cad::EntityType::Line: {
                const auto* ln = static_cast<const cad::Line*>(e);
                auto s = ln->GetStart(), en = ln->GetEnd();
                addGrip(eid, 0, s);
                addGrip(eid, 1, geom::Vec3((s.x+en.x)/2, (s.y+en.y)/2, (s.z+en.z)/2));
                addGrip(eid, 2, en);
                break;
            }
            case cad::EntityType::Circle: {
                const auto* cr = static_cast<const cad::Circle*>(e);
                addGrip(eid, 0, cr->GetCenter());
                break;
            }
            case cad::EntityType::Arc: {
                const auto* ar = static_cast<const cad::Arc*>(e);
                auto c = ar->GetCenter();
                double r = ar->GetRadius();
                double sa = ar->GetStartAngle() * M_PI / 180.0;
                double ea = ar->GetEndAngle()   * M_PI / 180.0;
                double ma = (sa + ea) / 2.0;
                addGrip(eid, 0, {c.x + r*std::cos(sa), c.y + r*std::sin(sa), c.z});
                addGrip(eid, 1, {c.x + r*std::cos(ma), c.y + r*std::sin(ma), c.z});
                addGrip(eid, 2, {c.x + r*std::cos(ea), c.y + r*std::sin(ea), c.z});
                break;
            }
            case cad::EntityType::Polyline: {
                const auto* pl = static_cast<const cad::Polyline*>(e);
                const auto& verts = pl->GetVertices();
                for (int vi = 0; vi < static_cast<int>(verts.size()); ++vi)
                    addGrip(eid, vi, verts[vi].pos);
                break;
            }
            default: break;
        }
    };

    if (m_selectedCADEntityId != 0)
        processEntity(m_selectedCADEntityId);
    for (auto eid : m_selectedCADEntityIds)
        processEntity(eid);

    UpdateGripScreen();
    if (m_snapOverlay) {
        std::vector<QPoint> pts;
        pts.reserve(m_grips.size());
        for (const auto& g : m_grips) pts.push_back(g.screenPos);
        m_snapOverlay->SetGrips(pts, m_hotGrip);
    }
}

void MainWindow::ClearGrips() {
    m_grips.clear();
    m_gripDragging = false;
    m_activeGrip   = -1;
    m_hotGrip      = -1;
    if (m_snapOverlay) m_snapOverlay->ClearGrips();
}

void MainWindow::UpdateGripScreen() {
    if (!m_vulkanWindow) return;
    const auto& vp = m_vulkanWindow->GetViewport();
    for (auto& g : m_grips) {
        auto s = vp.WorldToScreen(g.worldPos);
        g.screenPos = QPoint(static_cast<int>(s.x), static_cast<int>(s.y));
    }
}

void MainWindow::RebuildCADEntityCache() {
    // Entity vektörü değişmeden önce async CAD build'i tamamlat
    if (m_vulkanWindow) m_vulkanWindow->WaitForCADBuild();
    m_cadEntityCache.clear();
    m_snapEntityCache.clear();
    if (!m_document) return;
    const auto& entities = m_document->GetCADEntities();
    m_cadEntityCache.reserve(entities.size());
    m_snapEntityCache.reserve(entities.size());
    for (const auto& e : entities) {
        if (!e) continue;
        m_cadEntityCache[e->GetId()] = e.get();
        m_snapEntityCache.push_back(e.get());
    }
    // Mekânsal ızgara — büyük projelerde pick O(n)→O(1)
    m_entityGrid.Build(entities);
}

} // namespace ui
} // namespace vkt
