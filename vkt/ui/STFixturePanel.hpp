#pragma once

#include <QWidget>
#include <QListWidget>
#include <QCheckBox>

namespace vkt {
namespace ui {

/**
 * @brief ST Cihazları (Sıhhi Tesisat) panel — TS EN 806-3 fixture kütüphanesi
 *
 * Dock widget içinde gösterilir. Her cihaz çift tıklanınca
 * FixtureSelected() sinyali fırlatılır; MainWindow bu sinyali
 * PlaceFixture modunu etkinleştirmek için kullanır.
 *
 * "Akıllı Bağlantı Noktaları" checkbox: FineSANI eşdeğeri — mimari planda
 * cihaz zaten çiziliyse, sadece pis su bağlantı sembolü (yıldız) yerleştir.
 */
class STFixturePanel : public QWidget {
    Q_OBJECT

public:
    explicit STFixturePanel(QWidget* parent = nullptr);

    /// Akıllı Bağlantı Noktaları modu aktif mi?
    bool IsSmartPointMode() const;

signals:
    void FixtureSelected(const QString& fixtureName);
    /// SmartPoint modu değiştiğinde yayınlanır
    void SmartPointModeChanged(bool enabled);

private slots:
    void OnItemActivated(QListWidgetItem* item);

private:
    void BuildUI();

    QListWidget* m_list           = nullptr;
    QCheckBox*   m_cbSmartPoint   = nullptr;
};

} // namespace ui
} // namespace vkt
