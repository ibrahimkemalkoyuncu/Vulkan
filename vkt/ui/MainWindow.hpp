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
#include "core/FloorManager.hpp"
#include "cad/SpaceManager.hpp"
#include "cad/Viewport.hpp"
#include "cad/Entity.hpp"
#include "ui/SpacePanel.hpp"
#include "ui/FixturePropertiesPanel.hpp"
#include "ui/CommandBar.hpp"
#include "ui/SnapOverlay.hpp"
#include "ui/PBRMaterialEditor.hpp"
#include "cad/SnapManager.hpp"

namespace vkt {
namespace render { class VulkanWindow; }
namespace ui {

/// Aktif cizim araci
enum class ToolMode { Select, DrawPipe, PlaceFixture, PlaceJunction };
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

    // Mimari kat yönetimi
    void OnMimariBelirle();

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
    QAction* m_actDrawPipe = nullptr;
    QAction* m_actDrawFixture = nullptr;
    QAction* m_actDrawJunction = nullptr;
    QAction* m_actSelect = nullptr;
    QAction* m_actPlanView = nullptr;
    QAction* m_actIsometricView = nullptr;
    QAction* m_actZoomExtents = nullptr;
    QAction* m_actRunHydraulics = nullptr;
    QAction* m_actAutoSizeDN = nullptr;
    QAction* m_actGenerateSchedule = nullptr;
    QAction* m_actExportReport = nullptr;
    QAction* m_actMimariBelirle = nullptr;

    // Tool state
    ToolMode m_currentToolMode = ToolMode::Select;
    DrawState m_drawState = DrawState::Idle;
    uint32_t m_firstNodeId = 0;       // DrawPipe icin ilk node
    bool     m_firstNodeInGraph = false; // true = m_firstNodeId zaten graph'ta
    geom::Vec3 m_firstClickPos;       // Ilk tiklama world koordinati
    QPoint   m_firstClickScreen;      // Rubber-band için ekran koordinatı

    // Armatür tipi (PlaceFixture modu)
    QString m_selectedFixtureType = "Lavabo";

    // Selection state (Select modunda aktif seçim)
    uint32_t m_selectedNodeId = 0;    // 0 = seçim yok
    uint32_t m_selectedEdgeId = 0;    // 0 = seçim yok

    // Gerçek zamanlı hidrolik debounce zamanlayıcısı
    QTimer* m_autoHydroTimer = nullptr;

    // Mimari kat yönetimi
    core::FloorManager m_floorManager;

    // Mesafe ölçüm modu (UZAKLIK/DISTANCE komutu)
    bool       m_measureMode       = false;
    bool       m_measureHasFirstPt = false;
    geom::Vec3 m_measurePt1;

    // Mouse event handler'lari
    void HandleMousePress(double worldX, double worldY, Qt::MouseButton button);
    void HandleMouseMove(double worldX, double worldY);

    // CAD text entity'lerini SnapOverlay'e aktar
    void RefreshTextOverlay();
};

} // namespace ui
} // namespace vkt
