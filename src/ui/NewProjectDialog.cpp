#include "ui/NewProjectDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QDate>
#include <QRegularExpressionValidator>

namespace vkt::ui {

NewProjectDialog::NewProjectDialog(const QString& projectsRoot, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Yeni Proje Oluştur");
    setMinimumWidth(480);
    BuildUI(projectsRoot);
}

void NewProjectDialog::BuildUI(const QString& projectsRoot) {
    auto* mainLayout = new QVBoxLayout(this);

    // Başlık + kök dizin bilgisi
    auto* rootLbl = new QLabel(
        QString("Proje klasörü şu dizinde oluşturulacak:<br>"
                "<b>%1</b>").arg(projectsRoot.isEmpty() ? "(henüz belirlenmedi)" : projectsRoot));
    rootLbl->setWordWrap(true);
    mainLayout->addWidget(rootLbl);

    // ── Proje Bilgileri ─────────────────────────────────────
    auto* grpProje = new QGroupBox("Proje Bilgileri");
    auto* form = new QFormLayout(grpProje);

    m_edtName = new QLineEdit();
    m_edtName->setPlaceholderText("örn. KonutA_BlokB");
    // Windows klasör adı kuralı: özel karakter yasağı
    m_edtName->setValidator(
        new QRegularExpressionValidator(QRegularExpression(R"([^\\/:*?"<>|]+)"), this));
    m_edtName->setToolTip("Proje klasörünün adı (özel karakter kullanmayın).");
    form->addRow("Proje Adı *:", m_edtName);

    // Önizleme: "Klasör: C:/Projeler/KonutA_BlokB"
    m_lblPreview = new QLabel();
    m_lblPreview->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    form->addRow("", m_lblPreview);

    m_edtCustomer = new QLineEdit();
    m_edtCustomer->setPlaceholderText("Müşteri / İşveren adı");
    form->addRow("Müşteri:", m_edtCustomer);

    m_edtEngineer = new QLineEdit();
    m_edtEngineer->setPlaceholderText("Proje mühendisi adı");
    form->addRow("Mühendis:", m_edtEngineer);

    mainLayout->addWidget(grpProje);

    // ── Teknik Parametreler ──────────────────────────────────
    auto* grpTeknik = new QGroupBox("Teknik Parametreler");
    auto* fTeknik = new QFormLayout(grpTeknik);

    m_cmbBuildType = new QComboBox();
    m_cmbBuildType->addItems({
        "Konut (Apartman/Villa)",
        "Ofis / İşyeri",
        "Otel / Motel",
        "Hastane / Sağlık",
        "Okul / Eğitim",
        "Sanayi / Fabrika",
        "AVM / Ticari",
        "Diğer"
    });
    fTeknik->addRow("Bina Tipi:", m_cmbBuildType);

    m_cmbNorm = new QComboBox();
    m_cmbNorm->addItems({"EN 806-3 (Avrupa / Türkiye TS)", "DIN 1988-300 (Alman Standardı)"});
    m_cmbNorm->setToolTip("Debi hesaplama normunu şimdi seçin; "
                          "daha sonra Analiz → Hesap Normu ile değiştirilebilir.");
    fTeknik->addRow("Hesap Normu:", m_cmbNorm);

    m_datePicker = new QDateEdit(QDate::currentDate());
    m_datePicker->setCalendarPopup(true);
    m_datePicker->setDisplayFormat("dd.MM.yyyy");
    fTeknik->addRow("Proje Tarihi:", m_datePicker);

    mainLayout->addWidget(grpTeknik);

    // ── Tamam / İptal ────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_btnOk = buttons->button(QDialogButtonBox::Ok);
    m_btnOk->setText("Proje Oluştur");
    m_btnOk->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText("İptal");
    mainLayout->addWidget(buttons);

    connect(m_edtName, &QLineEdit::textChanged, this, &NewProjectDialog::OnProjectNameChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // İlk önizlemeyi oluştur
    OnProjectNameChanged("");
    // Kök dizin bilgisini önizlemeye göm
    if (!projectsRoot.isEmpty()) {
        connect(m_edtName, &QLineEdit::textChanged, this, [this, projectsRoot](const QString& txt) {
            if (txt.trimmed().isEmpty())
                m_lblPreview->setText("Klasör adı girilmedi");
            else
                m_lblPreview->setText(QString("Klasör: %1/%2").arg(projectsRoot, txt.trimmed()));
        });
    }
}

void NewProjectDialog::OnProjectNameChanged(const QString& text) {
    bool valid = !text.trimmed().isEmpty();
    m_btnOk->setEnabled(valid);
}

void NewProjectDialog::Validate() {}

QString NewProjectDialog::ProjectName()  const { return m_edtName ? m_edtName->text().trimmed() : QString(); }
QString NewProjectDialog::CustomerName() const { return m_edtCustomer ? m_edtCustomer->text().trimmed() : QString(); }
QString NewProjectDialog::EngineerName() const { return m_edtEngineer ? m_edtEngineer->text().trimmed() : QString(); }
QString NewProjectDialog::BuildingType() const { return m_cmbBuildType ? m_cmbBuildType->currentText() : QString(); }
QString NewProjectDialog::Norm()         const { return m_cmbNorm ? m_cmbNorm->currentText() : QString(); }
QString NewProjectDialog::ProjectDate()  const {
    return m_datePicker ? m_datePicker->date().toString("yyyy-MM-dd") : QString();
}

} // namespace vkt::ui
