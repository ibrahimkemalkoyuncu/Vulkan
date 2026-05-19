/**
 * @file DevreSecenekleriDialog.hpp
 * @brief Devre Seçenekleri — boru cinsi, pürüzlülük, max hız, norm (FineSANI eşdeğeri)
 */

#pragma once

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

namespace vkt {
namespace ui {

/**
 * Hesap parametreleri: tek yerden norm + malzeme + hız limiti ayarı.
 * FineSANI "Devre Seçenekleri" penceresine karşılık gelir.
 */
struct DevreParams {
    QString buildingType   = "Konut";
    QString mainPipeMat    = "PPR";
    QString branchPipeMat  = "PPR";
    double  roughness_mm   = 0.007;   // PPR varsayılan
    double  maxVelocity_ms = 2.0;
    QString norm           = "TS EN 806-3";  // veya "DIN 1988-300"
};

class DevreSecenekleriDialog : public QDialog {
    Q_OBJECT
public:
    explicit DevreSecenekleriDialog(const DevreParams& current, QWidget* parent = nullptr);

    DevreParams GetParams() const;

private:
    QComboBox*      m_cbBuilding    = nullptr;
    QComboBox*      m_cbMainPipe    = nullptr;
    QComboBox*      m_cbBranchPipe  = nullptr;
    QDoubleSpinBox* m_spRoughness   = nullptr;
    QDoubleSpinBox* m_spMaxVelocity = nullptr;
    QComboBox*      m_cbNorm        = nullptr;

    void OnMainPipeChanged(const QString& mat);
};

} // namespace ui
} // namespace vkt
