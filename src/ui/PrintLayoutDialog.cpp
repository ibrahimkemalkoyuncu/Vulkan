/**
 * @file PrintLayoutDialog.cpp
 * @brief Pafta diyaloğu implementasyonu
 */

#include "ui/PrintLayoutDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDate>
#include <QDir>
#include <QString>
#include <QPixmap>
#include <QImageReader>
#include <QFileInfo>

namespace vkt {
namespace ui {

PrintLayoutDialog::PrintLayoutDialog(const PrintLayout& layout,
                                     const std::string& projectsRoot,
                                     QWidget* parent)
    : QDialog(parent)
    , m_baseLayout(layout)
    , m_projectsRoot(projectsRoot)
{
    setWindowTitle("Pafta Düzeni — PDF / SVG Çıktı");
    setMinimumWidth(500);
    BuildUI();
}

void PrintLayoutDialog::BuildUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // ── Sayfa Ayarları ───────────────────────────────────────
    auto* grpPage = new QGroupBox("Sayfa Ayarları");
    auto* fPage   = new QFormLayout(grpPage);

    m_cmbPageSize = new QComboBox();
    m_cmbPageSize->addItems({
        "A3 Yatay (420×297 mm)",
        "A3 Dikey  (297×420 mm)",
        "A4 Yatay (297×210 mm)",
        "A4 Dikey  (210×297 mm)"
    });
    fPage->addRow("Kağıt Boyutu:", m_cmbPageSize);

    m_chkAutoScale = new QCheckBox("Otomatik ölçek (çizimi sayfaya sığdır)");
    m_chkAutoScale->setChecked(true);
    fPage->addRow("", m_chkAutoScale);

    m_cmbScale = new QComboBox();
    m_cmbScale->addItems({"1:20", "1:50", "1:100", "1:200", "1:500"});
    m_cmbScale->setCurrentText("1:100");
    m_cmbScale->setEnabled(false);
    fPage->addRow("Manuel Ölçek:", m_cmbScale);

    mainLayout->addWidget(grpPage);

    connect(m_chkAutoScale, &QCheckBox::toggled, this, [this](bool checked){
        m_cmbScale->setEnabled(!checked);
    });
    connect(m_cmbPageSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrintLayoutDialog::OnPageSizeChanged);

    // ── Başlık Bloğu ─────────────────────────────────────────
    auto* grpTB = new QGroupBox("Başlık Bloğu (ISO 7200)");
    auto* fTB   = new QFormLayout(grpTB);

    m_edtProject      = new QLineEdit(); m_edtProject->setPlaceholderText("Proje adı");
    m_edtDrawingTitle = new QLineEdit(); m_edtDrawingTitle->setPlaceholderText("örn. Zemin Kat Temiz Su Tesisatı");
    m_edtDrawingNo    = new QLineEdit("P-001");
    m_edtRevision     = new QLineEdit("A");
    m_edtCompany      = new QLineEdit(); m_edtCompany->setPlaceholderText("Firma / Büro adı");
    m_edtDrawnBy      = new QLineEdit(); m_edtDrawnBy->setPlaceholderText("Baş harfler");
    m_edtDate         = new QLineEdit(QDate::currentDate().toString("yyyy-MM-dd"));

    fTB->addRow("Proje Adı *:",     m_edtProject);
    fTB->addRow("Pafta Adı:",       m_edtDrawingTitle);
    fTB->addRow("Pafta No:",        m_edtDrawingNo);
    fTB->addRow("Revizyon:",        m_edtRevision);
    fTB->addRow("Firma:",           m_edtCompany);
    fTB->addRow("Çizen:",           m_edtDrawnBy);
    fTB->addRow("Tarih:",           m_edtDate);

    // Logo satırı
    auto* logoRow = new QHBoxLayout();
    m_lblLogoPreview = new QLabel("Logo yüklenmedi");
    m_lblLogoPreview->setFixedSize(120, 40);
    m_lblLogoPreview->setAlignment(Qt::AlignCenter);
    m_lblLogoPreview->setStyleSheet(
        "QLabel { border:1px solid #aaa; background:#f8f8f8; color:#888; font-size:10px; }");
    m_btnLoadLogo  = new QPushButton("Yükle...");
    m_btnClearLogo = new QPushButton("Temizle");
    m_btnClearLogo->setEnabled(false);
    logoRow->addWidget(m_lblLogoPreview);
    logoRow->addWidget(m_btnLoadLogo);
    logoRow->addWidget(m_btnClearLogo);
    logoRow->addStretch();
    fTB->addRow("Firma Logosu:", logoRow);

    connect(m_btnLoadLogo,  &QPushButton::clicked, this, &PrintLayoutDialog::OnLoadLogo);
    connect(m_btnClearLogo, &QPushButton::clicked, this, &PrintLayoutDialog::OnClearLogo);

    mainLayout->addWidget(grpTB);

    // ── Durum ────────────────────────────────────────────────
    m_lblStatus = new QLabel("Hazır");
    m_lblStatus->setStyleSheet("QLabel { color: #444; font-size: 11px; }");
    mainLayout->addWidget(m_lblStatus);

    // ── Butonlar ─────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_btnPDF = new QPushButton("PDF Kaydet...");
    m_btnSVG = new QPushButton("SVG Kaydet...");
    auto* btnClose = new QPushButton("Kapat");

    btnRow->addWidget(m_btnPDF);
    btnRow->addWidget(m_btnSVG);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    mainLayout->addLayout(btnRow);

    connect(m_btnPDF,  &QPushButton::clicked, this, &PrintLayoutDialog::OnExportPDF);
    connect(m_btnSVG,  &QPushButton::clicked, this, &PrintLayoutDialog::OnExportSVG);
    connect(btnClose,  &QPushButton::clicked, this, &QDialog::accept);
}

TitleBlock PrintLayoutDialog::ReadTitleBlock() const {
    TitleBlock tb;
    tb.projectName   = m_edtProject->text().toStdString();
    tb.drawingTitle  = m_edtDrawingTitle->text().toStdString();
    tb.drawingNumber = m_edtDrawingNo->text().toStdString();
    tb.revision      = m_edtRevision->text().toStdString();
    tb.company       = m_edtCompany->text().toStdString();
    tb.drawnBy       = m_edtDrawnBy->text().toStdString();
    tb.date          = m_edtDate->text().toStdString();
    tb.standard      = "TS EN 806-3 / EN 12056-2";
    tb.logoPath      = m_logoPath.toStdString();

    if (m_chkAutoScale->isChecked()) {
        tb.scale = "Otomatik";
    } else {
        tb.scale = m_cmbScale->currentText().toStdString();
    }
    return tb;
}

void PrintLayoutDialog::OnLoadLogo() {
    QString path = QFileDialog::getOpenFileName(
        this, "Firma Logosu Seç", QString(),
        "Resim Dosyaları (*.png *.jpg *.jpeg *.bmp *.svg);;Tüm Dosyalar (*)");
    if (path.isEmpty()) return;

    QPixmap px(path);
    if (px.isNull()) {
        QMessageBox::warning(this, "Logo Yüklenemedi",
            "Dosya okunamadı veya desteklenmeyen format:\n" + path);
        return;
    }

    m_logoPixmap = px;
    m_logoPath   = path;
    m_lblLogoPreview->setPixmap(
        px.scaled(m_lblLogoPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_btnClearLogo->setEnabled(true);
    m_lblStatus->setText("Logo yüklendi: " + QFileInfo(path).fileName());
}

void PrintLayoutDialog::OnClearLogo() {
    m_logoPixmap = QPixmap();
    m_logoPath.clear();
    m_lblLogoPreview->setPixmap(QPixmap());
    m_lblLogoPreview->setText("Logo yüklenmedi");
    m_btnClearLogo->setEnabled(false);
    m_lblStatus->setText("Logo temizlendi.");
}

PrintLayout PrintLayoutDialog::GetConfiguredLayout() const {
    PrintLayout layout = m_baseLayout;

    // Sayfa boyutu
    switch (m_cmbPageSize->currentIndex()) {
        case 0: layout.SetPaperSize(PaperSize::A3_Landscape); break;
        case 1: layout.SetPaperSize(PaperSize::A3_Portrait);  break;
        case 2: layout.SetPaperSize(PaperSize::A4_Landscape); break;
        case 3: layout.SetPaperSize(PaperSize::A4_Portrait);  break;
    }

    // Ölçek
    layout.SetAutoScale(m_chkAutoScale->isChecked());
    if (!m_chkAutoScale->isChecked()) {
        QString scaleStr = m_cmbScale->currentText(); // "1:100"
        int colonIdx = scaleStr.indexOf(':');
        if (colonIdx >= 0) {
            double denom = scaleStr.mid(colonIdx + 1).toDouble();
            if (denom > 0.0) layout.SetScale(1.0 / denom);
        }
    }

    layout.SetTitleBlock(ReadTitleBlock());
    return layout;
}

void PrintLayoutDialog::SetInitialTitleBlock(const TitleBlock& tb) {
    if (m_edtProject)      m_edtProject->setText(QString::fromStdString(tb.projectName));
    if (m_edtDrawingTitle) m_edtDrawingTitle->setText(QString::fromStdString(tb.drawingTitle));
    if (m_edtDrawingNo)    m_edtDrawingNo->setText(QString::fromStdString(tb.drawingNumber));
    if (m_edtRevision)     m_edtRevision->setText(QString::fromStdString(tb.revision));
    if (m_edtCompany)      m_edtCompany->setText(QString::fromStdString(tb.company));
    if (m_edtDrawnBy)      m_edtDrawnBy->setText(QString::fromStdString(tb.drawnBy));
    if (m_edtDate && !tb.date.empty())
        m_edtDate->setText(QString::fromStdString(tb.date));
}

void PrintLayoutDialog::OnPageSizeChanged(int /*index*/) {
    // Gelecekte önizleme güncellenebilir
}

void PrintLayoutDialog::OnExportPDF() {
    if (m_edtProject->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Eksik Bilgi", "Proje Adı alanı boş bırakılamaz.");
        return;
    }

    QString startDir = m_projectsRoot.empty()
        ? QDir::homePath()
        : QString::fromStdString(m_projectsRoot);
    QString defaultName = QString("%1_pafta.pdf")
        .arg(m_edtProject->text().trimmed().replace(' ', '_'));

    QString path = QFileDialog::getSaveFileName(
        this, "PDF Olarak Kaydet", startDir + "/" + defaultName,
        "PDF Dosyası (*.pdf)");
    if (path.isEmpty()) return;

    m_lblStatus->setText("PDF üretiliyor...");
    m_lblStatus->repaint();

    PrintLayout layout = GetConfiguredLayout();
    if (layout.ExportToPDF(path.toStdString())) {
        m_lastSavedPath = path;
        m_lblStatus->setText(QString("Kaydedildi: %1").arg(path));
        QMessageBox::information(this, "PDF Kaydedildi",
            QString("Pafta başarıyla kaydedildi:\n%1").arg(path));
    } else {
        m_lblStatus->setText("HATA: PDF yazılamadı.");
        QMessageBox::critical(this, "PDF Hatası",
            "PDF dosyası oluşturulamadı.\nDizin izinlerini kontrol edin.");
    }
}

void PrintLayoutDialog::OnExportSVG() {
    if (m_edtProject->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Eksik Bilgi", "Proje Adı alanı boş bırakılamaz.");
        return;
    }

    QString startDir = m_projectsRoot.empty()
        ? QDir::homePath()
        : QString::fromStdString(m_projectsRoot);
    QString defaultName = QString("%1_pafta.svg")
        .arg(m_edtProject->text().trimmed().replace(' ', '_'));

    QString path = QFileDialog::getSaveFileName(
        this, "SVG Olarak Kaydet", startDir + "/" + defaultName,
        "SVG Dosyası (*.svg)");
    if (path.isEmpty()) return;

    m_lblStatus->setText("SVG üretiliyor...");
    m_lblStatus->repaint();

    PrintLayout layout = GetConfiguredLayout();
    if (layout.ExportToSVG(path.toStdString())) {
        m_lastSavedPath = path;
        m_lblStatus->setText(QString("Kaydedildi: %1").arg(path));
        QMessageBox::information(this, "SVG Kaydedildi",
            QString("Pafta başarıyla kaydedildi:\n%1").arg(path));
    } else {
        m_lblStatus->setText("HATA: SVG yazılamadı.");
        QMessageBox::critical(this, "SVG Hatası",
            "SVG dosyası oluşturulamadı.");
    }
}

} // namespace ui
} // namespace vkt
