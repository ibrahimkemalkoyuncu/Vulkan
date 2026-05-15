#include "ui/MimariBelirleDialog.hpp"
#include "core/FloorManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>

namespace vkt {
namespace ui {

MimariBelirleDialog::MimariBelirleDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Mimari Belirle");
    setMinimumSize(760, 540);
    BuildUI();
}

void MimariBelirleDialog::BuildUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // ── Bilgi etiketi ───────────────────────────────────────
    auto* infoLabel = new QLabel(
        "Her kat için numara, kot değeri, isim ve mimari DXF/DWG dosyasını "
        "girin. Referans noktası, tüm katlarda ortak olan bir noktanın "
        "(kolon köşesi, asansör kenarı vb.) bu dosyadaki CAD koordinatıdır. "
        "\"Yenile\" ile listeye ekleyin. Tüm katlar tanımlandıktan sonra \"Tamam\"a basın.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // ── Giriş formu ─────────────────────────────────────────
    auto* formGroup = new QGroupBox("Kat Tanımı");
    auto* form = new QFormLayout(formGroup);

    m_spnKatNo = new QSpinBox();
    m_spnKatNo->setRange(1, 99);
    m_spnKatNo->setValue(1);
    m_spnKatNo->setToolTip("Kat numarası (1'den başlar)");
    form->addRow("Kat Numarası:", m_spnKatNo);

    m_spnKod = new QDoubleSpinBox();
    m_spnKod->setRange(-50.0, 200.0);
    m_spnKod->setValue(0.0);
    m_spnKod->setSuffix(" m");
    m_spnKod->setDecimals(2);
    m_spnKod->setToolTip("Döşeme kotu (metre). Bodrum = negatif, örn. -3.00");
    form->addRow("Kot:", m_spnKod);

    m_edtIsim = new QLineEdit();
    m_edtIsim->setPlaceholderText("örn. Bodrum Kat, Zemin Kat, 1. Kat ...");
    form->addRow("İsim:", m_edtIsim);

    auto* dosyaRow = new QHBoxLayout();
    m_edtDosya = new QLineEdit();
    m_edtDosya->setPlaceholderText("Mimari DXF/DWG dosyası...");
    m_edtDosya->setReadOnly(true);
    m_btnDosya = new QPushButton("...");
    m_btnDosya->setFixedWidth(32);
    m_btnDosya->setToolTip("DXF/DWG dosyası seç");
    dosyaRow->addWidget(m_edtDosya);
    dosyaRow->addWidget(m_btnDosya);
    form->addRow("Dosya:", dosyaRow);

    // ── Referans noktası (W-Block baz noktası) ────────────
    auto* refRow = new QHBoxLayout();
    m_spnRefX = new QDoubleSpinBox();
    m_spnRefX->setRange(-1e9, 1e9);
    m_spnRefX->setValue(0.0);
    m_spnRefX->setDecimals(3);
    m_spnRefX->setPrefix("X: ");
    m_spnRefX->setToolTip(
        "Bu kattaki DXF/DWG dosyasında ortak referans noktasının X koordinatı.\n"
        "Tüm katlarda aynı fiziksel noktanın (örn. A-B kolon köşesi) seçilmesi\n"
        "gerekir. Program bu noktaları üst üste getirerek katları hizalar.");
    m_spnRefY = new QDoubleSpinBox();
    m_spnRefY->setRange(-1e9, 1e9);
    m_spnRefY->setValue(0.0);
    m_spnRefY->setDecimals(3);
    m_spnRefY->setPrefix("Y: ");
    m_spnRefY->setToolTip(m_spnRefX->toolTip());
    refRow->addWidget(m_spnRefX);
    refRow->addWidget(m_spnRefY);
    form->addRow("Referans Noktası:", refRow);

    auto* btnRow = new QHBoxLayout();
    m_btnYenile = new QPushButton("Yenile");
    m_btnYenile->setToolTip("Girilen bilgileri listeye ekle / güncelle");
    m_btnSil = new QPushButton("Sil");
    m_btnSil->setToolTip("Seçili katı listeden kaldır");
    m_btnSil->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(m_btnYenile);
    btnRow->addWidget(m_btnSil);
    form->addRow(btnRow);

    mainLayout->addWidget(formGroup);

    // ── Kat listesi tablosu ──────────────────────────────────
    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels(
        {"Kat No", "Kot (m)", "İsim", "Ref X", "Ref Y", "Dosya"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 70);
    m_table->setColumnWidth(3, 90);
    m_table->setColumnWidth(4, 90);
    mainLayout->addWidget(m_table, 1);

    // ── Tamam / İptal ────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Tamam");
    buttons->button(QDialogButtonBox::Cancel)->setText("İptal");
    mainLayout->addWidget(buttons);

    // Bağlantılar
    connect(m_btnDosya,  &QPushButton::clicked, this, &MimariBelirleDialog::OnDosyaSec);
    connect(m_btnYenile, &QPushButton::clicked, this, &MimariBelirleDialog::OnYenile);
    connect(m_btnSil,    &QPushButton::clicked, this, &MimariBelirleDialog::OnSil);
    connect(m_table,     &QTableWidget::itemSelectionChanged,
            this, &MimariBelirleDialog::OnRowSelected);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MimariBelirleDialog::OnDosyaSec() {
    QString path = QFileDialog::getOpenFileName(
        this, "Mimari Dosya Seç", QString(),
        "CAD Dosyaları (*.dxf *.dwg);;DXF (*.dxf);;DWG (*.dwg)");
    if (!path.isEmpty())
        m_edtDosya->setText(path);
}

void MimariBelirleDialog::OnYenile() {
    if (m_edtIsim->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Eksik Bilgi", "Lütfen kat ismi girin.");
        return;
    }

    FloorDef def;
    def.katNo = m_spnKatNo->value();
    def.kod   = m_spnKod->value();
    def.isim  = m_edtIsim->text().trimmed().toStdString();
    def.dosya = m_edtDosya->text().toStdString();
    def.refX  = m_spnRefX->value();
    def.refY  = m_spnRefY->value();

    if (m_editRow >= 0 && m_editRow < (int)m_defs.size()) {
        m_defs[m_editRow] = def;
        m_editRow = -1;
    } else {
        m_defs.push_back(def);
    }

    // Form sıfırla
    m_spnKatNo->setValue(m_spnKatNo->value() + 1);
    m_edtIsim->clear();
    m_edtDosya->clear();
    // refX/Y sıfırlama: bir sonraki katın referans noktası muhtemelen aynı
    // olduğundan değerleri koruyoruz.
    m_btnSil->setEnabled(false);

    RefreshTable();
}

void MimariBelirleDialog::OnSil() {
    int row = m_table->currentRow();
    if (row < 0 || row >= (int)m_defs.size()) return;
    m_defs.erase(m_defs.begin() + row);
    m_editRow = -1;
    m_btnSil->setEnabled(false);
    RefreshTable();
}

void MimariBelirleDialog::OnRowSelected() {
    int row = m_table->currentRow();
    if (row < 0 || row >= (int)m_defs.size()) {
        m_btnSil->setEnabled(false);
        return;
    }
    m_btnSil->setEnabled(true);
    m_editRow = row;
    PopulateForm(m_defs[row]);
}

void MimariBelirleDialog::RefreshTable() {
    m_table->setRowCount(0);
    for (const auto& d : m_defs) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(d.katNo)));
        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(d.kod, 'f', 2)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(d.isim)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(d.refX, 'f', 3)));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(d.refY, 'f', 3)));
        m_table->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(d.dosya)));
    }
}

void MimariBelirleDialog::PopulateForm(const FloorDef& def) {
    m_spnKatNo->setValue(def.katNo);
    m_spnKod->setValue(def.kod);
    m_edtIsim->setText(QString::fromStdString(def.isim));
    m_edtDosya->setText(QString::fromStdString(def.dosya));
    m_spnRefX->setValue(def.refX);
    m_spnRefY->setValue(def.refY);
}

void MimariBelirleDialog::ApplyToFloorManager(core::FloorManager& fm) const {
    fm.Clear();
    for (const auto& d : m_defs) {
        core::Floor f;
        f.index       = d.katNo;
        f.label       = d.isim;
        f.elevation_m = d.kod;
        f.height_m    = 3.0; // varsayılan; ileride UI'da düzenlenebilir
        f.refX        = d.refX;
        f.refY        = d.refY;
        fm.AddFloor(f);
    }
}

} // namespace ui
} // namespace vkt
