#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <vector>
#include <string>

namespace vkt {
namespace core { class FloorManager; }
namespace ui {

/**
 * @brief "Mimari Belirle" penceresi
 *
 * FineSANI iş akışı:
 *  1. Her kat için: numara, kot, isim ve DXF/DWG mimari dosyası tanımlanır.
 *  2. "Yenile" ile listeye eklenir.
 *  3. "Tamam" ile FloorManager güncellenir ve mimari dosyalar import edilir.
 *
 * Tanımlanan kat dosyaları DXFImportDialog üzerinden arka plan layer olarak
 * yüklenir; tesisat çizimi bu katmanların üstüne yapılır.
 */
class MimariBelirleDialog : public QDialog {
    Q_OBJECT

public:
    struct FloorDef {
        int         katNo     = 1;       ///< Kat numarası (1'den başlar)
        double      kod       = 0.0;     ///< Döşeme kotu (m), örn. -3.0
        std::string isim;               ///< "Bodrum Kat", "Zemin Kat" ...
        std::string dosya;              ///< DXF/DWG dosya yolu
        /// W-Block baz noktası — katlar arasında ortak olan fiziksel noktanın
        /// bu dosyadaki CAD koordinatı (örn. kolon köşesi, asansör kenarı).
        double      refX = 0.0;
        double      refY = 0.0;
    };

    explicit MimariBelirleDialog(QWidget* parent = nullptr);

    /// Tanımlanan kat listesini döndür
    const std::vector<FloorDef>& GetFloorDefs() const { return m_defs; }

    /// FloorManager'a mevcut tanımları uygula
    void ApplyToFloorManager(core::FloorManager& fm) const;

private slots:
    void OnDosyaSec();
    void OnYenile();
    void OnSil();
    void OnRowSelected();

private:
    void BuildUI();
    void RefreshTable();
    void PopulateForm(const FloorDef& def);

    // Form alanları
    QSpinBox*       m_spnKatNo  = nullptr;
    QDoubleSpinBox* m_spnKod    = nullptr;
    QLineEdit*      m_edtIsim   = nullptr;
    QLineEdit*      m_edtDosya  = nullptr;
    QPushButton*    m_btnDosya  = nullptr;
    QDoubleSpinBox* m_spnRefX   = nullptr;  ///< W-Block referans noktası X
    QDoubleSpinBox* m_spnRefY   = nullptr;  ///< W-Block referans noktası Y
    QPushButton*    m_btnYenile = nullptr;
    QPushButton*    m_btnSil    = nullptr;

    QTableWidget*   m_table     = nullptr;

    std::vector<FloorDef> m_defs;
    int m_editRow = -1; ///< Düzenlenen satır (-1 = yeni ekleme)
};

} // namespace ui
} // namespace vkt
