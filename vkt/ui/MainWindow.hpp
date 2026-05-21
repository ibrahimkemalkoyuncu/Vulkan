/**
 * @file MainWindow.hpp
 * @brief VKT Ana Pencere UI
 * 
 * Qt tabanlı tam özellikli CAD arayüzü.
 */

#pragma once

#include <QMainWindow>
#include <QToolBar>
#include <QDockWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QKeyEvent>
#include <QTimer>
#include <memory>
#include "core/Document.hpp"
#include "mep/NetworkGraph.hpp"
#include "core/FloorManager.hpp"
#include "core/ProjectManager.hpp"
#include "cad/SpaceManager.hpp"
#include "cad/Viewport.hpp"
#include "cad/Entity.hpp"
#include "ui/SpacePanel.hpp"
#include "ui/FixturePropertiesPanel.hpp"
#include "ui/CommandBar.hpp"
#include "ui/SnapOverlay.hpp"
#include "ui/PBRMaterialEditor.hpp"
#include "ui/STFixturePanel.hpp"
#include "ui/DevreSecenekleriDialog.hpp"
#include "cad/SnapManager.hpp"

namespace vkt {
namespace render { class VulkanWindow; }
namespace ui {

/// Aktif cizim araci
enum class ToolMode { Select, DrawPipe, PlaceFixture, PlaceJunction, PlaceDrain, ConnectFixture, DrawPolyArea };
enum class DrawState { Idle, WaitingFirstPoint, WaitingSecondPoint };

/**
 * @class MainWindow
 * @brief Ana CAD penceresi (FINE SANI++ özellikleriyle)
 *
 * Toolbar, property panel, layer manager içerir.
 * VulkanWindow üzerinden GPU rendering yapar.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void SetDocument(core::Document* doc);
    core::Document* GetDocument() const { return m_document; }

    render::VulkanWindow* GetVulkanWindow() const { return m_vulkanWindow; }

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    // Dosya menüsü
    void OnNew();
    void OnOpen();
    void OnSave();
    void OnSaveAs();
    void OnImportDXF();
    void OnExit();

    // Düzen menüsü
    void OnUndo();
    void OnRedo();
    void OnDelete();

    // Çizim araçları
    void OnDrawPipe();
    void OnDrawFixture();
    void OnDrawJunction();
    void OnSelectMode();

    // Görünüm
    void OnPlanView();
    void OnIsometricView();
    void OnZoomExtents();

    // Analiz
    void OnRunHydraulics();
    void OnAutoSizeDN();
    void OnGenerateSchedule();
    void OnExportReport();

    // Gerçek zamanlı hidrolik (debounced)
    void ScheduleAutoHydro();   ///< Debounce zamanlayıcısını başlat/yeniden başlat
    void RunAutoHydro();        ///< Timer'dan tetiklenen sessiz DN boyutlandırma

    // Mahal (Space) yönetimi
    void OnSelectSpace();
    void OnCalculateLoads();
    void OnSpaceSelected(unsigned long long spaceId);
    void OnSpaceDoubleClicked(unsigned long long spaceId);
    void OnDeleteSpace(unsigned long long spaceId);

    // Node seçimi
    void OnNodeSelected(uint32_t nodeId);
    void OnNodeUpdated(uint32_t nodeId);

    // PBR malzeme editörü
    void OnPBRMaterialChanged(float roughness, float metalness, float ambient,
                               float r, float g, float b,
                               float lightAzimuth, float lightElevation);

    // Proje çalışma alanı (CC klasörü eşdeğeri)
    void OnNewProject();
    void OnSetProjectsRoot();
    void OnOpenProjectFolder();

    // Mimari kat yönetimi
    void OnMimariBelirle();
    void OnFloorAlignment();   ///< 3D Hizalama Kontrolü

    // ST Cihazları paneli — armatür seçimi
    void OnSTFixtureSelected(const QString& name);

    // Pis su çizim komutları
    void OnDrawDrainPipe();
    void OnPlaceYerSuzgeci();
    void OnPlaceRogar();

    // Akıllı Bağlantı Noktası — cihaz yerine sadece bağlantı sembolü (yıldız/artı)
    void OnPlaceSmartPoint();

    // Boşaltma Noktası — en alttaki Drain'i ana tahliye olarak işaretle
    void OnBosaltmaNoktasi();

    // Uygulama Katman Görünürlüğü — Temiz Su / Sıcak Su / Pis Su bağımsız gizle/göster
    void OnLayerVisibility();

    // Sıcak su çizim komutları
    void OnDrawHotWaterPipe();
    void OnPlaceHotSource();

    // Tesisatı Kabul Et — doğrulama + numaralandırma
    void OnTesistatKabul();

    // Aktif kat değişimi
    void OnActiveFloorChanged(int index);
    void RefreshFloorSelector();     ///< MimariBelirle sonrası listeyi güncelle
    double GetActiveFloorZ() const;  ///< Aktif katın elevation_m (node z'si)

    // Tesisat Kopyalama — seçili katı başka kata kopyala (kolonlar hariç)
    void OnCopyFloor();

    // Cihazları Tesisata Bağla (FineSANI BAGLA eşdeğeri)
    void OnConnectFixture();

    // Kolon Bağlantı Asistanı — dikey boru (Z ekseni)
    void OnDrawColumn();

    // PDF Pafta (Sayfa) Düzeni
    void OnPrintLayout();

    // DXF Export — tüm proje veya aktif kat
    void OnExportDXF();
    void OnExportFloorDXF();

    // Hidrofor / Ekipman Boyutlandırma
    void OnHidrofor();

    // Hesap Normu Seçimi (EN 806-3 / DIN 1988)
    void OnNormSelection();

    // Devre Seçenekleri (bina tipi, boru cinsi, pürüzlülük, max hız)
    void OnDevreSecenekleri();

    // Baskı İçeriği — çizimde hangi etiketler görünsün
    void OnBaskiIcerigi();

    // Parçaların Basınç Kaybı — tüm devreler ayrıntılı tablo
    void OnBaskiKaybi();

    // Word/HTML rapor export
    void OnWordRapor();

    // Yağmur Suyu Modülü (EN 12056-3)
    void OnYagmurSuyu();

    // Keşif Listesi / BOM
    void OnBOM();

    // Kolon Şeması (Riser Diagram)
    void OnRiserDiagram();

    // Hesap Föyü — DN Manuel Override
    void OnDNOverride();

    // Çizimi Güncelle — hesap sonuçlarını çizime yaz
    void OnCizimiGuncelle();

    // Kapalı Çukur / Foseptik Hesabı (TS 822 / EN 12566-1)
    void OnFoseptik();

    // Pis Su Hesap Föyü — drenaj devresi ayrıntı tablosu
    void OnPisSuHesapFoyu();

    // Emdirme Çukuru hesabı (toprak emme kapasitesi)
    void OnEmdirmeCukuru();

    // Pis Su Çukuru hesabı (geçirimsiz depolama tankı)
    void OnPisSuCukuru();

    // Pis Su Pompası boyutlandırma
    void OnPisSuPompasi();

    // Membranlı Genleşme Tankı boyutlandırma (EN 12828)
    void OnGenlesimTanki();

    // Yağmur Düşme Alanı — çizimden poligon seçimi
    void OnYagmurAlani();
    void FinishPolyArea();   ///< Enter tuşuyla polygon kapatıp alan+yağmur hesabı

    // Komut satırı
    void OnCommandEntered(const QString& cmd);
    void OnCommandEscape();

    // Property değişiklikleri
    void OnPropertiesUpdated();
    void OnDiameterChanged(const QString& text);
    void OnMaterialChanged(const QString& text);
    void OnZetaChanged(const QString& text);
    void OnSlopeChanged(const QString& text);

private:
    void CreateActions();
    void CreateMenus();
    void CreateToolbars();
    void CreateDockPanels();
    void UpdateUI();

    core::Document* m_document = nullptr;

    // Vulkan rendering penceresi
    render::VulkanWindow* m_vulkanWindow = nullptr;

    // Toolbars
    QToolBar* m_drawToolbar = nullptr;
    QToolBar* m_editToolbar = nullptr;
    QToolBar* m_viewToolbar = nullptr;
    QToolBar* m_analysisToolbar = nullptr;

    // Property Panel
    QDockWidget* m_propertyPanel = nullptr;
    QLineEdit* m_propDiameter = nullptr;
    QLineEdit* m_propLength = nullptr;
    QComboBox* m_propMaterial = nullptr;
    QLineEdit* m_propZeta = nullptr;
    QLineEdit* m_propSlope = nullptr;
    QLineEdit* m_propDU = nullptr;

    // Layer Panel
    QDockWidget* m_layerPanel = nullptr;
    QListWidget* m_layerList = nullptr;

    // Log Panel
    QDockWidget* m_logPanel = nullptr;
    QListWidget* m_logList = nullptr;
    
    // Space Panel
    SpacePanel* m_spacePanel = nullptr;
    std::unique_ptr<cad::SpaceManager> m_spaceManager;

    // Fixture Properties Panel
    QDockWidget*            m_fixtureDock  = nullptr;
    FixturePropertiesPanel* m_fixturePanel = nullptr;

    QDockWidget*            m_pbrDock      = nullptr;
    PBRMaterialEditor*      m_pbrEditor    = nullptr;

    QDockWidget*            m_stDock       = nullptr;
    STFixturePanel*         m_stPanel      = nullptr;

    // Komut satırı
    CommandBar*  m_commandBar  = nullptr;

    // Snap overlay (Vulkan container üzerinde şeffaf crosshair)
    SnapOverlay* m_snapOverlay      = nullptr;
    QWidget*     m_vulkanContainer  = nullptr;
    cad::SnapManager m_snapManager;

    // Actions
    QAction* m_actNew = nullptr;
    QAction* m_actOpen = nullptr;
    QAction* m_actSave = nullptr;
    QAction* m_actSaveAs = nullptr;
    QAction* m_actImportDXF = nullptr;
    QAction* m_actExit = nullptr;
    QAction* m_actUndo = nullptr;
    QAction* m_actRedo = nullptr;
    QAction* m_actDelete = nullptr;
    QAction* m_actDrawPipe        = nullptr;
    QAction* m_actDrawFixture     = nullptr;
    QAction* m_actDrawJunction    = nullptr;
    QAction* m_actDrawDrainPipe      = nullptr;
    QAction* m_actPlaceYerSuzgeci    = nullptr;
    QAction* m_actPlaceRogar         = nullptr;
    QAction* m_actPlaceSmartPoint    = nullptr;
    QAction* m_actBosaltmaNoktasi    = nullptr;
    QAction* m_actLayerVisibility    = nullptr;
    QAction* m_actDrawHotWaterPipe = nullptr;
    QAction* m_actPlaceHotSource   = nullptr;
    QAction* m_actTesistatKabul    = nullptr;
    QAction* m_actCopyFloor        = nullptr;
    QAction* m_actConnectFixture   = nullptr;
    QAction* m_actDrawColumn       = nullptr;
    QAction* m_actPrintLayout      = nullptr;
    QAction* m_actHidrofor         = nullptr;
    QAction* m_actNormSelection      = nullptr;
    QAction* m_actDevreSecenekleri   = nullptr;
    QAction* m_actBaskiIcerigi       = nullptr;
    QAction* m_actBaskiKaybi         = nullptr;
    QAction* m_actWordRapor          = nullptr;
    QAction* m_actYagmurSuyu         = nullptr;
    QAction* m_actBOM                = nullptr;
    QAction* m_actRiserDiagram       = nullptr;
    QAction* m_actDNOverride         = nullptr;
    QAction* m_actCizimiGuncelle     = nullptr;
    QAction* m_actFoseptik           = nullptr;
    QAction* m_actPisSuHesapFoyu     = nullptr;
    QAction* m_actEmdirmeCukuru      = nullptr;
    QAction* m_actPisSuCukuru        = nullptr;
    QAction* m_actPisSuPompasi       = nullptr;
    QAction* m_actExportDXF          = nullptr;
    QAction* m_actExportFloorDXF     = nullptr;
    QAction* m_actGenlesimTanki      = nullptr;
    QAction* m_actYagmurAlani        = nullptr;
    QAction* m_actSelect = nullptr;
    QAction* m_actPlanView = nullptr;
    QAction* m_actIsometricView = nullptr;
    QAction* m_actZoomExtents = nullptr;
    QAction* m_actRunHydraulics = nullptr;
    QAction* m_actAutoSizeDN = nullptr;
    QAction* m_actGenerateSchedule = nullptr;
    QAction* m_actExportReport = nullptr;
    QAction* m_actMimariBelirle    = nullptr;
    QAction* m_actFloorAlignment   = nullptr;
    QAction* m_actNewProject       = nullptr;
    QAction* m_actSetProjectsRoot  = nullptr;
    QAction* m_actOpenProjectFolder = nullptr;

    // Tool state
    ToolMode m_currentToolMode = ToolMode::Select;
    DrawState m_drawState = DrawState::Idle;
    uint32_t m_firstNodeId = 0;       // DrawPipe icin ilk node
    bool     m_firstNodeInGraph = false; // true = m_firstNodeId zaten graph'ta
    geom::Vec3 m_firstClickPos;       // Ilk tiklama world koordinati
    QPoint   m_firstClickScreen;      // Rubber-band için ekran koordinatı

    // Armatür tipi (PlaceFixture modu)
    QString m_selectedFixtureType = "Lavabo";

    // Pis su modu: drawn pipes ve drain label
    mep::EdgeType m_currentPipeType = mep::EdgeType::Supply;
    QString       m_drainLabel      = "Yer Süzgeci";

    // ConnectFixture durumu
    uint32_t m_connectFixtureNodeId = 0; ///< Seçili armatür node ID (0 = henüz seçilmedi)
    std::vector<uint32_t> m_batchFixtureIds; ///< Batch BAGLA: birden fazla armatür listesi

    // Selection state (Select modunda aktif seçim)
    uint32_t m_selectedNodeId = 0;    // 0 = seçim yok
    uint32_t m_selectedEdgeId = 0;    // 0 = seçim yok

    // Devre parametreleri (Devre Seçenekleri dialog'dan)
    DevreParams m_devreParams;

    // Baskı İçeriği — çizimde hangi etiketler görünsün
    bool m_labelShowDN       = true;
    bool m_labelShowFlow     = false;
    bool m_labelShowLength   = false;
    bool m_labelShowVelocity = false;
    bool m_labelShowHeadLoss = false;
    bool m_labelShowSlope    = false;   ///< Drenaj eğimi (%)
    bool m_labelShowFillRate = false;   ///< Doluluk derecesi h/d (%)

    // Uygulama Katman Görünürlüğü — MEP katmanlarını bağımsız göster/gizle
    bool m_showTemizSu = true;
    bool m_showSicakSu = true;
    bool m_showPisSu   = true;

    // Ana Tahliye (Boşaltma) Noktası — işaretlenmiş Drain node
    uint32_t m_mainDrainNodeId = 0;

    // MEP node drag taşıma
    bool     m_draggingNode   = false;
    uint32_t m_dragNodeId     = 0;
    geom::Vec3 m_dragStartPos;        // Drag öncesi node pozisyonu (undo için)

    // Gerçek zamanlı hidrolik debounce zamanlayıcısı
    QTimer* m_autoHydroTimer = nullptr;

    // Mimari kat yönetimi
    core::FloorManager m_floorManager;
    int      m_activeFloorIndex = 0;   ///< Aktif kat (çizim z'si = floor.elevation_m)
    QComboBox* m_floorSelector  = nullptr; ///< Toolbar'daki kat seçici

    // Mesafe ölçüm modu (UZAKLIK/DISTANCE komutu)
    bool       m_measureMode       = false;
    bool       m_measureHasFirstPt = false;
    geom::Vec3 m_measurePt1;

    // Yağmur düşme alanı poligon çizim noktaları (DrawPolyArea modu)
    std::vector<geom::Vec3> m_polyAreaPoints;

    // Mouse event handler'lari
    void HandleMousePress(double worldX, double worldY, Qt::MouseButton button);
    void HandleMouseMove(double worldX, double worldY);
    void HandleMouseRelease(double worldX, double worldY, Qt::MouseButton button);

    // CAD text entity'lerini SnapOverlay'e aktar
    void RefreshTextOverlay();
};

} // namespace ui
} // namespace vkt
