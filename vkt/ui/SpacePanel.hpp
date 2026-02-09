/**
 * @file SpacePanel.hpp
 * @brief Mahal Yönetim Paneli
 */

#pragma once

#include <QDockWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include "cad/SpaceManager.hpp"

namespace vkt {
namespace ui {

/**
 * @class SpacePanel
 * @brief Mahal listesi ve özellikleri paneli
 * 
 * Mahalleri listeler, filtreler, özelliklerini gösterir.
 */
class SpacePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SpacePanel(QWidget* parent = nullptr);
    ~SpacePanel() override = default;
    
    /**
     * @brief SpaceManager'ı bağla
     */
    void SetSpaceManager(cad::SpaceManager* manager);
    
    /**
     * @brief Listeyi güncelle
     */
    void RefreshList();
    
    /**
     * @brief Seçili mahal ID'si
     */
    cad::EntityId GetSelectedSpaceId() const;

signals:
    void SpaceSelected(unsigned long long spaceId);
    void SpaceDoubleClicked(unsigned long long spaceId);
    void SpacePropertiesChanged(unsigned long long spaceId);
    void DeleteSpaceRequested(unsigned long long spaceId);

private slots:
    void OnSpaceSelected();
    void OnSpaceDoubleClicked(QTreeWidgetItem* item, int column);
    void OnFilterChanged(const QString& text);
    void OnTypeFilterChanged(int index);
    void OnDeleteSpace();
    void OnRefresh();
    void OnShowStatistics();

private:
    void SetupUI();
    void UpdateStatistics();
    void PopulateTypeFilter();
    
    cad::SpaceManager* m_spaceManager = nullptr;
    
    // UI Components
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeFilterCombo = nullptr;
    QTreeWidget* m_spaceTree = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_statsButton = nullptr;
    
    QLabel* m_totalCountLabel = nullptr;
    QLabel* m_totalAreaLabel = nullptr;
    QLabel* m_totalVolumeLabel = nullptr;
};

} // namespace ui
} // namespace vkt
