/**
 * @file PBRMaterialEditor.cpp
 * @brief PBR malzeme düzenleyici paneli implementasyonu
 */

#include "ui/PBRMaterialEditor.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QColor>
#include <QString>
#include <cmath>

namespace vkt {
namespace ui {

static constexpr int SLIDER_MAX = 1000; // maps 0.0..1.0 in 0.001 steps

PBRMaterialEditor::PBRMaterialEditor(QWidget* parent)
    : QWidget(parent)
{
    BuildUI();
}

void PBRMaterialEditor::BuildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    // ── Preset row ───────────────────────────────────────────────────────────
    auto* presetGroup = new QGroupBox("Malzeme Ön Ayarı", this);
    auto* presetLayout = new QHBoxLayout(presetGroup);
    m_presetBox = new QComboBox(this);
    m_presetBox->addItem("Özel");
    m_presetBox->addItem("Bakır");
    m_presetBox->addItem("Çelik");
    m_presetBox->addItem("Paslanmaz");
    m_presetBox->addItem("Dökme Demir");
    m_presetBox->addItem("PPR");
    m_presetBox->addItem("PVC");
    m_presetBox->addItem("HDPE");
    m_presetBox->addItem("Galvaniz");
    presetLayout->addWidget(m_presetBox);
    root->addWidget(presetGroup);

    // ── Base color ───────────────────────────────────────────────────────────
    auto* colorGroup  = new QGroupBox("Taban Rengi", this);
    auto* colorLayout = new QHBoxLayout(colorGroup);
    m_colorButton = new QToolButton(this);
    m_colorButton->setFixedSize(36, 24);
    m_colorLabel  = new QLabel("RGB", this);
    colorLayout->addWidget(m_colorButton);
    colorLayout->addWidget(m_colorLabel);
    colorLayout->addStretch();
    root->addWidget(colorGroup);

    // ── PBR sliders ──────────────────────────────────────────────────────────
    auto* pbrGroup  = new QGroupBox("PBR Parametreleri", this);
    auto* pbrGrid   = new QGridLayout(pbrGroup);
    pbrGrid->setSpacing(4);

    auto makeSlider = [&](const QString& label, QSlider*& slider, QLabel*& valLabel, int row) {
        pbrGrid->addWidget(new QLabel(label, this), row, 0);
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, SLIDER_MAX);
        slider->setValue(SLIDER_MAX / 2);
        pbrGrid->addWidget(slider, row, 1);
        valLabel = new QLabel("0.500", this);
        valLabel->setFixedWidth(42);
        pbrGrid->addWidget(valLabel, row, 2);
    };

    makeSlider("Pürüzlülük",   m_roughnessSlider, m_roughnessVal, 0);
    makeSlider("Metaliklik",   m_metalnessSlider, m_metalnessVal, 1);
    makeSlider("Ortam ışığı",  m_ambientSlider,   m_ambientVal,   2);
    m_ambientSlider->setValue(static_cast<int>(0.25f * SLIDER_MAX));
    root->addWidget(pbrGroup);

    // ── Light direction ───────────────────────────────────────────────────────
    auto* lightGroup = new QGroupBox("Işık Yönü", this);
    auto* lightGrid  = new QGridLayout(lightGroup);
    lightGrid->setSpacing(4);

    lightGrid->addWidget(new QLabel("Azimut (°)",  this), 0, 0);
    m_azimuthSlider = new QSlider(Qt::Horizontal, this);
    m_azimuthSlider->setRange(0, 3600); // 0.1° steps
    m_azimuthSlider->setValue(450);     // 45°
    lightGrid->addWidget(m_azimuthSlider, 0, 1);
    m_azimuthVal = new QLabel("45.0°", this);
    m_azimuthVal->setFixedWidth(50);
    lightGrid->addWidget(m_azimuthVal, 0, 2);

    lightGrid->addWidget(new QLabel("Yükselme (°)", this), 1, 0);
    m_elevSlider = new QSlider(Qt::Horizontal, this);
    m_elevSlider->setRange(0, 900);   // 0.1° steps → 0–90°
    m_elevSlider->setValue(350);      // 35°
    lightGrid->addWidget(m_elevSlider, 1, 1);
    m_elevVal = new QLabel("35.0°", this);
    m_elevVal->setFixedWidth(50);
    lightGrid->addWidget(m_elevVal, 1, 2);

    root->addWidget(lightGroup);
    root->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_presetBox,       QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,              &PBRMaterialEditor::OnPresetChanged);
    connect(m_colorButton,     &QToolButton::clicked,
            this,              &PBRMaterialEditor::OnColorButtonClicked);
    connect(m_roughnessSlider, &QSlider::valueChanged, this, &PBRMaterialEditor::OnAnySliderChanged);
    connect(m_metalnessSlider, &QSlider::valueChanged, this, &PBRMaterialEditor::OnAnySliderChanged);
    connect(m_ambientSlider,   &QSlider::valueChanged, this, &PBRMaterialEditor::OnAnySliderChanged);
    connect(m_azimuthSlider,   &QSlider::valueChanged, this, &PBRMaterialEditor::OnAnySliderChanged);
    connect(m_elevSlider,      &QSlider::valueChanged, this, &PBRMaterialEditor::OnAnySliderChanged);

    UpdateColorSwatch();
}

// ── Public API ───────────────────────────────────────────────────────────────

void PBRMaterialEditor::SetCurrentMaterial(const mep::MaterialProperties& mat) {
    m_baseColor = {mat.baseColor[0], mat.baseColor[1], mat.baseColor[2]};

    // Block signals while we set programmatically
    m_roughnessSlider->blockSignals(true);
    m_metalnessSlider->blockSignals(true);
    m_roughnessSlider->setValue(static_cast<int>(mat.roughness * SLIDER_MAX));
    m_metalnessSlider->setValue(static_cast<int>(mat.metallic  * SLIDER_MAX));
    m_roughnessSlider->blockSignals(false);
    m_metalnessSlider->blockSignals(false);

    UpdateColorSwatch();
    OnAnySliderChanged();  // update labels + emit
}

void PBRMaterialEditor::SetAmbient(float ambient) {
    m_ambientSlider->blockSignals(true);
    m_ambientSlider->setValue(static_cast<int>(ambient * SLIDER_MAX));
    m_ambientSlider->blockSignals(false);
    OnAnySliderChanged();
}

void PBRMaterialEditor::SetLightDir(float azimuthDeg, float elevationDeg) {
    m_azimuthSlider->blockSignals(true);
    m_elevSlider->blockSignals(true);
    m_azimuthSlider->setValue(static_cast<int>(azimuthDeg  * 10.0f));
    m_elevSlider->setValue(   static_cast<int>(elevationDeg * 10.0f));
    m_azimuthSlider->blockSignals(false);
    m_elevSlider->blockSignals(false);
    OnAnySliderChanged();
}

float PBRMaterialEditor::GetRoughness() const {
    return static_cast<float>(m_roughnessSlider->value()) / SLIDER_MAX;
}

float PBRMaterialEditor::GetMetalness() const {
    return static_cast<float>(m_metalnessSlider->value()) / SLIDER_MAX;
}

float PBRMaterialEditor::GetAmbient() const {
    return static_cast<float>(m_ambientSlider->value()) / SLIDER_MAX;
}

void PBRMaterialEditor::GetLightDir(float& azimuth, float& elevation) const {
    azimuth   = static_cast<float>(m_azimuthSlider->value()) / 10.0f;
    elevation = static_cast<float>(m_elevSlider->value())    / 10.0f;
}

std::array<float, 3> PBRMaterialEditor::GetBaseColor() const {
    return m_baseColor;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void PBRMaterialEditor::OnPresetChanged(int index) {
    if (index == 0) return; // "Özel" — don't overwrite

    static const char* presetNames[] = {
        nullptr, "Bakır", "Çelik", "Paslanmaz", "Dökme Demir",
        "PPR", "PVC", "HDPE", "Galvaniz"
    };
    if (index < 0 || index >= 9) return;

    mep::MaterialProperties mp = mep::GetPipeMaterial(presetNames[index]);
    m_baseColor = {mp.baseColor[0], mp.baseColor[1], mp.baseColor[2]};

    m_roughnessSlider->blockSignals(true);
    m_metalnessSlider->blockSignals(true);
    m_roughnessSlider->setValue(static_cast<int>(mp.roughness * SLIDER_MAX));
    m_metalnessSlider->setValue(static_cast<int>(mp.metallic  * SLIDER_MAX));
    m_roughnessSlider->blockSignals(false);
    m_metalnessSlider->blockSignals(false);

    UpdateColorSwatch();
    EmitChange();
}

void PBRMaterialEditor::OnColorButtonClicked() {
    QColor initial(
        static_cast<int>(m_baseColor[0] * 255),
        static_cast<int>(m_baseColor[1] * 255),
        static_cast<int>(m_baseColor[2] * 255));
    QColor chosen = QColorDialog::getColor(initial, this, "Taban Rengi Seç");
    if (!chosen.isValid()) return;

    m_baseColor[0] = chosen.redF();
    m_baseColor[1] = chosen.greenF();
    m_baseColor[2] = chosen.blueF();

    m_presetBox->setCurrentIndex(0); // switch to "Özel"
    UpdateColorSwatch();
    EmitChange();
}

void PBRMaterialEditor::OnAnySliderChanged() {
    // Update value labels
    m_roughnessVal->setText(QString::number(GetRoughness(), 'f', 3));
    m_metalnessVal->setText(QString::number(GetMetalness(), 'f', 3));
    m_ambientVal->setText(  QString::number(GetAmbient(),   'f', 3));

    float az, el;
    GetLightDir(az, el);
    m_azimuthVal->setText(QString::number(static_cast<double>(az), 'f', 1) + "°");
    m_elevVal->setText(   QString::number(static_cast<double>(el), 'f', 1) + "°");

    EmitChange();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PBRMaterialEditor::UpdateColorSwatch() {
    int r = static_cast<int>(m_baseColor[0] * 255);
    int g = static_cast<int>(m_baseColor[1] * 255);
    int b = static_cast<int>(m_baseColor[2] * 255);
    m_colorButton->setStyleSheet(
        QString("background-color: rgb(%1,%2,%3); border: 1px solid #555;")
            .arg(r).arg(g).arg(b));
    m_colorLabel->setText(QString("rgb(%1, %2, %3)").arg(r).arg(g).arg(b));
}

void PBRMaterialEditor::EmitChange() {
    float az, el;
    GetLightDir(az, el);
    emit MaterialChanged(
        GetRoughness(), GetMetalness(), GetAmbient(),
        m_baseColor[0], m_baseColor[1], m_baseColor[2],
        az, el);
}

} // namespace ui
} // namespace vkt
