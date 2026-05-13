/**
 * @file FixturePropertiesPanel.hpp
 * @brief Armatür (Node) özellikleri dock paneli
 *
 * Seçili NetworkGraph node'unun özelliklerini gösterir ve düzenlemeye izin verir:
 *   - Armatür tipi (Lavabo, WC, Duş, ...)
 *   - Etiket / açıklama
 *   - Yük birimi (LU: besleme, DU: drenaj)
 *   - Eşzamanlı kullanım bayrağı
 *   - Kat ataması (FloorManager)
 *   - FixtureSymbolLibrary'den SVG sembol önizlemesi
 */

#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

#include "mep/NetworkGraph.hpp"
#include "core/FloorManager.hpp"

namespace vkt {
namespace ui {

class FixturePropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit FixturePropertiesPanel(QWidget* parent = nullptr);

    /**
     * @brief Seçili node'u yükle
     * @param nodeId    Gösterilecek node (0 = temizle)
     * @param graph     Ağ veri yapısı (okuma + yazma)
     * @param floors    Kat yöneticisi (kat listesi için)
     */
    void LoadNode(uint32_t nodeId,
                  mep::NetworkGraph* graph,
                  core::FloorManager* floors);

    /** @brief Paneli temizle (seçim kaldırıldığında) */
    void Clear();

signals:
    /** @brief Kullanıcı "Uygula" tıkladığında — node güncellendi */
    void NodeUpdated(uint32_t nodeId);

private slots:
    void OnApply();
    void OnTypeChanged(int index);
    void UpdateSymbolPreview();

private:
    void BuildUI();
    void PopulateFloors(core::FloorManager* floors);

    // Çizim yardımcısı: SVG string → QPixmap
    static QPixmap SVGToPixmap(const std::string& svg, int size = 80);

    // Mevcut seçim
    uint32_t          m_nodeId = 0;
    mep::NetworkGraph* m_graph  = nullptr;

    // UI widget'ları
    QLabel*        m_nodeIdLabel   = nullptr;
    QComboBox*     m_typeCombo     = nullptr;
    QLineEdit*     m_labelEdit     = nullptr;
    QDoubleSpinBox* m_loadUnitSpin = nullptr;
    QCheckBox*     m_simultCheck   = nullptr;
    QComboBox*     m_floorCombo    = nullptr;
    QLabel*        m_symbolPreview = nullptr;
    QPushButton*   m_applyBtn      = nullptr;

    // Fixture tipi isimleri (ComboBox sırasıyla)
    static const QStringList s_fixtureTypes;
};

} // namespace ui
} // namespace vkt
