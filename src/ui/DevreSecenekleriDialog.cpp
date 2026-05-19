#include "ui/DevreSecenekleriDialog.hpp"

#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QMap>

namespace vkt {
namespace ui {

// Malzeme → pürüzlülük eşlemesi (mm)
static const QMap<QString, double> kRoughness = {
    {"PVC",    0.0015},
    {"PPR",    0.007},
    {"Bakir",  0.0015},
    {"Galvaniz", 0.15},
    {"PE",     0.007},
};

DevreSecenekleriDialog::DevreSecenekleriDialog(const DevreParams& cur, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Devre Seçenekleri");
    setMinimumWidth(380);

    auto* form = new QFormLayout;

    // Bina tipi
    m_cbBuilding = new QComboBox;
    m_cbBuilding->addItems({"Konut", "Otel", "Endüstri"});
    m_cbBuilding->setCurrentText(cur.buildingType);
    form->addRow("Bina Tipi:", m_cbBuilding);

    // Ana boru cinsi
    m_cbMainPipe = new QComboBox;
    m_cbMainPipe->addItems({"PPR", "PVC", "Bakir", "Galvaniz", "PE"});
    m_cbMainPipe->setCurrentText(cur.mainPipeMat);
    form->addRow("Ana Boru Cinsi:", m_cbMainPipe);

    // İkincil boru cinsi
    m_cbBranchPipe = new QComboBox;
    m_cbBranchPipe->addItems({"PPR", "PVC", "Bakir", "Galvaniz", "PE"});
    m_cbBranchPipe->setCurrentText(cur.branchPipeMat);
    form->addRow("İkincil Boru Cinsi:", m_cbBranchPipe);

    // Pürüzlülük
    m_spRoughness = new QDoubleSpinBox;
    m_spRoughness->setRange(0.0001, 5.0);
    m_spRoughness->setDecimals(4);
    m_spRoughness->setSingleStep(0.001);
    m_spRoughness->setSuffix(" mm");
    m_spRoughness->setValue(cur.roughness_mm);
    form->addRow("Boru Pürüzlülüğü:", m_spRoughness);

    // Maksimum hız
    m_spMaxVelocity = new QDoubleSpinBox;
    m_spMaxVelocity->setRange(0.5, 5.0);
    m_spMaxVelocity->setDecimals(1);
    m_spMaxVelocity->setSingleStep(0.5);
    m_spMaxVelocity->setSuffix(" m/s");
    m_spMaxVelocity->setValue(cur.maxVelocity_ms);
    form->addRow("Maks. Su Hızı:", m_spMaxVelocity);

    // Norm
    m_cbNorm = new QComboBox;
    m_cbNorm->addItems({"TS EN 806-3", "DIN 1988-300"});
    m_cbNorm->setCurrentText(cur.norm);
    form->addRow("Hesap Normu:", m_cbNorm);

    // Açıklama
    auto* note = new QLabel(
        "<small><i>Ana boru cinsi değiştiğinde pürüzlülük otomatik güncellenir.<br>"
        "Onaylamak için Tamam'a basın — sistem otomatik yeniden hesaplar.</i></small>");
    note->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(note);
    root->addWidget(buttons);

    // Ana boru değişince pürüzlülüğü otomatik güncelle
    connect(m_cbMainPipe, &QComboBox::currentTextChanged,
            this, &DevreSecenekleriDialog::OnMainPipeChanged);
}

void DevreSecenekleriDialog::OnMainPipeChanged(const QString& mat)
{
    auto it = kRoughness.find(mat);
    if (it != kRoughness.end())
        m_spRoughness->setValue(it.value());
}

DevreParams DevreSecenekleriDialog::GetParams() const
{
    DevreParams p;
    p.buildingType   = m_cbBuilding->currentText();
    p.mainPipeMat    = m_cbMainPipe->currentText();
    p.branchPipeMat  = m_cbBranchPipe->currentText();
    p.roughness_mm   = m_spRoughness->value();
    p.maxVelocity_ms = m_spMaxVelocity->value();
    p.norm           = m_cbNorm->currentText();
    return p;
}

} // namespace ui
} // namespace vkt
