#include "ui/STFixturePanel.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QFont>

namespace vkt {
namespace ui {

struct STEntry {
    const char* name;
    double      lu;
    const char* note;
};

// TS EN 806-3 Table 3 — supply load units (LU)
static const STEntry k_stEntries[] = {
    {"Lavabo",             0.5,  "Lavabo (tek musluklu)"},
    {"Duş",                1.0,  "Duş/el duşu"},
    {"WC",                 2.0,  "Alçak rezervuarlı klozet"},
    {"Küvet",              3.0,  "Banyo küveti"},
    {"Evye",               1.5,  "Mutfak/çalışma evyesi"},
    {"Bulaşık Makinesi",   1.5,  "Ev tipi bulaşık makinesi"},
    {"Çamaşır Makinesi",   2.0,  "Ev tipi çamaşır makinesi"},
    {"Pisuar",             0.5,  "Pisuar (kendiliğinden kapanan vana)"},
    {"Bide",               0.5,  "Bide"},
};

STFixturePanel::STFixturePanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUI();
}

void STFixturePanel::BuildUI()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(6);

    auto* hdr = new QLabel(
        "<b>ST Cihazları</b> — Sıhhi Tesisat<br>"
        "<small>Standart: TS EN 806-3, Tablo 3</small>");
    hdr->setWordWrap(true);
    lay->addWidget(hdr);

    m_list = new QListWidget();
    m_list->setAlternatingRowColors(true);
    m_list->setToolTip("Cihazı çift tıklayarak çizim alanına yerleştirin");
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    QFont monoFont = m_list->font();
    monoFont.setFamily("Consolas");

    for (const auto& e : k_stEntries) {
        QString label = QString("%-22 LU: %2")
            .arg(e.name, -22)
            .arg(e.lu, 3, 'f', 1);
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, QString(e.name));
        item->setToolTip(QString("%1\nYük Birimi (LU): %2\n%3")
            .arg(e.name).arg(e.lu, 0, 'f', 1).arg(e.note));
        m_list->addItem(item);
    }

    lay->addWidget(m_list, 1);

    auto* hint = new QLabel(
        "<small><i>Çift tıkla → yerleştirme modunu etkinleştir</i></small>");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("QLabel { color: #888; }");
    lay->addWidget(hint);

    connect(m_list, &QListWidget::itemDoubleClicked,
            this,   &STFixturePanel::OnItemActivated);
}

void STFixturePanel::OnItemActivated(QListWidgetItem* item)
{
    if (item)
        emit FixtureSelected(item->data(Qt::UserRole).toString());
}

} // namespace ui
} // namespace vkt
