#pragma once

#include <QWidget>
#include <QListWidget>

namespace vkt {
namespace ui {

/**
 * @brief ST Cihazları (Sıhhi Tesisat) panel — TS EN 806-3 fixture kütüphanesi
 *
 * Dock widget içinde gösterilir. Her cihaz çift tıklanınca
 * FixtureSelected() sinyali fırlatılır; MainWindow bu sinyali
 * PlaceFixture modunu etkinleştirmek için kullanır.
 */
class STFixturePanel : public QWidget {
    Q_OBJECT

public:
    explicit STFixturePanel(QWidget* parent = nullptr);

signals:
    void FixtureSelected(const QString& fixtureName);

private slots:
    void OnItemActivated(QListWidgetItem* item);

private:
    void BuildUI();

    QListWidget* m_list = nullptr;
};

} // namespace ui
} // namespace vkt
