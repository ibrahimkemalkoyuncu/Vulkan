/**
 * @file SymbolPalettePanel.hpp
 * @brief İnteraktif armatür sembol paleti — kategori + önizleme + yerleştirme
 *
 * FineSANI'nin "Blok Kütüphanesi" paneline denk. QDockWidget içinde gösterilir.
 * SVG önizleme thumbnail'leri olan kategori listesi sunar; çift tıklama veya
 * "Yerleştir" butonu ile PlaceFixture moduna geçer.
 */

#pragma once

#include <QWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <functional>
#include "cad/FixtureSymbolLibrary.hpp"

namespace vkt {
namespace ui {

class SymbolPalettePanel : public QWidget {
    Q_OBJECT
public:
    explicit SymbolPalettePanel(QWidget* parent = nullptr);

    using PlaceCallback = std::function<void(const std::string& fixtureType)>;
    void SetPlaceCallback(PlaceCallback cb) { m_placeCallback = std::move(cb); }

    /// Özel sembol JSON dizinini yükle (varsa)
    void LoadCustomSymbols(const std::string& dir);

signals:
    void FixtureTypeSelected(const QString& fixtureType);

private slots:
    void OnItemDoubleClicked(QListWidgetItem* item);
    void OnPlaceClicked();
    void OnLoadCustom();

private:
    void BuildCategoryTabs();
    void AddSymbolsToList(QListWidget* list,
                          const std::vector<cad::FixtureSymbolType>& types);
    QIcon MakeSymbolIcon(cad::FixtureSymbolType type);

    QTabWidget*  m_tabs      = nullptr;
    QPushButton* m_btnPlace  = nullptr;
    QPushButton* m_btnCustom = nullptr;
    QLabel*      m_lblHint   = nullptr;

    // Currently selected fixture type
    QString m_selectedType;
    PlaceCallback m_placeCallback;

    static constexpr int kIconSize = 48;
};

} // namespace ui
} // namespace vkt
