#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QLabel>
#include <QString>

namespace vkt::ui {

/**
 * @brief Yeni proje oluşturma penceresi — proje meta verileri
 *
 * ProjectManager::CreateProject() çağrılmadan önce kullanıcıdan:
 *   - Proje adı, müşteri, mühendis
 *   - Bina tipi, standart seçimi (EN 806-3 / DIN 1988-300)
 *   - Proje tarihi
 * bilgilerini toplar.
 */
class NewProjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewProjectDialog(const QString& projectsRoot, QWidget* parent = nullptr);

    QString ProjectName()   const;
    QString CustomerName()  const;
    QString EngineerName()  const;
    QString BuildingType()  const;
    QString Norm()          const;  // "EN 806-3" veya "DIN 1988-300"
    QString ProjectDate()   const;  // ISO yyyy-MM-dd

private slots:
    void OnProjectNameChanged(const QString& text);
    void Validate();

private:
    void BuildUI(const QString& projectsRoot);

    QLineEdit*  m_edtName      = nullptr;
    QLineEdit*  m_edtCustomer  = nullptr;
    QLineEdit*  m_edtEngineer  = nullptr;
    QComboBox*  m_cmbBuildType = nullptr;
    QComboBox*  m_cmbNorm      = nullptr;
    QDateEdit*  m_datePicker   = nullptr;
    QLabel*     m_lblPreview   = nullptr;
    QPushButton* m_btnOk       = nullptr;
};

} // namespace vkt::ui
