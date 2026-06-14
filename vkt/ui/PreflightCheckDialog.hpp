#pragma once
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include "../mep/NetworkGraph.hpp"
#include "../mep/HydraulicSolver.hpp"

namespace vkt::ui {

/**
 * @brief Çıktı öncesi otomatik kontrol listesi — FineSANI "Son Kontrol" eşdeğeri.
 *
 * 6 kontrol: açık uç, kaynak, hesap güncelliği, boru numaralama,
 * bina tipi seçimi, katman görünürlüğü.
 * Kırmızı=hata (çıktı engellenir), sarı=uyarı, yeşil=OK.
 */
class PreflightCheckDialog : public QDialog {
    Q_OBJECT
public:
    enum class Severity { OK, Warning, Error };

    struct CheckResult {
        QString     label;
        Severity    severity = Severity::OK;
        QString     detail;
    };

    explicit PreflightCheckDialog(const mep::NetworkGraph& network,
                                  bool hydraulicRanRecently,
                                  QWidget* parent = nullptr);

    /// true → kullanıcı "Devam Et" seçti ve hata yok
    bool CanProceed() const { return m_canProceed; }

private:
    void RunChecks(const mep::NetworkGraph& network, bool hydraulicRanRecently);
    void AddRow(const CheckResult& r);

    QListWidget* m_list    = nullptr;
    QPushButton* m_btnOk   = nullptr;
    bool         m_canProceed = false;
    bool         m_hasError   = false;
};

} // namespace vkt::ui
