/**
 * @file PrintLayoutDialog.hpp
 * @brief Pafta ayarları + başlık bloğu + PDF/SVG çıktı diyaloğu
 */

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include "ui/PrintLayout.hpp"

namespace vkt {
namespace ui {

/**
 * @class PrintLayoutDialog
 * @brief Kullanıcıdan sayfa boyutu, ölçek ve başlık bloğu bilgilerini alır;
 *        PrintLayout üzerinden PDF veya SVG üretir.
 */
class PrintLayoutDialog : public QDialog {
    Q_OBJECT

public:
    explicit PrintLayoutDialog(const PrintLayout& layout,
                               const std::string& projectsRoot,
                               QWidget* parent = nullptr);

    /// Başlık bloğu alanlarını önceden doldur (proje verisiyle)
    void SetInitialTitleBlock(const TitleBlock& tb);

    /// Kullanıcı değerlerini layout'a uygulayarak doldurulmuş kopyayı döndür
    PrintLayout GetConfiguredLayout() const;

    QString LastSavedPath() const { return m_lastSavedPath; }

private slots:
    void OnExportPDF();
    void OnExportSVG();
    void OnPageSizeChanged(int index);

private:
    void BuildUI();
    TitleBlock ReadTitleBlock() const;

    PrintLayout m_baseLayout;
    std::string m_projectsRoot;
    QString     m_lastSavedPath;

    // Title block alanları
    QLineEdit* m_edtProject      = nullptr;
    QLineEdit* m_edtDrawingTitle = nullptr;
    QLineEdit* m_edtDrawingNo    = nullptr;
    QLineEdit* m_edtRevision     = nullptr;
    QLineEdit* m_edtCompany      = nullptr;
    QLineEdit* m_edtDrawnBy      = nullptr;
    QLineEdit* m_edtDate         = nullptr;

    // Sayfa ayarları
    QComboBox* m_cmbPageSize  = nullptr;
    QComboBox* m_cmbScale     = nullptr;
    QCheckBox* m_chkAutoScale = nullptr;

    // Durum etiketi
    QLabel* m_lblStatus = nullptr;

    QPushButton* m_btnPDF = nullptr;
    QPushButton* m_btnSVG = nullptr;
};

} // namespace ui
} // namespace vkt
