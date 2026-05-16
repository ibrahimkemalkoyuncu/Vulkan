#include "ui/FloorAlignmentDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QColor>
#include <cmath>

namespace vkt::ui {

FloorAlignmentDialog::FloorAlignmentDialog(core::FloorManager& fm,
                                            const mep::NetworkGraph& network,
                                            QWidget* parent)
    : QDialog(parent)
    , m_fm(fm)
    , m_network(network)
{
    setWindowTitle("3D Hizalama Kontrolü");
    setMinimumSize(700, 400);
    BuildUI();
    Populate();
    HighlightIssues();
}

void FloorAlignmentDialog::BuildUI() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* infoLbl = new QLabel(
        "Her kat için Z kotunu, atanan node sayısını ve durumu aşağıda görüntüleyebilirsiniz. "
        "\"Kat Yüksekliği\" sütununu çift tıklayarak düzenleyebilirsiniz. "
        "Kırmızı satırlar sorunlu katları belirtir (çakışan kot, boş kat).");
    infoLbl->setWordWrap(true);
    mainLayout->addWidget(infoLbl);

    // Tablo: Kat No | İsim | Kot (m) | Yükseklik (m) | Node Sayısı | Durum
    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({
        "Kat No", "İsim", "Kot (m)", "Kat Yük. (m)", "Node Sayısı", "Durum"
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 130);
    m_table->setColumnWidth(2, 80);
    m_table->setColumnWidth(3, 90);
    m_table->setColumnWidth(4, 90);
    mainLayout->addWidget(m_table, 1);

    m_lblSummary = new QLabel();
    m_lblSummary->setWordWrap(true);
    mainLayout->addWidget(m_lblSummary);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Uygula ve Kapat");
    buttons->button(QDialogButtonBox::Cancel)->setText("İptal");
    mainLayout->addWidget(buttons);

    connect(m_table, &QTableWidget::cellChanged, this, &FloorAlignmentDialog::OnCellChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { ApplyChanges(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FloorAlignmentDialog::Populate() {
    m_ignoreChange = true;
    m_table->setRowCount(0);

    const auto& floors = m_fm.GetFloors();
    const auto& nodes  = m_network.GetNodeMap();

    // Her kat için kaç node var?
    auto nodeCountForFloor = [&](double elevation_m) -> int {
        int count = 0;
        for (const auto& [id, nd] : nodes) {
            double z = nd.position.z;   // metre olarak
            if (std::abs(z - elevation_m) < 0.15) ++count;
        }
        return count;
    };

    for (const auto& fl : floors) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        int nc = nodeCountForFloor(fl.elevation_m);

        auto makeItem = [](const QString& txt, bool editable = false) {
            auto* it = new QTableWidgetItem(txt);
            if (!editable) it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };

        m_table->setItem(row, 0, makeItem(QString::number(fl.index)));
        m_table->setItem(row, 1, makeItem(QString::fromStdString(fl.label)));
        m_table->setItem(row, 2, makeItem(QString::number(fl.elevation_m, 'f', 2)));
        m_table->setItem(row, 3, makeItem(QString::number(fl.height_m, 'f', 2), true)); // düzenlenebilir
        m_table->setItem(row, 4, makeItem(QString::number(nc)));
        m_table->setItem(row, 5, makeItem(nc == 0 ? "Boş (node yok)" : "OK"));
    }

    // Atanamamış node sayısı (hiçbir kata yakın olmayan)
    int unassigned = 0;
    for (const auto& [id, nd] : nodes) {
        bool assigned = false;
        for (const auto& fl : floors) {
            if (std::abs(nd.position.z - fl.elevation_m) < 0.15) { assigned = true; break; }
        }
        if (!assigned) ++unassigned;
    }

    QString summary;
    if (floors.empty()) {
        summary = "Henüz kat tanımlanmamış. Mimari Belirle (Ctrl+M) ile kat ekleyin.";
    } else {
        summary = QString("%1 kat tanımlı, toplam %2 node")
            .arg(floors.size()).arg(nodes.size());
        if (unassigned > 0)
            summary += QString(" — <b style='color:red;'>%1 node hiçbir kata atanamadı "
                                "(kot farkı > 0.15 m)</b>").arg(unassigned);
    }
    m_lblSummary->setText(summary);

    m_ignoreChange = false;
}

void FloorAlignmentDialog::HighlightIssues() {
    int rows = m_table->rowCount();
    QColor red(255, 200, 200);
    QColor yellow(255, 255, 180);

    // Çakışan kot kontrolü
    for (int r = 0; r < rows; ++r) {
        double kotR = m_table->item(r, 2)->text().toDouble();
        for (int r2 = r + 1; r2 < rows; ++r2) {
            double kotR2 = m_table->item(r2, 2)->text().toDouble();
            if (std::abs(kotR - kotR2) < 0.01) {
                for (int c = 0; c < 6; ++c) {
                    if (m_table->item(r,  c)) m_table->item(r,  c)->setBackground(red);
                    if (m_table->item(r2, c)) m_table->item(r2, c)->setBackground(red);
                }
                m_table->item(r,  5)->setText("HATA: Kot çakışıyor");
                m_table->item(r2, 5)->setText("HATA: Kot çakışıyor");
            }
        }
        // Boş kat sarı
        if (m_table->item(r, 4)->text().toInt() == 0) {
            for (int c = 0; c < 6; ++c)
                if (m_table->item(r, c)) m_table->item(r, c)->setBackground(yellow);
        }
    }
}

void FloorAlignmentDialog::OnCellChanged(int row, int col) {
    if (m_ignoreChange || col != 3) return;  // sadece "Kat Yüksekliği" sütunu
    auto* item = m_table->item(row, col);
    if (!item) return;
    bool ok = false;
    double val = item->text().toDouble(&ok);
    if (!ok || val <= 0.0) {
        item->setText("3.00");
        val = 3.0;
    }
    // Güncel kat listesinde bu satırı güncelle (ApplyChanges'a kadar FloorManager dokunmuyoruz)
}

void FloorAlignmentDialog::ApplyChanges() {
    auto floors = m_fm.GetFloors();
    int rows = m_table->rowCount();
    for (int r = 0; r < rows && r < (int)floors.size(); ++r) {
        auto* heightItem = m_table->item(r, 3);
        if (!heightItem) continue;
        bool ok = false;
        double h = heightItem->text().toDouble(&ok);
        if (ok && h > 0.0) floors[r].height_m = h;
    }
    m_fm.Clear();
    for (const auto& f : floors)
        m_fm.AddFloor(f);
}

} // namespace vkt::ui
