/**
 * @file DXFImportDialog.hpp
 * @brief CAD Dosya Import Dialog (DXF + DWG)
 * 
 * Mimari projeleri DXF/DWG formatlarından import eder.
 */

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QString>
#include <memory>
#include <unordered_map>
#include "cad/DXFReader.hpp"
#ifdef HAVE_LIBREDWG
#include "cad/DWGReader.hpp"
#endif
#include "cad/SpaceManager.hpp"

namespace vkt {
namespace ui {

/**
 * @class DXFImportDialog
 * @brief CAD dosya import (DXF/DWG) ve otomatik mahal tespiti dialogu
 * 
 * İki aşamalı workflow:
 * 1. CAD dosya seçimi (DXF veya DWG) ve import ayarları
 * 2. Tespit edilen mahallerin önizleme ve onay
 */
class DXFImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit DXFImportDialog(QWidget* parent = nullptr);
    ~DXFImportDialog() override = default;
    
    /**
     * @brief Import edilen entity'leri getir
     */
    std::vector<cad::Entity*> GetImportedEntities() const;
    
    /**
     * @brief Tespit edilen ve onaylanan mahalleri getir
     */
    std::vector<cad::SpaceCandidate> GetAcceptedSpaces() const;
    
    /**
     * @brief Entity sahipliğini devret (rendering için)
     */
    std::vector<std::unique_ptr<cad::Entity>> TakeEntities();

    /**
     * @brief Import başarılı mı?
     */
    bool WasSuccessful() const { return m_importSuccess; }

    /**
     * @brief DXF dosyasındaki global $LTSCALE değeri (1.0 = varsayılan)
     */
    double GetDXFLtscale() const;

    /**
     * @brief DWG'den okunan layer bilgilerini getir
     */
    std::unordered_map<std::string, cad::Layer> GetLayers() const;

private slots:
    void OnBrowseFile();
    void OnImport();
    void OnDetectSpaces();
    void OnAcceptAll();
    void OnRejectAll();
    void OnAcceptSelected();
    void OnCandidateSelectionChanged();
    void OnLayerFilterChanged();

private:
    void SetupUI();
    void UpdateCandidateList();
    void UpdateStatistics();
    
    // UI Components - Sayfa 1: DXF Import
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    
    // Import istatistikleri
    QLabel* m_entityCountLabel = nullptr;
    QLabel* m_layerCountLabel = nullptr;
    QLabel* m_readTimeLabel = nullptr;
    
    // UI Components - Sayfa 2: Space Detection
    QCheckBox* m_detectSpacesCheckbox = nullptr;
    QLineEdit* m_layerFilterEdit = nullptr;
    QDoubleSpinBox* m_minAreaSpinBox = nullptr;
    QDoubleSpinBox* m_maxAreaSpinBox = nullptr;
    QCheckBox* m_detectNamesCheckbox = nullptr;
    QCheckBox* m_autoInferTypesCheckbox = nullptr;
    
    QPushButton* m_detectButton = nullptr;
    QListWidget* m_candidateListWidget = nullptr;
    QPushButton* m_acceptAllButton = nullptr;
    QPushButton* m_rejectAllButton = nullptr;
    QPushButton* m_acceptSelectedButton = nullptr;
    
    QLabel* m_candidateCountLabel = nullptr;
    
    // Data
    enum class FileType { DXF, DWG, Unknown };
    FileType m_fileType = FileType::Unknown;
    
    std::unique_ptr<cad::DXFReader> m_dxfReader;
#ifdef HAVE_LIBREDWG
    std::unique_ptr<cad::DWGReader> m_dwgReader;
#endif
    std::vector<cad::SpaceCandidate> m_candidates;
    std::vector<bool> m_candidateAccepted; // Her aday için onay durumu
    bool m_importSuccess = false;
};

} // namespace ui
} // namespace vkt
