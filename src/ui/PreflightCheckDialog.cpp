#include "ui/PreflightCheckDialog.hpp"
#include "mep/HydraulicSolver.hpp"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidgetItem>

namespace vkt::ui {

PreflightCheckDialog::PreflightCheckDialog(const mep::NetworkGraph& network,
                                           bool hydraulicRanRecently,
                                           QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Çıktı Öncesi Kontrol");
    setMinimumWidth(520);

    auto* vl = new QVBoxLayout(this);
    vl->addWidget(new QLabel("<b>Çıktı alınmadan önce aşağıdaki kontroller yapılmıştır:</b>"));

    m_list = new QListWidget(this);
    m_list->setSpacing(2);
    vl->addWidget(m_list);

    RunChecks(network, hydraulicRanRecently);

    auto* info = new QLabel(m_hasError
        ? "<span style='color:red'>⛔ Hata var — lütfen düzeltin, sonra tekrar deneyin.</span>"
        : "<span style='color:green'>✔ Tüm kontroller geçti. Çıktı alınabilir.</span>",
        this);
    vl->addWidget(info);

    auto* bb = new QDialogButtonBox(this);
    m_btnOk = bb->addButton(m_hasError ? "Kapat" : "Devam Et", QDialogButtonBox::AcceptRole);
    if (!m_hasError) {
        auto* cancel = bb->addButton("İptal", QDialogButtonBox::RejectRole);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    connect(m_btnOk, &QPushButton::clicked, this, [this]() {
        m_canProceed = !m_hasError;
        accept();
    });
    vl->addWidget(bb);
}

void PreflightCheckDialog::RunChecks(const mep::NetworkGraph& network,
                                     bool hydraulicRanRecently) {
    const auto& nodes = network.GetNodes();
    const auto& edges = network.GetEdges();

    // 1 — Kaynak (Source/HotSource) var mı?
    {
        int srcCount = 0;
        for (const auto& n : nodes)
            if (n.type == mep::NodeType::Source || n.type == mep::NodeType::HotSource)
                ++srcCount;
        CheckResult r;
        r.label = "Su Kaynağı";
        if (srcCount == 0) {
            r.severity = Severity::Error;
            r.detail   = "Sistemde hiç kaynak (sayaç/hidrofor) tanımlanmamış.";
        } else if (srcCount > 1) {
            r.severity = Severity::Warning;
            r.detail   = QString("%1 kaynak var — tek kaynak önerilir.").arg(srcCount);
        } else {
            r.detail = "1 kaynak tanımlandı.";
        }
        AddRow(r);
    }

    // 2 — Açık uç var mı?
    {
        int openEnds = 0;
        for (const auto& n : nodes) {
            int deg = (int)network.GetConnectedEdges(n.id).size();
            if (deg == 0 || (n.type == mep::NodeType::Junction && deg == 1))
                ++openEnds;
        }
        CheckResult r;
        r.label = "Açık Uç Kontrolü";
        if (openEnds > 0) {
            r.severity = Severity::Error;
            r.detail   = QString("%1 açık uç/kopuk düğüm tespit edildi.").arg(openEnds);
        } else {
            r.detail = "Açık uç yok.";
        }
        AddRow(r);
    }

    // 3 — Hidrolik hesap yapıldı mı?
    {
        bool anyFlow = false;
        for (const auto& e : edges)
            if (e.flowRate_m3s > 1e-9) { anyFlow = true; break; }

        CheckResult r;
        r.label = "Hidrolik Hesap";
        if (!anyFlow) {
            r.severity = Severity::Error;
            r.detail   = "Hesap henüz çalıştırılmamış — Analiz → Hesapla veya Ctrl+H.";
        } else if (!hydraulicRanRecently) {
            r.severity = Severity::Warning;
            r.detail   = "Son boru değişikliğinden bu yana hesap yenilenmemiş olabilir.";
        } else {
            r.detail = "Hesap güncel.";
        }
        AddRow(r);
    }

    // 4 — Boru numaralandırma yapıldı mı?
    {
        int labeled = 0;
        for (const auto& e : edges)
            if (!e.label.empty()) ++labeled;

        CheckResult r;
        r.label = "Boru Numaralandırma";
        if (labeled == 0 && !edges.empty()) {
            r.severity = Severity::Warning;
            r.detail   = "Borulara numara atanmamış — Tesisatı Kabul Et (Ctrl+Enter) çalıştırın.";
        } else {
            r.detail = QString("%1/%2 boru numaralı.").arg(labeled).arg(edges.size());
        }
        AddRow(r);
    }

    // 5 — DN/çap bilgisi var mı?
    {
        int zeroDN = 0;
        for (const auto& e : edges)
            if (e.type == mep::EdgeType::Supply || e.type == mep::EdgeType::HotWater)
                if (e.diameter_mm < 1.0) ++zeroDN;

        CheckResult r;
        r.label = "DN Boyutlandırma";
        if (zeroDN > 0) {
            r.severity = Severity::Error;
            r.detail   = QString("%1 boru çapsız — otomatik boyutlandırma için Analiz → Hesapla.").arg(zeroDN);
        } else {
            r.detail = "Tüm borular DN bilgisine sahip.";
        }
        AddRow(r);
    }

    // 6 — Boşaltma noktası (Drainage/Drain node) var mı?
    {
        int drainCount = 0;
        for (const auto& n : nodes)
            if (n.type == mep::NodeType::Drain) ++drainCount;

        bool hasDrainEdge = false;
        for (const auto& e : edges)
            if (e.type == mep::EdgeType::Drainage) { hasDrainEdge = true; break; }

        CheckResult r;
        r.label = "Boşaltma / Pis Su";
        if (hasDrainEdge && drainCount == 0) {
            r.severity = Severity::Warning;
            r.detail   = "Pis su borusu var ama boşaltma noktası (rögar) tanımlanmamış.";
        } else {
            r.detail = hasDrainEdge
                ? QString("%1 boşaltma noktası tanımlandı.").arg(drainCount)
                : "Pis su sistemi yok (gerekli değilse OK).";
        }
        AddRow(r);
    }

    // has error?
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        if (item->data(Qt::UserRole).toInt() == (int)Severity::Error) {
            m_hasError = true;
            break;
        }
    }
}

void PreflightCheckDialog::AddRow(const CheckResult& r) {
    QString icon = (r.severity == Severity::Error)   ? "⛔"
                 : (r.severity == Severity::Warning)  ? "⚠️"
                                                       : "✅";
    QString text = QString("%1  %2 — %3").arg(icon, r.label, r.detail);
    auto* item = new QListWidgetItem(text, m_list);
    if (r.severity == Severity::Error)
        item->setForeground(QColor("#c00000"));
    else if (r.severity == Severity::Warning)
        item->setForeground(QColor("#7a5000"));
    item->setData(Qt::UserRole, (int)r.severity);
}

} // namespace vkt::ui
