#include "ui/MimariBelirleDialog.hpp"
#include "core/FloorManager.hpp"
#include "core/ProjectManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QFileInfo>

namespace vkt {
namespace ui {

MimariBelirleDialog::MimariBelirleDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Mimari Belirle");
    setMinimumSize(760, 580);
    BuildUI();
}

void MimariBelirleDialog::BuildUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // ── Bilgi etiketi ───────────────────────────────────────
    auto* infoLabel = new QLabel(
        "Her kat için numara, kot değeri, isim ve mimari DXF/DWG dosyasını girin. "
        "Kat numarası sıralı bir tanımlayıcıdır ve her zaman 1'den başlar; "
        "\"Kod\" alanı ise projedeki gerçek döşeme kotunu (metre) belirtir ve "
        "istediğiniz değeri girebilirsiniz. "
        "Örnek: Kat No = 1  |  Kod = -1.00  |  İsim = \"Bodrum Kat\". "
        "\"Yenile\" ile listeye ekleyin, tüm katları girdikten sonra \"Tamam\"a basın.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // ── Global Referans Noktası (W-Block baz noktası) ───────
    auto* refGroup = new QGroupBox("Global Referans Noktası (W-Block Baz Noktası)");
    auto* refForm  = new QFormLayout(refGroup);

    auto* refInfoLabel = new QLabel(
        "Tüm katlarda ortak olan bir noktanın (kolon köşesi, asansör kenarı vb.) "
        "DXF/DWG koordinatını girin. <b>Her katta aynı fiziksel noktanın seçilmesi zorunludur.</b> "
        "Program bu koordinatı tüm katların origin'ine (0,0) eşler; "
        "3D görünümde katlar bu sayede hizalanır.");
    refInfoLabel->setWordWrap(true);
    refForm->addRow(refInfoLabel);

    auto* refXYRow = new QHBoxLayout();
    m_spnGlobalRefX = new QDoubleSpinBox();
    m_spnGlobalRefX->setRange(-1e9, 1e9);
    m_spnGlobalRefX->setValue(0.0);
    m_spnGlobalRefX->setDecimals(3);
    m_spnGlobalRefX->setPrefix("X: ");
    m_spnGlobalRefX->setMinimumWidth(180);
    m_spnGlobalRefX->setToolTip("Tüm kat dosyalarında bu X koordinatı (0,0)'a eşlenir.");
    m_spnGlobalRefY = new QDoubleSpinBox();
    m_spnGlobalRefY->setRange(-1e9, 1e9);
    m_spnGlobalRefY->setValue(0.0);
    m_spnGlobalRefY->setDecimals(3);
    m_spnGlobalRefY->setPrefix("Y: ");
    m_spnGlobalRefY->setMinimumWidth(180);
    m_spnGlobalRefY->setToolTip("Tüm kat dosyalarında bu Y koordinatı (0,0)'a eşlenir.");
    refXYRow->addWidget(m_spnGlobalRefX);
    refXYRow->addWidget(m_spnGlobalRefY);
    refXYRow->addStretch();
    refForm->addRow("Referans Koordinatı:", refXYRow);
    mainLayout->addWidget(refGroup);

    // ── Giriş formu ─────────────────────────────────────────
    auto* formGroup = new QGroupBox("Kat Tanımı");
    auto* form = new QFormLayout(formGroup);

    m_spnKatNo = new QSpinBox();
    m_spnKatNo->setRange(1, 99);
    m_spnKatNo->setValue(1);
    m_spnKatNo->setToolTip(
        "Sıralı kat tanımlayıcısı — program gereği her zaman 1'den başlar.\n"
        "Bu numara gerçek kat adını etkilemez; bodrum katı da 1 olarak girilebilir.\n"
        "Örn: Kat No=1, Kod=-1.00, İsim=\"Bodrum Kat\"");
    form->addRow("Kat Numarası:", m_spnKatNo);

    m_spnKod = new QDoubleSpinBox();
    m_spnKod->setRange(-50.0, 200.0);
    m_spnKod->setValue(0.0);
    m_spnKod->setSuffix(" m");
    m_spnKod->setDecimals(2);
    m_spnKod->setToolTip(
        "Döşeme kotu (metre) — istediğiniz değeri girebilirsiniz.\n"
        "Kat numarasından bağımsızdır.\n"
        "Bodrum: negatif değer  (örn. -1.00, -3.00)\n"
        "Zemin:  0.00\n"
        "1. Kat: +3.00  (zemin yüksekliğine göre değişir)");
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

    // İpucu satırı
    auto* hintLabel = new QLabel(
        "<i>İpucu: Kat No=1 / Kod=−1.00 / İsim=\"Bodrum Kat\" gibi bodrum kodu da girilebilir.</i>");
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("QLabel { color: #555; font-size: 11px; }");
    form->addRow(hintLabel);

    // Proje klasörüne kopyalama seçeneği
    m_chkKopyala = new QCheckBox("Dosyayı proje mimari/ klasörüne kopyala");
    m_chkKopyala->setToolTip(
        "İşaretlendiğinde, seçilen DXF/DWG dosyası proje klasörünün\n"
        "\"mimari/\" alt dizinine kopyalanır ve dosya yolu güncellenir.\n"
        "Aktif proje yoksa bu seçenek pasiftir.");
    bool hasProject = core::ProjectManager::Instance().HasActiveProject();
    m_chkKopyala->setEnabled(hasProject);
    m_chkKopyala->setChecked(hasProject);
    form->addRow(m_chkKopyala);

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
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({"Kat No", "Kot (m)", "İsim", "Dosya"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 70);
    mainLayout->addWidget(m_table, 1);

    // ── Tamam / İptal ────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Tamam");
    buttons->button(QDialogButtonBox::Cancel)->setText("İptal");
    mainLayout->addWidget(buttons);

    connect(m_btnDosya,  &QPushButton::clicked, this, &MimariBelirleDialog::OnDosyaSec);
    connect(m_btnYenile, &QPushButton::clicked, this, &MimariBelirleDialog::OnYenile);
    connect(m_btnSil,    &QPushButton::clicked, this, &MimariBelirleDialog::OnSil);
    connect(m_table,     &QTableWidget::itemSelectionChanged,
            this, &MimariBelirleDialog::OnRowSelected);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

double MimariBelirleDialog::GetGlobalRefX() const {
    return m_spnGlobalRefX ? m_spnGlobalRefX->value() : 0.0;
}

double MimariBelirleDialog::GetGlobalRefY() const {
    return m_spnGlobalRefY ? m_spnGlobalRefY->value() : 0.0;
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

    // Dosyayı proje mimari/ klasörüne kopyala (seçenek işaretliyse)
    QString srcPath = m_edtDosya->text();
    if (m_chkKopyala && m_chkKopyala->isChecked() &&
        !srcPath.isEmpty() && QFile::exists(srcPath)) {
        auto& pm = core::ProjectManager::Instance();
        if (pm.HasActiveProject()) {
            QString mimariDir = QString::fromStdString(pm.GetMimariFolder());
            QDir().mkpath(mimariDir);
            QFileInfo fi(srcPath);
            QString destPath = mimariDir + "/" + fi.fileName();
            if (destPath != srcPath) {
                if (QFile::exists(destPath)) QFile::remove(destPath);
                if (QFile::copy(srcPath, destPath)) {
                    srcPath = destPath;
                    m_edtDosya->setText(destPath);
                }
            }
        }
    }
    def.dosya = srcPath.toStdString();

    if (m_editRow >= 0 && m_editRow < (int)m_defs.size()) {
        m_defs[m_editRow] = def;
        m_editRow = -1;
    } else {
        m_defs.push_back(def);
    }

    m_spnKatNo->setValue(m_spnKatNo->value() + 1);
    m_edtIsim->clear();
    m_edtDosya->clear();
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
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(d.dosya)));
    }
}

void MimariBelirleDialog::PopulateForm(const FloorDef& def) {
    m_spnKatNo->setValue(def.katNo);
    m_spnKod->setValue(def.kod);
    m_edtIsim->setText(QString::fromStdString(def.isim));
    m_edtDosya->setText(QString::fromStdString(def.dosya));
}

void MimariBelirleDialog::ApplyToFloorManager(core::FloorManager& fm) const {
    fm.Clear();
    for (const auto& d : m_defs) {
        core::Floor f;
        f.index       = d.katNo;
        f.label       = d.isim;
        f.elevation_m = d.kod;
        f.height_m    = 3.0;
        // Global referans noktası tüm katlara uygulanır
        f.refX = m_spnGlobalRefX ? m_spnGlobalRefX->value() : 0.0;
        f.refY = m_spnGlobalRefY ? m_spnGlobalRefY->value() : 0.0;
        fm.AddFloor(f);
    }
}

} // namespace ui
} // namespace vkt
