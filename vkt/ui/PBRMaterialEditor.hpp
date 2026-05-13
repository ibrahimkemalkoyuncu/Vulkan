#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <array>
#include "mep/MaterialProperties.hpp"

namespace vkt {
namespace ui {

/**
 * @class PBRMaterialEditor
 * @brief PBR malzeme (roughness/metalness/albedo/ambient) düzenleyici paneli.
 *
 * Renderer'ın composite push-constant parametrelerini ve seçili borunun
 * malzeme özelliklerini gerçek zamanlı olarak düzenler.
 */
class PBRMaterialEditor : public QWidget {
    Q_OBJECT

public:
    explicit PBRMaterialEditor(QWidget* parent = nullptr);

    void SetCurrentMaterial(const mep::MaterialProperties& mat);
    void SetAmbient(float ambient);
    void SetLightDir(float azimuthDeg, float elevationDeg);

    float GetRoughness()   const;
    float GetMetalness()   const;
    float GetAmbient()     const;
    void  GetLightDir(float& azimuth, float& elevation) const;
    std::array<float, 3> GetBaseColor() const;

signals:
    /** Emitted whenever any PBR parameter changes. */
    void MaterialChanged(float roughness, float metalness, float ambient,
                         float r, float g, float b,
                         float lightAzimuth, float lightElevation);

private slots:
    void OnPresetChanged(int index);
    void OnColorButtonClicked();
    void OnAnySliderChanged();

private:
    void UpdateColorSwatch();
    void BuildUI();
    void EmitChange();

    QComboBox*   m_presetBox      = nullptr;
    QToolButton* m_colorButton    = nullptr;
    QLabel*      m_colorLabel     = nullptr;

    QSlider*     m_roughnessSlider = nullptr;
    QSlider*     m_metalnessSlider = nullptr;
    QSlider*     m_ambientSlider   = nullptr;
    QSlider*     m_azimuthSlider   = nullptr;
    QSlider*     m_elevSlider      = nullptr;

    QLabel*      m_roughnessVal    = nullptr;
    QLabel*      m_metalnessVal    = nullptr;
    QLabel*      m_ambientVal      = nullptr;
    QLabel*      m_azimuthVal      = nullptr;
    QLabel*      m_elevVal         = nullptr;

    std::array<float, 3> m_baseColor{0.7f, 0.7f, 0.7f};
};

} // namespace ui
} // namespace vkt
