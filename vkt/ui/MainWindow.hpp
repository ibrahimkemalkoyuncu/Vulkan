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
#include <memory>
#include "core/Document.hpp"
#include "cad/SpaceManager.hpp"
#include "cad/Entity.hpp"
#include "ui/SpacePanel.hpp"

namespace vkt {
namespace ui {

/**
 * @class MainWindow
 * @brief Ana CAD penceresi (FINE SANI++ özellikleriyle)
 * 
 * Toolbar, property panel, layer manager içerir.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void SetDocument(core::Document* doc);
    core::Document* GetDocument() const { return m_document; }

protected:
    void closeEvent(QCloseEvent* event) override;

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
    void OnGenerateSchedule();
    void OnExportReport();

    // Mahal (Space) yönetimi
    void OnSelectSpace();
    void OnCalculateLoads();
    void OnSpaceSelected(unsigned long long spaceId);
    void OnSpaceDoubleClicked(unsigned long long spaceId);
    void OnDeleteSpace(unsigned long long spaceId);

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
    QAction* m_actGenerateSchedule = nullptr;
    QAction* m_actExportReport = nullptr;
};

} // namespace ui
} // namespace vkt
