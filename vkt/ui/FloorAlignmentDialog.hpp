#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include "core/FloorManager.hpp"
#include "mep/NetworkGraph.hpp"

namespace vkt::ui {

/**
 * @brief 3D Hizalama Kontrol Diyaloğu
 *
 * İçe aktarılmış her kat için Z kotunu, boru/node sayısını ve
 * hizalama durumunu tablo halinde gösterir. Kullanıcı
 * "Kat Yüksekliği" sütununu düzenleyerek kotları düzeltebilir.
 *
 * Sorunlu durumlar kırmızı renk ile işaretlenir:
 *  - Aynı Z kotuna sahip birden fazla kat (çakışma)
 *  - Hiç node içermeyen kat (boş kat — import edilmemiş)
 *  - Katlara atanamamış node var ise uyarı satırı
 */
class FloorAlignmentDialog : public QDialog {
    Q_OBJECT
public:
    explicit FloorAlignmentDialog(core::FloorManager& fm,
                                   const mep::NetworkGraph& network,
                                   QWidget* parent = nullptr);

    /// Kullanıcının düzelttiği kot değerlerini FloorManager'a yaz
    void ApplyChanges();

private slots:
    void OnCellChanged(int row, int col);

private:
    void BuildUI();
    void Populate();
    void HighlightIssues();

    core::FloorManager&         m_fm;
    const mep::NetworkGraph&    m_network;
    QTableWidget*               m_table   = nullptr;
    QLabel*                     m_lblSummary = nullptr;
    bool                        m_ignoreChange = false;
};

} // namespace vkt::ui
