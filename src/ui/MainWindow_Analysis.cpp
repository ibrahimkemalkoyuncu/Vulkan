/**
 * @file MainWindow_Analysis.cpp
 * @brief Analysis/hydraulic calculation dialog methods extracted from MainWindow.cpp
 *
 * This file contains all MEP analysis, hydraulic calculation, and engineering
 * dialog methods for vkt::ui::MainWindow. It is part of the same class —
 * split purely for source file manageability.
 */


#include "ui/MainWindow.hpp"
#include <QApplication>
#include "ui/DXFImportDialog.hpp"
#include "ui/MimariBelirleDialog.hpp"
#include "ui/NewProjectDialog.hpp"
#include "ui/FloorAlignmentDialog.hpp"
#include "ui/PrintLayoutDialog.hpp"
#include "ui/PreflightCheckDialog.hpp"
#include "core/ProjectManager.hpp"
#include "core/DocxWriter.hpp"
#include "core/IfcExporter.hpp"
#include "ui/SpacePanel.hpp"
#include "ui/CommandBar.hpp"
#include "ui/SnapOverlay.hpp"
#include "render/VulkanWindow.hpp"
#include "mep/HydraulicSolver.hpp"
#include "mep/RiserDiagram.hpp"
#include "mep/ScheduleGenerator.hpp"
#include "mep/Database.hpp"
#include "mep/XLSXWriter.hpp"
#include "mep/MaterialProperties.hpp"
#include "core/Application.hpp"
#include "core/Commands.hpp"
#include "cad/Text.hpp"
#include "cad/Line.hpp"
#include "cad/Circle.hpp"
#include "cad/Arc.hpp"
#include "cad/DXFWriter.hpp"
#include "cad/Dimension.hpp"
#include "cad/Polyline.hpp"
#include "core/Version.hpp"
#include <QGuiApplication>
#include <QRegularExpression>
#include <filesystem>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QColorDialog>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QTextBrowser>
#include <QTableWidget>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QComboBox>
#include <QPrinter>
#include <QPainter>
#include <QPageLayout>
#include <QSvgRenderer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

static void LogCAD(const std::string& msg) {
#ifndef NDEBUG
    std::cout << "[MainWindow] " << msg << std::endl;
#else
    (void)msg;
#endif
}

namespace vkt {
namespace ui {

void MainWindow::OnRunHydraulics() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty() || network.GetNodeMap().empty()) {
        QMessageBox::warning(this, "Hidrolik Analiz",
            "Ağda boru veya armatür bulunamadı.\n"
            "Önce boru çizin ve armatür ekleyin, ardından analizi tekrar çalıştırın.");
        return;
    }

    m_logList->clear();
    m_logList->addItem("═══════════════════════════════════════");
    m_logList->addItem("  FULL MEP ANALİZ BAŞLIYOR");
    m_logList->addItem("═══════════════════════════════════════");

    mep::HydraulicSolver solver(network);

    // 1. Besleme analizi
    m_logList->addItem("");
    m_logList->addItem("1️⃣ TEMİZ SU SİSTEMİ (TS EN 806-3)");
    solver.Solve();
    m_logList->addItem("✅ Besleme analizi tamamlandı");

    // 2. Drenaj analizi
    m_logList->addItem("");
    m_logList->addItem("2️⃣ ATIK SU SİSTEMİ (EN 12056)");
    solver.SolveDrainage();
    m_logList->addItem("✅ Drenaj analizi tamamlandı");

    // 3. Kritik devre
    m_logList->addItem("");
    m_logList->addItem("3️⃣ KRİTİK DEVRE (Pompa Yüksekliği)");
    auto criticalPath = solver.CalculateCriticalPath();
    
    QString cpMsg = QString("✅ Gerekli pompa yüksekliği: %1 mSS")
        .arg(criticalPath.requiredPumpHead_m, 0, 'f', 2);
    m_logList->addItem(cpMsg);

    m_logList->addItem("");
    m_logList->addItem("═══════════════════════════════════════");
    m_logList->addItem("  ANALİZ TAMAMLANDI ✅");
    m_logList->addItem("═══════════════════════════════════════");

    statusBar()->showMessage("Hidrolik analiz tamamlandı!", 3000);
}

// ============================================================
//  DN OTOMATİK BOYUTLANDIRMA (EN 806-3)
// ============================================================
void MainWindow::OnAutoSizeDN() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        statusBar()->showMessage("DN boyutlandırma: önce boru çizin.");
        return;
    }

    mep::HydraulicSolver solver(network);
    solver.Solve();
    solver.SolveDrainage();

    // EN 806-3: v_max=3 m/s → D_min = sqrt(4Q/π/v_max) mm
    // Solver flow_rate_lps yazılmışsa kullan, yoksa LU→Q tahmin
    const double v_max = 2.5; // m/s (konforlu)
    int sized = 0;

    // Standart DN serileri (PVC/PP mm OD → iç çap yaklaşık)
    static const double kDN[] = {16,20,25,32,40,50,63,75,90,110,125,160,200,0};

    auto selectDN = [&](double Q_lps) -> double {
        double Q_m3s = Q_lps / 1000.0;
        double d_m   = std::sqrt(4.0 * Q_m3s / M_PI / v_max);
        double d_mm  = d_m * 1000.0;
        for (int i = 0; kDN[i] > 0; ++i)
            if (kDN[i] >= d_mm) return kDN[i];
        return 200.0;
    };

    m_logList->clear();
    m_logList->addItem("── DN Otomatik Boyutlandırma (EN 806-3) ──");

    // Toplam LU: tüm fixture'lar (EN 806-3 Tablo 3 yaklaşımı)
    double totalLU = 0.0;
    for (const auto& [nid, node] : network.GetNodeMap())
        if (node.type == mep::NodeType::Fixture) totalLU += node.loadUnit;
    totalLU = std::max(totalLU, 0.1);

    int edgeCount = static_cast<int>(network.GetEdgeMap().size());

    for (auto& [eid, edge] : network.GetEdgeMap()) {
        // Önce solver sonucunu kullan, yoksa LU'dan tahmin et
        double Q_m3s = edge.flowRate_m3s;
        double Q_lps;
        if (Q_m3s > 1e-9) {
            Q_lps = Q_m3s * 1000.0;
        } else {
            // EN 806-3: Q = 0.682 * LU^0.45  (l/s), eşit dağılım varsayımı
            double edgeLU = totalLU / std::max(edgeCount, 1);
            Q_lps = 0.682 * std::pow(edgeLU, 0.45);
        }
        Q_lps = std::max(Q_lps, 0.05);
        double newDN = selectDN(Q_lps);
        if (std::abs(newDN - edge.diameter_mm) > 0.5) {
            m_logList->addItem(QString("Boru #%1: DN%2 → DN%3 (Q=%4 l/s)")
                .arg(eid).arg(edge.diameter_mm, 0, 'f', 0)
                .arg(newDN, 0, 'f', 0).arg(Q_lps, 0, 'f', 3));
        }
        edge.diameter_mm = newDN;
        edge.label = "DN" + std::to_string(static_cast<int>(newDN));
        sized++;
    }

    m_logList->addItem(QString("✅ %1 boru boyutlandırıldı").arg(sized));
    m_document->SetModified(true);
    UpdateUI();
    statusBar()->showMessage(QString("DN boyutlandırma tamamlandı: %1 boru güncellendi").arg(sized), 3000);
}

// ============================================================
//  GERÇEK ZAMANLI HİDROLİK (DEBOUNCED AUTO-DN)
// ============================================================

bool MainWindow::RunPreflight() {
    if (!m_document) return false;
    auto& network = m_document->GetNetwork();
    PreflightCheckDialog dlg(network, m_hydraulicRanRecently, this);
    dlg.exec();
    return dlg.CanProceed();
}

void MainWindow::ScheduleAutoHydro() {
    m_hydraulicRanRecently = false; // ağ değişti → hesap stale
    if (m_autoHydroTimer) m_autoHydroTimer->start(600);
}

void MainWindow::RunAutoHydro() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) return;

    static const double kDN[] = {16,20,25,32,40,50,63,75,90,110,125,160,200,0};
    const double v_max = 2.5;

    auto selectDN = [&](double Q_lps) -> double {
        double d_mm = std::sqrt(4.0 * (Q_lps / 1000.0) / M_PI / v_max) * 1000.0;
        for (int i = 0; kDN[i] > 0; ++i)
            if (kDN[i] >= d_mm) return kDN[i];
        return 200.0;
    };

    mep::HydraulicSolver solver(network);
    solver.Solve();
    solver.SolveDrainage();
    solver.SolveGas();
    solver.SolveHeating();
    solver.SolveElectric();
    solver.SolveVentilation();

    // Solver uyarı/hata bildirimlerini status bar + log'a yaz
    if (solver.HasErrors()) {
        QString errMsg;
        for (const auto& e : solver.GetErrors()) errMsg += QString::fromStdString(e) + "; ";
        statusBar()->showMessage("Hidrolik HATA: " + errMsg, 5000);
        if (m_logList) m_logList->addItem("[HATA] " + errMsg);
    }
    for (const auto& w : solver.GetWarnings()) {
        if (m_logList) m_logList->addItem("[UYARI] " + QString::fromStdString(w));
    }
    if (solver.HasErrors()) return;

    // ── Topoloji tabanlı downstream LU (DFS) ─────────────────
    // Her edge için: o edge'in "ilerisi"ndeki fixture'ların toplam LU'su
    // Tree-like ağlarda doğru; döngü varsa fallback eşit dağılım
    std::unordered_map<uint32_t, double> edgeDownstreamLU; // eid → downstream LU
    {
        // Her node'un kendi LU katkısı
        std::unordered_map<uint32_t, double> nodeLU;
        double totalLU = 0.0;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            double lu = (node.type == mep::NodeType::Fixture) ? std::max(node.loadUnit, 0.1) : 0.0;
            nodeLU[nid] = lu;
            totalLU += lu;
        }
        totalLU = std::max(totalLU, 0.1);

        // DFS: her Source'dan başla, her edge için downstream subtree LU hesapla
        std::unordered_set<uint32_t> visited;
        std::function<double(uint32_t, uint32_t)> dfs = [&](uint32_t nid, uint32_t parentId) -> double {
            visited.insert(nid);
            double lu = nodeLU[nid];
            for (uint32_t eid : network.GetConnectedEdges(nid)) {
                const mep::Edge* e = network.GetEdge(eid);
                if (!e) continue;
                uint32_t neighbor = (e->nodeA == nid) ? e->nodeB : e->nodeA;
                if (neighbor == parentId || visited.count(neighbor)) continue;
                double childLU = dfs(neighbor, nid);
                lu += childLU;
                // Bu edge, neighbor tarafındaki subtree'yi taşıyor
                edgeDownstreamLU[eid] = childLU;
            }
            return lu;
        };

        // Source node'lardan başla; source yoksa tüm node'lardan başla
        bool hasSource = false;
        for (const auto& [nid, node] : network.GetNodeMap()) {
            if (node.type == mep::NodeType::Source || node.type == mep::NodeType::HotSource) {
                if (!visited.count(nid)) dfs(nid, UINT32_MAX);
                hasSource = true;
            }
        }
        if (!hasSource) {
            for (const auto& [nid, node] : network.GetNodeMap())
                if (!visited.count(nid)) dfs(nid, UINT32_MAX);
        }

        // Hâlâ atanmamış edge'ler (izole bölgeler) → eşit dağılım
        int edgeCount = static_cast<int>(network.GetEdgeMap().size());
        double equalLU = totalLU / std::max(edgeCount, 1);
        for (const auto& [eid, edge] : network.GetEdgeMap())
            if (!edgeDownstreamLU.count(eid))
                edgeDownstreamLU[eid] = equalLU;
    }

    // ── Edge başına DN seçimi ──────────────────────────────────
    int updated = 0;
    for (auto& [eid, edge] : network.GetEdgeMap()) {
        double Q_lps;
        if (edge.flowRate_m3s > 1e-9) {
            // Solver çözdüyse doğrudan kullan
            Q_lps = edge.flowRate_m3s * 1000.0;
        } else {
            // Downstream LU'dan EN 806-3 tahmini
            double lu = std::max(edgeDownstreamLU[eid], 0.1);
            Q_lps = 0.682 * std::pow(lu, 0.45);
        }
        Q_lps = std::max(Q_lps, 0.05);
        double newDN = selectDN(Q_lps);
        if (std::abs(newDN - edge.diameter_mm) > 0.5 || edge.label.empty()) {
            edge.diameter_mm = newDN;
            edge.label = "DN" + std::to_string(static_cast<int>(newDN));
            updated++;
        }
    }

    if (updated > 0) {
        RefreshTextOverlay();
        statusBar()->showMessage(
            QString("⚡ Otomatik DN: %1 boru güncellendi (EN 806-3)").arg(updated), 3000);
    }

    // Kritik devre edge'lerini renderer'a bildir → turuncu-kırmızı vurgulama
    if (m_vulkanWindow) {
        mep::HydraulicSolver critSolver(network);
        critSolver.Solve();
        auto critResult = critSolver.CalculateCriticalPath();
        m_vulkanWindow->SetCriticalPathEdges(critResult.criticalPath);
        m_vulkanWindow->requestUpdate();
    }

    m_hydraulicRanRecently = true;
}

void MainWindow::OnGenerateSchedule() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();
    mep::ScheduleGenerator generator(network);

    m_logList->clear();
    m_logList->addItem("Metraj oluşturuluyor...");

    auto materials = generator.GenerateMaterialList();
    m_logList->addItem(QString("Toplam %1 kalem malzeme").arg(materials.size()));

    for (const auto& item : materials) {
        QString line = QString("%1: %2 %3")
            .arg(QString::fromStdString(item.description))
            .arg(item.quantity, 0, 'f', 2)
            .arg(QString::fromStdString(item.unit));
        m_logList->addItem(line);
    }

    statusBar()->showMessage("Metraj oluşturuldu!");
}

void MainWindow::OnExportReport() {
    if (!m_document) return;

    // Proje varsa rapor/ klasöründen başla
    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder())
        : "";

    QString filePath = QFileDialog::getSaveFileName(this,
        "Rapor Kaydet", startDir,
        "Excel Raporu (*.xls);;"
        "CSV Dosyası (*.csv);;"
        "Metin Dosyası (*.txt)");
    if (filePath.isEmpty()) return;

    auto& network = m_document->GetNetwork();
    bool ok = false;

    if (filePath.endsWith(".xls", Qt::CaseInsensitive)) {
        ok = mep::XLSXWriter::ExportProjectReport(filePath.toStdString(), network);
    } else {
        mep::ScheduleGenerator generator(network);
        std::string content = filePath.endsWith(".csv", Qt::CaseInsensitive)
                              ? generator.ExportToCSV()
                              : generator.GenerateHydraulicReport();
        std::ofstream f(filePath.toStdString());
        if (f.is_open()) { f << content; ok = true; }
    }

    if (ok) {
        statusBar()->showMessage(QString("Rapor kaydedildi: %1").arg(filePath), 3000);
    } else {
        QMessageBox::critical(this, "Hata", "Dosya yazılamadı!");
    }
}

// ============================================================
//  HİDROFOR BOYUTLANDIRMA DİYALOGU
// ============================================================
void MainWindow::OnHidrofor() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Hidrofor Boyutlandirma",
            "Once tesisat sebekesi cizin (boru + kaynak + armaturler).");
        return;
    }

    mep::HydraulicSolver solver(network);
    solver.Solve();
    auto result = solver.CalculateCriticalPath();

    QString msg;
    msg += QString("<h3>Hidrofor / Pompa Boyutlandirma</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td><b>Kritik devre kaybi:</b></td><td><font color='red'><b>%1 mSS</b></font></td></tr>")
               .arg(result.totalHeadLoss_m, 0, 'f', 2);
    msg += QString("<tr><td><b>Gerekli pompa basma yuksekligi:</b></td><td><font color='red'><b>%1 mSS</b></font></td></tr>")
               .arg(result.requiredPumpHead_m, 0, 'f', 2);
    msg += QString("<tr><td><b>Gerekli debi:</b></td><td>%1 m³/h</td></tr>")
               .arg(result.requiredFlow_m3h, 0, 'f', 2);
    msg += QString("</table><hr>");
    msg += QString("<b>Onerilen Ekipman:</b><br>");
    msg += QString("Model: %1<br>").arg(QString::fromStdString(result.suggestedPumpModel));
    msg += QString("Maks. basinc: %1 mSS<br>").arg(result.suggestedPumpHead_m, 0, 'f', 1);
    msg += QString("Maks. debi: %1 m³/h<br>").arg(result.suggestedPumpFlow_m3h, 0, 'f', 1);
    msg += QString("Guc: %1 kW").arg(result.suggestedPumpPower_kW, 0, 'f', 2);

    QMessageBox dlg(this);
    dlg.setWindowTitle("Hidrofor Boyutlandirma (TS EN 806-3)");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(QMessageBox::Information);
    dlg.exec();

    if (m_logList) {
        m_logList->addItem(QString("Hidrofor: %1 mSS / %2 m3/h → %3")
            .arg(result.requiredPumpHead_m, 0, 'f', 2)
            .arg(result.requiredFlow_m3h, 0, 'f', 2)
            .arg(QString::fromStdString(result.suggestedPumpModel)));
    }
}

// ============================================================
//  HESAP NORMU SEÇİMİ (EN 806-3 / DIN 1988-300)
// ============================================================
void MainWindow::OnNormSelection() {
    QStringList norms;
    norms << "TS EN 806-3  (Avrupa / Turkiye standardi — varsayilan)"
          << "DIN 1988-300  (Alman standardi — esszamanlilik katsayili)"
          << "TS 825 Sarfiyat  (Musluk Birimi — Turkiye kamu ihalelerinde kullanilir)";

    auto cur = mep::HydraulicSolver::GlobalNorm();
    int current = (cur == mep::HydroNorm::DIN1988) ? 1
                : (cur == mep::HydroNorm::SARFIYAT) ? 2 : 0;

    bool ok = false;
    QString chosen = QInputDialog::getItem(this,
        "Hesap Normu Secimi",
        "Besleme debisi hesaplama standardi:",
        norms, current, false, &ok);
    if (!ok) return;

    if (chosen.startsWith("DIN")) {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
        statusBar()->showMessage("Hesap normu: DIN 1988-300 (esszamanlilik katsayili)");
        if (m_logList) m_logList->addItem("Norm degistirildi: DIN 1988-300");
    } else if (chosen.startsWith("TS 825")) {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::SARFIYAT;
        statusBar()->showMessage("Hesap normu: TS 825 Sarfiyat / Musluk Birimi");
        if (m_logList) m_logList->addItem("Norm degistirildi: TS 825 Sarfiyat (Musluk Birimi)");
    } else {
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;
        statusBar()->showMessage("Hesap normu: TS EN 806-3");
        if (m_logList) m_logList->addItem("Norm degistirildi: TS EN 806-3");
    }
    ScheduleAutoHydro();
}

// ============================================================
//  DEVRE SEÇENEKLERİ (FineSANI eşdeğeri)
// ============================================================
void MainWindow::OnDevreSecenekleri() {
    DevreSecenekleriDialog dlg(m_devreParams, this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_devreParams = dlg.GetParams();

    // Norm senkronizasyonu
    if (m_devreParams.norm == "DIN 1988-300")
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
    else
        mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;

    // Pürüzlülüğü tüm Supply/HotWater edgelere uygula (mevcut ağa)
    if (m_document) {
        auto& network = m_document->GetNetwork();
        for (auto& [eid, edge] : network.GetEdgeMap()) {
            if (edge.type == mep::EdgeType::Supply || edge.type == mep::EdgeType::HotWater) {
                edge.roughness_mm = m_devreParams.roughness_mm;
                edge.material     = m_devreParams.mainPipeMat.toStdString();
            }
        }
        m_document->SetModified(true);
        ScheduleAutoHydro();
    }

    QString msg = QString("Devre Seçenekleri uygulandı — %1, %2, pürüzlülük=%3 mm, max hız=%4 m/s")
        .arg(m_devreParams.norm)
        .arg(m_devreParams.mainPipeMat)
        .arg(m_devreParams.roughness_mm, 0, 'f', 4)
        .arg(m_devreParams.maxVelocity_ms, 0, 'f', 1);
    statusBar()->showMessage(msg, 3000);
    if (m_logList) m_logList->addItem(msg);
}

// ============================================================
//  BASKI İÇERİĞİ — çizimde hangi etiketler görünsün
// ============================================================
void MainWindow::OnBaskiIcerigi() {
    QDialog dlg(this);
    dlg.setWindowTitle("Baskı İçeriği — Çizimde Görünecek Değerler");
    dlg.setMinimumWidth(320);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("<b>Boru etiketi bileşenleri:</b>"));

    auto* cbDN       = new QCheckBox("DN (çap, örn: DN32)");              cbDN->setChecked(m_labelShowDN);
    auto* cbFlow     = new QCheckBox("Debi Q (L/s)");                     cbFlow->setChecked(m_labelShowFlow);
    auto* cbLen      = new QCheckBox("Uzunluk L (m)");                    cbLen->setChecked(m_labelShowLength);
    auto* cbVel      = new QCheckBox("Hız v (m/s)  [Temiz Su]");          cbVel->setChecked(m_labelShowVelocity);
    auto* cbHL       = new QCheckBox("Basınç kaybı ΔH (m)  [Temiz Su]"); cbHL->setChecked(m_labelShowHeadLoss);

    layout->addWidget(cbDN);
    layout->addWidget(cbFlow);
    layout->addWidget(cbLen);
    layout->addWidget(cbVel);
    layout->addWidget(cbHL);

    layout->addWidget(new QLabel("<b>Pis Su ek etiketleri:</b>"));

    auto* cbSlope    = new QCheckBox("Eğim i (%)  [Pis Su]");             cbSlope->setChecked(m_labelShowSlope);
    auto* cbFillRate = new QCheckBox("Doluluk h/d (%)  [Pis Su]");        cbFillRate->setChecked(m_labelShowFillRate);

    layout->addWidget(cbSlope);
    layout->addWidget(cbFillRate);

    auto* note = new QLabel("<small><i>Seçimler anlık etiket overlay'ini etkiler.</i></small>");
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    m_labelShowDN       = cbDN->isChecked();
    m_labelShowFlow     = cbFlow->isChecked();
    m_labelShowLength   = cbLen->isChecked();
    m_labelShowVelocity = cbVel->isChecked();
    m_labelShowHeadLoss = cbHL->isChecked();
    m_labelShowSlope    = cbSlope->isChecked();
    m_labelShowFillRate = cbFillRate->isChecked();

    RefreshTextOverlay();
    statusBar()->showMessage("Baskı İçeriği güncellendi — etiketler yenilendi", 2000);
}

// ============================================================
//  PARÇALARIN BASINÇ KAYBI — tüm devreler detay tablosu
// ============================================================
void MainWindow::OnBaskiKaybi() {
    if (!m_document) return;

    // Önce hesapla
    auto& network = m_document->GetNetwork();
    mep::HydraulicSolver solver(network);
    solver.Solve();

    QString html;
    html += "<html><body style='font-family:Arial;font-size:11px'>";
    html += "<h3>Parçaların Basınç Kaybı Tablosu</h3>";
    html += "<table border='1' cellpadding='4' cellspacing='0' width='100%'>";
    html += "<tr style='background:#2255aa;color:white'>"
            "<th>Devre No</th><th>Tip</th><th>Malzeme</th>"
            "<th>DN (mm)</th><th>L (m)</th>"
            "<th>Q_nom (L/s)</th><th>Q_hes (L/s)</th>"
            "<th>v (m/s)</th><th>ΔH_surt (m)</th><th>ΔH_lokal (m)</th><th>ΔH_top (m)</th><th>Durum</th></tr>";

    // Kritik devreyi bul
    mep::HydraulicSolver critSolver(network);
    auto critResult = critSolver.CalculateCriticalPath();
    std::set<uint32_t> critSet(critResult.criticalPath.begin(), critResult.criticalPath.end());

    int row = 0;
    double totalHL = 0.0;
    uint32_t criticalEdgeId = 0;
    double   maxHL = 0.0;

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type == mep::EdgeType::Drainage || edge.type == mep::EdgeType::Vent) continue;

        bool isCrit = (critSet.count(eid) > 0);
        QString bg  = isCrit ? "#fff3cd" : (row % 2 == 0 ? "#f5f8ff" : "white");

        QString typeStr;
        switch (edge.type) {
            case mep::EdgeType::Supply:   typeStr = "Soğuk Su"; break;
            case mep::EdgeType::HotWater: typeStr = "Sıcak Su"; break;
            default:                       typeStr = "Diğer";    break;
        }

        double qNom = (edge.nominalFlow_Ls > 0.0) ? edge.nominalFlow_Ls : edge.flowRate_m3s * 1000.0;
        double qHes = edge.flowRate_m3s * 1000.0;
        QString qNomStr = QString("%1").arg(qNom, 0, 'f', 3);
        // Eğer DIN simultaneity uygulandıysa nominal > hesap → vurgula
        QString qHesStr = (qNom > qHes + 0.001)
            ? QString("<font color='#00a'>%1</font>").arg(qHes, 0, 'f', 3)
            : QString("%1").arg(qHes, 0, 'f', 3);

        double localLoss_m = edge.localLoss_Pa / (1000.0 * 9.81); // Pa → m
        double frictLoss_m = edge.headLoss_m - localLoss_m;       // sürtünme kaybı
        if (frictLoss_m < 0.0) frictLoss_m = edge.headLoss_m;

        html += QString("<tr style='background:%1'>"
                        "<td>%2</td><td>%3</td><td>%4</td>"
                        "<td>%5</td><td>%6</td>"
                        "<td>%7</td><td>%8</td>"
                        "<td>%9</td><td>%10</td><td>%11</td><td>%12</td><td>%13</td></tr>")
            .arg(bg)
            .arg(edge.label.empty() ? QString("E%1").arg(eid) : QString::fromStdString(edge.label))
            .arg(typeStr)
            .arg(QString::fromStdString(edge.material))
            .arg(edge.diameter_mm, 0, 'f', 0)
            .arg(edge.length_m, 0, 'f', 2)
            .arg(qNomStr)
            .arg(qHesStr)
            .arg(edge.velocity_ms, 0, 'f', 2)
            .arg(frictLoss_m,      0, 'f', 4)
            .arg(localLoss_m,      0, 'f', 4)
            .arg(edge.headLoss_m,  0, 'f', 4)
            .arg(isCrit ? "<b style='color:#c00'>KRİTİK</b>" : "OK");

        totalHL += edge.headLoss_m;
        if (edge.headLoss_m > maxHL) { maxHL = edge.headLoss_m; criticalEdgeId = eid; }
        ++row;
    }

    html += "</table>";
    html += QString("<p><b>Toplam kayıp (supply ağı):</b> %1 m &nbsp;&nbsp;"
                    "<b>Kritik devre toplam:</b> <span style='color:#c00'>%2 m</span></p>")
        .arg(totalHL, 0, 'f', 3)
        .arg(critResult.totalHeadLoss_m, 0, 'f', 3);
    html += "</body></html>";

    QDialog dlg(this);
    dlg.setWindowTitle("Parçaların Basınç Kaybı");
    dlg.resize(860, 520);
    auto* browser = new QTextBrowser(&dlg);
    browser->setHtml(html);
    auto* savBtn = new QPushButton("PDF Kaydet", &dlg);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->addButton(savBtn, QDialogButtonBox::ActionRole);
    auto* vl = new QVBoxLayout(&dlg);
    vl->addWidget(browser);
    vl->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    connect(savBtn, &QPushButton::clicked, [&](){
        QString path = QFileDialog::getSaveFileName(&dlg, "PDF Kaydet", "", "PDF (*.pdf)");
        if (path.isEmpty()) return;
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
                              QPageLayout::Landscape, QMarginsF(15,10,15,10)));
        browser->print(&printer);
    });
    dlg.exec();
}

// ============================================================
//  WORD DOCX RAPOR EXPORT (FineSANI Word dosyası eşdeğeri — OOXML .docx)
// ============================================================
void MainWindow::OnWordRapor() {
    if (!m_document) return;
    if (!RunPreflight()) return;

    auto& network = m_document->GetNetwork();
    mep::HydraulicSolver solver(network);
    solver.Solve();
    auto critResult = solver.CalculateCriticalPath();

    auto& pm       = core::ProjectManager::Instance();
    QString projName = QString::fromStdString(pm.GetProjectName());
    QString today    = QDate::currentDate().toString("dd.MM.yyyy");

    // Dosya yolu
    QString defaultPath;
    if (!pm.GetProjectFolder().empty())
        defaultPath = QString::fromStdString(pm.GetProjectFolder()) + "/rapor/";
    defaultPath += (projName.isEmpty() ? "rapor" : projName) + "_hesap_foyu.docx";

    QString path = QFileDialog::getSaveFileName(this, "Word Rapor Kaydet (.docx)",
                                                defaultPath,
                                                "Word Belgesi (*.docx);;Tum Dosyalar (*)");
    if (path.isEmpty()) return;

    core::DocxReportParams params;
    params.projectName    = projName.toStdString();
    params.norm           = m_devreParams.norm.toStdString();
    params.mainMaterial   = m_devreParams.mainPipeMat.toStdString();
    params.branchMaterial = m_devreParams.branchPipeMat.toStdString();
    params.buildingType   = m_devreParams.buildingType.toStdString();
    params.roughness_mm   = m_devreParams.roughness_mm;
    params.maxVelocity_ms = m_devreParams.maxVelocity_ms;
    params.date           = today.toStdString();

    if (core::DocxWriter::Write(path.toStdString(), network, critResult, params)) {
        if (m_logList) m_logList->addItem("Word .docx rapor kaydedildi: " + path);
        statusBar()->showMessage("Word rapor kaydedildi: " + path, 4000);
        QMessageBox::information(this, "Rapor Kaydedildi",
            QString("Word belgesi kaydedildi:\n%1\n\nMicrosoft Word ile acilabiilir.").arg(path));
    } else {
        QMessageBox::warning(this, "Hata", "Dosya yazilimadi: " + path);
    }
}

// ============================================================
//  YAGMUR SUYU MODULU (EN 12056-3)
// ============================================================
void MainWindow::OnYagmurSuyu() {
    // Alan girisi
    bool ok = false;
    double area = QInputDialog::getDouble(this,
        "Yagmur Suyu — Cati Alani",
        "Tahliye edilecek cati/zemin alani (m²):",
        100.0, 0.1, 100000.0, 1, &ok);
    if (!ok) return;

    QStringList surfaceTypes;
    surfaceTypes << "Cati (C=1.0)"
                 << "Beton/Asfalt (C=0.9)"
                 << "Yeşil çati (C=0.5)"
                 << "Çakilli zemin (C=0.6)";
    QString surface = QInputDialog::getItem(this,
        "Yuzey Tipi", "Yuzey/ drenaj tipi:", surfaceTypes, 0, false, &ok);
    if (!ok) return;

    double C = 1.0;
    if (surface.contains("Beton")) C = 0.9;
    else if (surface.contains("Yeşil") || surface.contains("yesil")) C = 0.5;
    else if (surface.contains("akilli") || surface.contains("Çakil")) C = 0.6;

    // EN 12056-3: tasarim yagmur yuku r_D = 0.03 l/(s*m²) — Turkiye 2-yillik
    double rD = 0.03;
    double Q_ls = C * area * rD; // l/s

    // Standart drenaj DN secimi (Manning, %2 egim)
    static const struct { double dn; double cap_ls; } kTable[] = {
        {50, 1.2}, {75, 3.5}, {100, 8.0}, {125, 15.0},
        {150, 25.0}, {200, 50.0}, {0, 0}
    };
    double selectedDN = 200.0;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= Q_ls) { selectedDN = kTable[i].dn; break; }
    }

    // Yagmurluk borusu sayisi (tek boru maks 50 l/s kabul)
    int numPipes = (Q_ls > 50.0) ? (int)std::ceil(Q_ls / 50.0) : 1;
    double qPerPipe = Q_ls / numPipes;
    // Her boru icin DN yeniden sec
    double dnPerPipe = 200.0;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= qPerPipe) { dnPerPipe = kTable[i].dn; break; }
    }

    QString msg;
    msg += QString("<h3>Yagmur Suyu Tahliye Hesabi (EN 12056-3)</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td>Alan:</td><td><b>%1 m²</b></td></tr>").arg(area, 0, 'f', 1);
    msg += QString("<tr><td>Yuzey katsayisi (C):</td><td>%1</td></tr>").arg(C, 0, 'f', 2);
    msg += QString("<tr><td>Tasarim yagmur yuku (r_D):</td><td>%1 l/(s·m²)</td></tr>").arg(rD, 0, 'f', 3);
    msg += QString("<tr><td><b>Tasarim debisi (Q):</b></td><td><font color='red'><b>%1 l/s</b></font></td></tr>").arg(Q_ls, 0, 'f', 2);
    msg += QString("<tr><td>Onerilen boru:</td><td><b>%1x DN%2</b></td></tr>").arg(numPipes).arg((int)dnPerPipe);
    msg += QString("</table><hr>");
    msg += QString("<small>r_D = 0.03 l/(s·m²) — Turkiye iklim bolgesi (2 yillik donus periyodu)<br>");
    msg += QString("Egim: %%2 min — Manning n=0.012 (PVC)</small>");

    QMessageBox dlg(this);
    dlg.setWindowTitle("Yagmur Suyu Modulu (EN 12056-3)");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(QMessageBox::Information);
    dlg.exec();

    if (m_logList) {
        m_logList->addItem(QString("Yagmur Suyu: A=%1m2, Q=%2l/s → %3x DN%4")
            .arg(area,0,'f',0).arg(Q_ls,0,'f',2).arg(numPipes).arg((int)dnPerPipe));
    }
}

// ============================================================
//  KEŞİF LİSTESİ / BOM (Bill of Materials)
// ============================================================
void MainWindow::OnBOM() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Kesif Listesi", "Oncelikle tesisat sebekesi cizin.");
        return;
    }

    auto& db = mep::Database::Instance();
    auto dnPriceFactor = [](int dn) -> double {
        if (dn <= 16) return 0.8; if (dn <= 20) return 1.0; if (dn <= 25) return 1.3;
        if (dn <= 32) return 1.7; if (dn <= 40) return 2.3; if (dn <= 50) return 3.0;
        if (dn <= 63) return 4.0; if (dn <= 75) return 5.5; if (dn <= 90) return 7.0;
        return 10.0;
    };

    // Disiplin isimleri
    auto edgeTypeName = [](mep::EdgeType t) -> QString {
        switch (t) {
            case mep::EdgeType::Supply:        return "Temiz Su";
            case mep::EdgeType::HotWater:      return "Sıcak Su";
            case mep::EdgeType::Drainage:      return "Pis Su / Drenaj";
            case mep::EdgeType::Vent:          return "Havalandırma Borusu";
            case mep::EdgeType::Gas:           return "Doğal Gaz";
            case mep::EdgeType::Heating:       return "Isıtma Gidiş";
            case mep::EdgeType::HeatingReturn: return "Isıtma Dönüş";
            case mep::EdgeType::FireLine:      return "Yangın Hattı";
            case mep::EdgeType::Electric:      return "Elektrik";
            case mep::EdgeType::Duct:          return "HVAC Kanal";
            default: return "Diğer";
        }
    };

    // Disiplin bazlı gruplama: EdgeType → {DN/kesit → (uzunluk, adet)}
    struct SizeGroup { double length_m = 0; int count = 0; std::string material; };
    std::map<mep::EdgeType, std::map<std::string, SizeGroup>> byDiscipline;

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        std::string sizeKey;
        if (edge.IsRectangularDuct()) {
            sizeKey = std::to_string((int)edge.ductWidth_mm) + "x"
                    + std::to_string((int)edge.ductHeight_mm);
        } else {
            sizeKey = "DN" + std::to_string((int)std::round(edge.diameter_mm));
        }
        auto& sg = byDiscipline[edge.type][sizeKey];
        sg.length_m += edge.length_m;
        sg.count++;
        if (sg.material.empty()) sg.material = edge.material;
    }

    // Bağlantı elemanları
    int nTee = 0, nElbow = 0, nFixture = 0, nSource = 0, nDrain = 0;
    int nAHU = 0, nDiffuser = 0, nPlenum = 0, nDamper = 0;
    for (const auto& [nid, node] : network.GetNodeMap()) {
        auto edges = network.GetConnectedEdges(nid);
        int deg = (int)edges.size();
        switch (node.type) {
            case mep::NodeType::Fixture:  ++nFixture; break;
            case mep::NodeType::Source:   ++nSource;  break;
            case mep::NodeType::Drain:    ++nDrain;   break;
            case mep::NodeType::AHU:      ++nAHU;     break;
            case mep::NodeType::Diffuser: ++nDiffuser; break;
            case mep::NodeType::Plenum:   ++nPlenum;  break;
            case mep::NodeType::Damper:   ++nDamper;  break;
            case mep::NodeType::Junction:
                if (deg >= 3) ++nTee; else if (deg == 2) ++nElbow;
                break;
            default: break;
        }
    }

    // HTML rapor
    QString msg;
    msg += "<h3>Keşif Listesi / Malzeme Dökümü (BOM)</h3>";
    msg += QString("<small>Toplam %1 disiplin, %2 hat, %3 düğüm</small><br><br>")
        .arg(byDiscipline.size()).arg(network.GetEdgeCount()).arg(network.GetNodeCount());

    double grandTotalLength = 0, grandTotalCost = 0;

    for (const auto& [eType, sizeMap] : byDiscipline) {
        QString typeName = edgeTypeName(eType);
        bool isDuct = (eType == mep::EdgeType::Duct);
        QString sizeHeader = isDuct ? "Boyut" : "DN (mm)";

        msg += QString("<b style='color:%1'>■ %2</b><br>")
            .arg(isDuct ? "#2ecc71" : eType == mep::EdgeType::Supply ? "#3498db"
                : eType == mep::EdgeType::HotWater ? "#e74c3c"
                : eType == mep::EdgeType::Drainage ? "#8b4513"
                : eType == mep::EdgeType::Gas ? "#f1c40f"
                : eType == mep::EdgeType::FireLine ? "#c0392b"
                : eType == mep::EdgeType::Electric ? "#e67e22" : "#95a5a6")
            .arg(typeName);

        msg += "<table border='1' cellspacing='0' cellpadding='3'>";
        msg += QString("<tr><th>%1</th><th>Malzeme</th><th>Uzunluk (m)</th><th>Parça</th><th>Tahmini Maliyet (TL)</th></tr>")
            .arg(sizeHeader);

        double discLen = 0, discCost = 0;
        for (const auto& [sizeKey, sg] : sizeMap) {
            double price = db.GetPipe(sg.material).unitPrice_TL;
            if (!isDuct) {
                int dn = 20;
                try { dn = std::stoi(sizeKey.substr(2)); } catch (...) {}
                price *= dnPriceFactor(dn);
            } else {
                price = std::max(price, 130.0);
            }
            double cost = sg.length_m * price;
            discLen  += sg.length_m;
            discCost += cost;
            msg += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(QString::fromStdString(sizeKey))
                .arg(QString::fromStdString(sg.material))
                .arg(sg.length_m, 0, 'f', 2).arg(sg.count)
                .arg(cost, 0, 'f', 0);
        }
        msg += QString("<tr style='background:#e8f4e8'><td colspan='2'><b>Alt Toplam</b></td>"
                       "<td><b>%1 m</b></td><td><b>%2</b></td><td><b>%3 TL</b></td></tr>")
            .arg(discLen, 0, 'f', 2).arg((int)sizeMap.size()).arg(discCost, 0, 'f', 0);
        msg += "</table><br>";
        grandTotalLength += discLen;
        grandTotalCost   += discCost;
    }

    // Fire + işçilik
    constexpr double kWaste = 7.0, kLabor = 35.0;
    double wasteCost = grandTotalCost * kWaste / 100.0;
    double laborCost = grandTotalCost * kLabor / 100.0;
    double grandTotal = grandTotalCost + wasteCost + laborCost;

    msg += "<b>Maliyet Özeti:</b><br><table border='1' cellspacing='0' cellpadding='3'>";
    msg += QString("<tr><td>Malzeme (net)</td><td style='text-align:right'><b>%1 TL</b></td></tr>").arg(grandTotalCost, 0, 'f', 0);
    msg += QString("<tr><td>Fire payı (%1%)</td><td style='text-align:right'>%2 TL</td></tr>").arg(kWaste, 0, 'f', 0).arg(wasteCost, 0, 'f', 0);
    msg += QString("<tr><td>İşçilik (%1%)</td><td style='text-align:right'>%2 TL</td></tr>").arg(kLabor, 0, 'f', 0).arg(laborCost, 0, 'f', 0);
    msg += QString("<tr style='background:#fff3cd'><td><b>GENEL TOPLAM</b></td><td style='text-align:right'><b>%1 TL</b></td></tr>").arg(grandTotal, 0, 'f', 0);
    msg += "</table><br>";

    // Bağlantı elemanları — tüm disiplinler
    msg += "<b>Bağlantı Elemanları:</b><br><table border='1' cellspacing='0' cellpadding='3'>";
    msg += "<tr><th>Eleman</th><th>Adet</th></tr>";
    if (nTee)      msg += QString("<tr><td>T-parça</td><td>%1</td></tr>").arg(nTee);
    if (nElbow)    msg += QString("<tr><td>Dirsek</td><td>%1</td></tr>").arg(nElbow);
    if (nFixture)  msg += QString("<tr><td>Armatür bağlantısı</td><td>%1</td></tr>").arg(nFixture);
    if (nSource)   msg += QString("<tr><td>Su kaynağı</td><td>%1</td></tr>").arg(nSource);
    if (nDrain)    msg += QString("<tr><td>Tahliye noktası</td><td>%1</td></tr>").arg(nDrain);
    if (nAHU)      msg += QString("<tr><td>Klima santrali (AHU)</td><td>%1</td></tr>").arg(nAHU);
    if (nDiffuser) msg += QString("<tr><td>Difüzör / Menfez</td><td>%1</td></tr>").arg(nDiffuser);
    if (nPlenum)   msg += QString("<tr><td>Plenum kutusu</td><td>%1</td></tr>").arg(nPlenum);
    if (nDamper)   msg += QString("<tr><td>Damper</td><td>%1</td></tr>").arg(nDamper);
    msg += "</table>";

    QDialog dlg(this);
    dlg.setWindowTitle("Keşif Listesi / BOM — Disiplin Bazlı");
    dlg.resize(600, 550);
    auto* layout = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setHtml(msg);
    layout->addWidget(browser);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(btn);

    if (m_logList) {
        m_logList->addItem(QString("BOM: %1 m toplam, %2 disiplin, %3 TL")
            .arg(grandTotalLength, 0, 'f', 2).arg(byDiscipline.size()).arg(grandTotal, 0, 'f', 0));
    }
    dlg.exec();
}

// ============================================================
//  KOLON ŞEMASI (RISER DIAGRAM)
// ============================================================
void MainWindow::OnRiserDiagram() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Kolon Semasi",
            "Once tesisat sebekesi cizin (boru + kaynak + armaturler).");
        return;
    }
    if (!RunPreflight()) return;

    mep::RiserDiagram riser(network, m_floorManager);
    auto data = riser.Generate();
    std::string svgContent = riser.ToSVG(data);
    std::string txtContent = riser.ToText(data);

    QDialog dlg(this);
    dlg.setWindowTitle("Kolon Semasi (Riser Diagram)");
    dlg.resize(720, 520);

    auto* layout   = new QVBoxLayout(&dlg);
    auto* browser  = new QTextBrowser(&dlg);
    browser->setOpenLinks(false);
    // SVG'yi HTML iframe olarak goster
    QString svgHtml = QString(
        "<html><body style='margin:0;background:#1a1a2e;'>"
        "<div style='padding:8px;'>%1</div>"
        "<pre style='color:#ccc;font-size:11px;padding:8px;'>%2</pre>"
        "</body></html>"
    ).arg(QString::fromStdString(svgContent))
     .arg(QString::fromStdString(txtContent).toHtmlEscaped());

    browser->setHtml(svgHtml);
    layout->addWidget(browser);

    auto* btnRow    = new QHBoxLayout();
    auto* btnSvg    = new QPushButton("SVG Kaydet...", &dlg);
    auto* btnPdf    = new QPushButton("PDF Kaydet...", &dlg);
    auto* btnDxf    = new QPushButton("DXF Kaydet...", &dlg);
    auto* btnClose  = new QPushButton("Kapat", &dlg);
    btnRow->addWidget(btnSvg);
    btnRow->addWidget(btnPdf);
    btnRow->addWidget(btnDxf);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    layout->addLayout(btnRow);

    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    // SVG kaydet
    connect(btnSvg, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — SVG Kaydet", startDir, "SVG Dosyasi (*.svg)");
        if (path.isEmpty()) return;
        std::ofstream f(path.toStdString());
        if (f.is_open()) {
            f << svgContent;
            statusBar()->showMessage(QString("SVG kaydedildi: %1").arg(path), 3000);
        } else {
            QMessageBox::critical(&dlg, "Hata", "Dosya yazılamadi!");
        }
    });

    // PDF kaydet (QPrinter + QSvgRenderer)
    connect(btnPdf, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — PDF Kaydet", startDir, "PDF Dosyasi (*.pdf)");
        if (path.isEmpty()) return;

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageSize(QPageSize(QPageSize::A3));
        printer.setPageOrientation(QPageLayout::Landscape);

        QSvgRenderer renderer(QByteArray::fromStdString(svgContent));
        if (!renderer.isValid()) {
            QMessageBox::critical(&dlg, "PDF Hatasi", "SVG gecersiz — PDF olusturulamadi.");
            return;
        }

        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::critical(&dlg, "PDF Hatasi", "PDF yazici baslatılamadi.");
            return;
        }
        QRectF pageRect = printer.pageLayout().paintRectPixels(printer.resolution());
        renderer.render(&painter, pageRect);
        painter.end();

        statusBar()->showMessage(QString("PDF kaydedildi: %1").arg(path), 3000);
    });

    // DXF kaydet — R12 format, LINE + TEXT entity'leri
    connect(btnDxf, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Kolon Semasi — DXF Kaydet", startDir, "DXF Dosyasi (*.dxf)");
        if (path.isEmpty()) return;

        std::ofstream f(path.toStdString());
        if (!f.is_open()) {
            QMessageBox::critical(&dlg, "Hata", "DXF dosyasi yazilamadi!");
            return;
        }

        // DXF R12 minimal format
        f << "0\nSECTION\n2\nHEADER\n0\nENDSEC\n";
        f << "0\nSECTION\n2\nENTITIES\n";

        for (const auto& ln : data.lines) {
            f << "0\nLINE\n8\n0\n";
            f << "10\n" << ln.a.x << "\n20\n" << -ln.a.y << "\n30\n0.0\n";
            f << "11\n" << ln.b.x << "\n21\n" << -ln.b.y << "\n31\n0.0\n";
        }
        for (const auto& lb : data.labels) {
            f << "0\nTEXT\n8\n0\n";
            f << "10\n" << lb.x << "\n20\n" << -lb.y << "\n30\n0.0\n";
            f << "40\n" << lb.fontSize << "\n";
            f << "1\n" << lb.text << "\n";
        }

        f << "0\nENDSEC\n0\nEOF\n";
        statusBar()->showMessage(QString("DXF kaydedildi: %1").arg(path), 3000);
    });

    if (m_logList)
        m_logList->addItem(QString("Kolon semasi: %1 kolon, %2 kat")
            .arg((int)data.columns.size()).arg((int)m_floorManager.GetFloorCount()));

    dlg.exec();
}

// ============================================================
//  HESAP FÖYÜ — DN MANUEL OVERRIDE
// ============================================================
void MainWindow::OnDNOverride() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "DN Override",
            "Oncelikle tesisat sebekesi cizin.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Hesap Foyu — DN Manuel Override");
    dlg.resize(650, 400);

    auto* layout = new QVBoxLayout(&dlg);
    auto* info   = new QLabel(
        "<b>Boru caplarinizi asagidaki tablodan duzenleyebilirsiniz.</b><br>"
        "Tamam'a basinca secilen degerler aninda uygulanir ve DN etiketleri guncellenir.", &dlg);
    info->setWordWrap(true);
    layout->addWidget(info);

    // Tablo: Boru ID | Tip | Malzeme | Mevcut DN | Yeni DN (editable)
    auto* table = new QTableWidget(&dlg);
    static const QStringList kDNList = {
        "16","20","25","32","40","50","63","75","90","110","125","160","200"
    };

    // Edge'leri sabit sirayla al
    std::vector<std::pair<uint32_t, const mep::Edge*>> edges;
    for (const auto& [eid, edge] : network.GetEdgeMap())
        edges.emplace_back(eid, &edge);
    std::sort(edges.begin(), edges.end(),
        [](const auto& a, const auto& b){ return a.first < b.first; });

    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"Boru ID","Tip","Malzeme","Mevcut DN","Yeni DN"});
    table->setRowCount(static_cast<int>(edges.size()));
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    for (int row = 0; row < static_cast<int>(edges.size()); ++row) {
        auto [eid, edge] = edges[row];
        // Boru ID
        auto* idItem = new QTableWidgetItem(QString::number(eid));
        idItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 0, idItem);
        // Tip
        QString tipStr = (edge->type == mep::EdgeType::Supply) ? "Temiz Su"
                       : (edge->type == mep::EdgeType::Drainage) ? "Pis Su" : "Hava";
        auto* tipItem = new QTableWidgetItem(tipStr);
        tipItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 1, tipItem);
        // Malzeme
        auto* matItem = new QTableWidgetItem(QString::fromStdString(edge->material));
        matItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 2, matItem);
        // Mevcut DN
        int curDN = static_cast<int>(std::round(edge->diameter_mm));
        auto* curItem = new QTableWidgetItem(QString("DN%1").arg(curDN));
        curItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, 3, curItem);
        // Yeni DN: ComboBox
        auto* combo = new QComboBox(&dlg);
        combo->addItems(kDNList);
        // Mevcut secimi bul
        int curIdx = kDNList.indexOf(QString::number(curDN));
        if (curIdx < 0) curIdx = 0;
        combo->setCurrentIndex(curIdx);
        table->setCellWidget(row, 4, combo);
    }
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->resizeColumnsToContents();
    layout->addWidget(table);

    // Toplu DN değiştirme araç çubuğu
    auto* batchRow   = new QHBoxLayout();
    auto* batchLabel = new QLabel("<b>Seçili satırlara toplu uygula:</b>", &dlg);
    auto* batchCombo = new QComboBox(&dlg);
    batchCombo->addItems(kDNList);
    auto* batchBtn   = new QPushButton("Seçilenlere Uygula", &dlg);
    batchBtn->setToolTip("Tabloda işaretlenen tüm satırların DN değerini aynı anda değiştirir");
    batchRow->addWidget(batchLabel);
    batchRow->addWidget(batchCombo);
    batchRow->addWidget(batchBtn);
    batchRow->addStretch();
    layout->addLayout(batchRow);

    connect(batchBtn, &QPushButton::clicked, [&]() {
        const QString targetDN = batchCombo->currentText();
        const auto selected = table->selectionModel()->selectedRows();
        if (selected.isEmpty()) {
            QMessageBox::information(&dlg, "Toplu DN",
                "Lütfen önce tabloda satır seçin (Ctrl+Tık veya Shift+Tık).");
            return;
        }
        for (const auto& idx : selected) {
            auto* combo = qobject_cast<QComboBox*>(table->cellWidget(idx.row(), 4));
            if (combo) combo->setCurrentText(targetDN);
        }
        statusBar()->showMessage(
            QString("%1 satira DN%2 atandi").arg(selected.size()).arg(targetDN), 2000);
    });

    auto* btnRow    = new QHBoxLayout();
    auto* btnExport = new QPushButton("XLS Olarak Kaydet...", &dlg);
    auto* btnApply  = new QPushButton("Tamam (Uygula)", &dlg);
    auto* btnCancel = new QPushButton("Iptal", &dlg);
    btnRow->addWidget(btnExport);
    btnRow->addStretch();
    btnRow->addWidget(btnApply);
    btnRow->addWidget(btnCancel);
    layout->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    // XLS export — mevcut ag durumunu yaz (solver once calistirilmali)
    connect(btnExport, &QPushButton::clicked, [&]() {
        auto& pm = core::ProjectManager::Instance();
        QString startDir = pm.HasActiveProject()
            ? QString::fromStdString(pm.GetRaporFolder()) : "";
        QString path = QFileDialog::getSaveFileName(&dlg,
            "Hesap Foyu Kaydet", startDir, "Excel Dosyasi (*.xls)");
        if (path.isEmpty()) return;

        // Solver'ı çalıştır (güncel flowRate/velocity/headLoss ile)
        mep::HydraulicSolver solver(network);
        solver.Solve();
        solver.SolveDrainage();

        if (mep::XLSXWriter::ExportCalculationSheet(path.toStdString(), network)) {
            statusBar()->showMessage(QString("Hesap foyu kaydedildi: %1").arg(path), 3000);
            if (m_logList) m_logList->addItem(QString("Hesap foyu XLS: %1").arg(path));
        } else {
            QMessageBox::critical(&dlg, "Hata", "XLS dosyasi yazılamadi!");
        }
    });

    connect(btnApply, &QPushButton::clicked, [&]() {
        int changed = 0;
        for (int row = 0; row < table->rowCount(); ++row) {
            uint32_t eid = table->item(row, 0)->text().toUInt();
            auto* combo  = qobject_cast<QComboBox*>(table->cellWidget(row, 4));
            if (!combo) continue;
            double newDN = combo->currentText().toDouble();
            mep::Edge* edge = network.GetEdge(eid);
            if (edge && std::abs(edge->diameter_mm - newDN) > 0.5) {
                edge->diameter_mm = newDN;
                edge->label = "DN" + std::to_string(static_cast<int>(newDN));
                ++changed;
            }
        }
        if (changed > 0) {
            m_document->SetModified(true);
            RefreshTextOverlay();
            if (m_vulkanWindow) m_vulkanWindow->requestUpdate();
            if (m_logList)
                m_logList->addItem(QString("DN Override: %1 boru guncellendi").arg(changed));
            statusBar()->showMessage(
                QString("DN Override: %1 boru capl degistirildi").arg(changed));
        }
        dlg.accept();
    });

    dlg.exec();
}

void MainWindow::OnIsometricDiagram() {
    if (!m_document) return;

    const auto& net = m_document->GetNetwork();
    if (net.GetEdgeCount() == 0) {
        QMessageBox::information(this, "İzometrik Şema", "Tesisat ağı boş.");
        return;
    }

    // 30° izometrik projeksiyon: x' = x×cos30 - y×cos30, y' = z + x×sin30 + y×sin30
    constexpr double cos30 = 0.866025;
    constexpr double sin30 = 0.5;
    constexpr double scale = 0.15; // mm → SVG px
    constexpr double zScale = 300.0; // Z amplification

    // Bounding box
    double minSX = 1e18, maxSX = -1e18, minSY = 1e18, maxSY = -1e18;
    auto project = [&](double x, double y, double z) -> std::pair<double,double> {
        double sx = (x * cos30 - y * cos30) * scale;
        double sy = -(z * zScale + x * sin30 + y * sin30) * scale;
        return {sx, sy};
    };

    // Pre-pass: bounds
    for (const auto& [nid, node] : net.GetNodeMap()) {
        auto [sx, sy] = project(node.position.x, node.position.y, node.position.z);
        minSX = std::min(minSX, sx); maxSX = std::max(maxSX, sx);
        minSY = std::min(minSY, sy); maxSY = std::max(maxSY, sy);
    }
    double margin = 40.0;
    double w = maxSX - minSX + margin * 2;
    double h = maxSY - minSY + margin * 2;
    double ox = -minSX + margin;
    double oy = -minSY + margin;

    // SVG build
    std::ostringstream svg;
    svg << std::fixed << std::setprecision(2);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << w << "' height='" << h << "' "
        << "viewBox='0 0 " << w << " " << h << "' "
        << "style='background:white;font-family:Arial,sans-serif'>\n";
    svg << "<text x='10' y='18' font-size='14' font-weight='bold'>VKT İzometrik Şema</text>\n";

    // Edge'ler
    auto getColor = [](mep::EdgeType t) -> std::string {
        switch (t) {
            case mep::EdgeType::Supply:        return "#0088ff";
            case mep::EdgeType::HotWater:      return "#ff3322";
            case mep::EdgeType::Drainage:      return "#8B5A2B";
            case mep::EdgeType::Gas:           return "#ccaa00";
            case mep::EdgeType::Heating:       return "#ff6600";
            case mep::EdgeType::HeatingReturn: return "#3366cc";
            case mep::EdgeType::FireLine:      return "#cc0000";
            case mep::EdgeType::Electric:      return "#ff8800";
            case mep::EdgeType::Duct:          return "#44aa44";
            default:                           return "#888888";
        }
    };

    for (const auto& edge : net.GetEdges()) {
        const mep::Node* nA = net.GetNode(edge.nodeA);
        const mep::Node* nB = net.GetNode(edge.nodeB);
        if (!nA || !nB) continue;
        auto [x1,y1] = project(nA->position.x, nA->position.y, nA->position.z);
        auto [x2,y2] = project(nB->position.x, nB->position.y, nB->position.z);
        svg << "<line x1='" << x1+ox << "' y1='" << y1+oy
            << "' x2='" << x2+ox << "' y2='" << y2+oy
            << "' stroke='" << getColor(edge.type)
            << "' stroke-width='2' stroke-linecap='round'/>\n";

        // DN label
        double mx = (x1+x2)*0.5 + ox, my = (y1+y2)*0.5 + oy;
        svg << "<text x='" << mx+3 << "' y='" << my-3
            << "' font-size='7' fill='" << getColor(edge.type)
            << "'>DN" << static_cast<int>(edge.diameter_mm) << "</text>\n";
    }

    // Node'lar
    for (const auto& [nid, node] : net.GetNodeMap()) {
        auto [sx, sy] = project(node.position.x, node.position.y, node.position.z);
        double cx = sx + ox, cy = sy + oy;
        std::string fill = "#333333";
        double r = 3.0;
        if (node.type == mep::NodeType::Fixture)      { fill = "#0066cc"; r = 4; }
        else if (node.type == mep::NodeType::Source)   { fill = "#00aa00"; r = 5; }
        else if (node.type == mep::NodeType::Drain)    { fill = "#8B4513"; r = 5; }
        svg << "<circle cx='" << cx << "' cy='" << cy
            << "' r='" << r << "' fill='" << fill << "'/>\n";
        if (!node.label.empty() && node.type != mep::NodeType::Junction)
            svg << "<text x='" << cx+5 << "' y='" << cy-5
                << "' font-size='7' fill='#444'>" << node.label << "</text>\n";
    }

    svg << "</svg>";
    std::string svgStr = svg.str();

    // Dialog göster
    QDialog dlg(this);
    dlg.setWindowTitle("İzometrik Şema (30° aksonometrik)");
    dlg.setMinimumSize(800, 600);
    auto* vl = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setHtml(QString::fromStdString(svgStr));
    vl->addWidget(browser, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    auto* btnSvg = bb->addButton("SVG Kaydet", QDialogButtonBox::ActionRole);
    auto* btnPdf = bb->addButton("PDF Kaydet", QDialogButtonBox::ActionRole);

    connect(btnSvg, &QPushButton::clicked, this, [&]() {
        QString path = QFileDialog::getSaveFileName(this, "SVG Kaydet", "", "SVG (*.svg)");
        if (!path.isEmpty()) {
            QFile f(path); if (f.open(QIODevice::WriteOnly))
                f.write(QByteArray::fromStdString(svgStr));
            statusBar()->showMessage("İzometrik SVG: " + path, 4000);
        }
    });
    connect(btnPdf, &QPushButton::clicked, this, [&]() {
        QString path = QFileDialog::getSaveFileName(this, "PDF Kaydet", "", "PDF (*.pdf)");
        if (!path.isEmpty()) {
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(path);
            printer.setPageOrientation(QPageLayout::Landscape);
            QSvgRenderer renderer(QByteArray::fromStdString(svgStr));
            QPainter painter(&printer);
            renderer.render(&painter);
            painter.end();
            statusBar()->showMessage("İzometrik PDF: " + path, 4000);
        }
    });
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

// ═══════════════════════════════════════════════════════════
//  GÜNEŞ KOLEKTÖRü BOYUTLANDIRMA — TS EN 12975 / TS EN 12977
// ═══════════════════════════════════════════════════════════
void MainWindow::OnSolarCollector() {
    QDialog dlg(this);
    dlg.setWindowTitle("Güneş Kolektörü Boyutlandırma");
    dlg.setMinimumWidth(420);
    auto* vl = new QVBoxLayout(&dlg);

    auto* form = new QFormLayout;
    auto* edPersons = new QSpinBox; edPersons->setRange(1, 500); edPersons->setValue(4);
    auto* edDailyL  = new QSpinBox; edDailyL->setRange(20, 100); edDailyL->setValue(50);
    auto* cbCity    = new QComboBox;
    cbCity->addItems({"İstanbul (1400 kWh/m²)", "Ankara (1550 kWh/m²)", "Antalya (1700 kWh/m²)",
                      "İzmir (1600 kWh/m²)", "Trabzon (1200 kWh/m²)"});
    auto* edEfficiency = new QSpinBox; edEfficiency->setRange(30, 90); edEfficiency->setValue(60);
    edEfficiency->setSuffix(" %");
    form->addRow("Kişi sayısı:", edPersons);
    form->addRow("Kişi başı günlük su (L):", edDailyL);
    form->addRow("Güneşlenme bölgesi:", cbCity);
    form->addRow("Kolektör verimi:", edEfficiency);
    vl->addLayout(form);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    int persons = edPersons->value();
    double dailyL = edDailyL->value();
    double irradiance[] = {1400, 1550, 1700, 1600, 1200};
    double G = irradiance[cbCity->currentIndex()]; // kWh/m²/yıl
    double eta = edEfficiency->value() / 100.0;
    double dT = 35.0; // °C (10→45)
    double cp = 4.186; // kJ/(kg·K)

    double dailyEnergy_kWh = persons * dailyL * dT * cp / 3600.0;
    double annualEnergy_kWh = dailyEnergy_kWh * 365.0;
    double solarFraction = 0.65; // %65 solar katkı
    double collectorArea = (annualEnergy_kWh * solarFraction) / (G * eta);
    double tankVolume_L = persons * dailyL * 1.5;
    int panelCount = static_cast<int>(std::ceil(collectorArea / 2.0)); // 2m² panel

    QString result = QString(
        "<h3>Güneş Kolektörü Boyutlandırma Sonuçları</h3>"
        "<table border='1' cellpadding='4' style='border-collapse:collapse'>"
        "<tr><td><b>Günlük sıcak su ihtiyacı</b></td><td>%1 L/gün</td></tr>"
        "<tr><td><b>Günlük enerji ihtiyacı</b></td><td>%2 kWh/gün</td></tr>"
        "<tr><td><b>Yıllık enerji ihtiyacı</b></td><td>%3 kWh/yıl</td></tr>"
        "<tr><td><b>Solar katkı oranı</b></td><td>%65</td></tr>"
        "<tr><td><b>Gerekli kolektör alanı</b></td><td><b>%4 m²</b></td></tr>"
        "<tr><td><b>Panel sayısı (2m²)</b></td><td><b>%5 adet</b></td></tr>"
        "<tr><td><b>Depo hacmi</b></td><td><b>%6 L</b></td></tr>"
        "</table>"
    ).arg(persons * static_cast<int>(dailyL))
     .arg(dailyEnergy_kWh, 0, 'f', 1)
     .arg(annualEnergy_kWh, 0, 'f', 0)
     .arg(collectorArea, 0, 'f', 1)
     .arg(panelCount)
     .arg(static_cast<int>(tankVolume_L));

    QMessageBox::information(this, "Güneş Kolektörü", result);
    if (m_logList) m_logList->addItem(QString("[GUNES] %1m² kolektör, %2L depo").arg(collectorArea,0,'f',1).arg((int)tankVolume_L));
}

// ═══════════════════════════════════════════════════════════
//  ISI POMPASI BOYUTLANDIRMA — EN 14511 / EN 15450
// ═══════════════════════════════════════════════════════════
void MainWindow::OnHeatPump() {
    QDialog dlg(this);
    dlg.setWindowTitle("Isı Pompası Boyutlandırma");
    dlg.setMinimumWidth(420);
    auto* vl = new QVBoxLayout(&dlg);

    auto* form = new QFormLayout;
    auto* edArea   = new QSpinBox; edArea->setRange(10, 10000); edArea->setValue(120); edArea->setSuffix(" m²");
    auto* edHeatLoss = new QSpinBox; edHeatLoss->setRange(10, 200); edHeatLoss->setValue(50); edHeatLoss->setSuffix(" W/m²");
    auto* cbType  = new QComboBox;
    cbType->addItems({"Hava-Su (COP 3.5)", "Toprak-Su (COP 4.5)", "Su-Su (COP 5.0)"});
    auto* edHotWater = new QSpinBox; edHotWater->setRange(0, 5000); edHotWater->setValue(200); edHotWater->setSuffix(" L/gün");
    form->addRow("Isıtma alanı:", edArea);
    form->addRow("Özgül ısı kaybı:", edHeatLoss);
    form->addRow("Pompa tipi:", cbType);
    form->addRow("Sıcak su ihtiyacı:", edHotWater);
    vl->addLayout(form);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    double area = edArea->value();
    double qSpec = edHeatLoss->value();
    double cop[] = {3.5, 4.5, 5.0};
    double COP = cop[cbType->currentIndex()];
    double hotWaterL = edHotWater->value();

    double heatLoad_kW = area * qSpec / 1000.0;
    double hwLoad_kW = hotWaterL * 35.0 * 4.186 / (3600.0 * 8.0); // 8 saat spread
    double totalLoad_kW = heatLoad_kW + hwLoad_kW;
    double elecPower_kW = totalLoad_kW / COP;
    double annualEnergy_kWh = totalLoad_kW * 2000.0; // 2000 saat/yıl
    double annualElec_kWh = annualEnergy_kWh / COP;

    // Standart model önerisi
    struct HPModel { const char* name; double kW; };
    static const HPModel models[] = {
        {"VKT-HP05", 5}, {"VKT-HP08", 8}, {"VKT-HP12", 12},
        {"VKT-HP16", 16}, {"VKT-HP22", 22}, {"VKT-HP30", 30},
        {"VKT-HP45", 45}, {"VKT-HP60", 60}
    };
    std::string modelName = "VKT-HP60";
    for (auto& m : models) {
        if (m.kW >= totalLoad_kW) { modelName = m.name; break; }
    }

    QString result = QString(
        "<h3>Isı Pompası Boyutlandırma — EN 14511</h3>"
        "<table border='1' cellpadding='4' style='border-collapse:collapse'>"
        "<tr><td><b>Isıtma yükü</b></td><td>%1 kW</td></tr>"
        "<tr><td><b>Sıcak su yükü</b></td><td>%2 kW</td></tr>"
        "<tr><td><b>Toplam termal yük</b></td><td><b>%3 kW</b></td></tr>"
        "<tr><td><b>COP (%4)</b></td><td>%5</td></tr>"
        "<tr><td><b>Elektrik gücü</b></td><td>%6 kW</td></tr>"
        "<tr><td><b>Yıllık enerji</b></td><td>%7 kWh</td></tr>"
        "<tr><td><b>Yıllık elektrik</b></td><td>%8 kWh</td></tr>"
        "<tr><td><b>Önerilen model</b></td><td><b>%9</b></td></tr>"
        "</table>"
    ).arg(heatLoad_kW, 0, 'f', 1)
     .arg(hwLoad_kW, 0, 'f', 1)
     .arg(totalLoad_kW, 0, 'f', 1)
     .arg(cbType->currentText())
     .arg(COP)
     .arg(elecPower_kW, 0, 'f', 1)
     .arg(annualEnergy_kWh, 0, 'f', 0)
     .arg(annualElec_kWh, 0, 'f', 0)
     .arg(QString::fromStdString(modelName));

    QMessageBox::information(this, "Isı Pompası", result);
    if (m_logList) m_logList->addItem(QString("[ISI-POMPASI] %1 kW, model=%2").arg(totalLoad_kW,0,'f',1).arg(QString::fromStdString(modelName)));
}

void MainWindow::OnCizimiGuncelle() {
    if (!m_document) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Cizimi Guncelle — Hesap Sonuclari");
    dlg.setMinimumWidth(340);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel("<b>Cizime yazilacak degerler:</b>"));

    auto* cbDN       = new QCheckBox("Boru capi (DN / mm)");         cbDN->setChecked(true);
    auto* cbLen      = new QCheckBox("Boru boyu (L)");               cbLen->setChecked(true);
    auto* cbSlope    = new QCheckBox("Egim i (%) — Pis Su boru");    cbSlope->setChecked(true);
    auto* cbFillRate = new QCheckBox("Doluluk h/d (%) — Pis Su");    cbFillRate->setChecked(true);
    lay->addWidget(cbDN);
    lay->addWidget(cbLen);
    lay->addWidget(cbSlope);
    lay->addWidget(cbFillRate);

    lay->addWidget(new QLabel(
        "<small><i>Cizimi Guncelle: Auto-Hydro yeniden kosturulur,<br>"
        "secilen degerler overlay etiketlerinde gosterilir.</i></small>"));

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    // Seçilen etiket flaglerini güncelle
    if (cbDN->isChecked())       m_labelShowDN     = true;
    if (cbLen->isChecked())      m_labelShowLength  = true;
    if (cbSlope->isChecked())    m_labelShowSlope   = true;
    if (cbFillRate->isChecked()) m_labelShowFillRate = true;

    // Hesabı yeniden çalıştır + overlay yenile
    ScheduleAutoHydro();
    statusBar()->showMessage("Cizim guncelleniyor — hesap yeniden kosturuluyor...", 3000);
}

// ============================================================
//  KAPALI CUKUR / FOSEPTIK HESABI (TS 822 / EN 12566-1)
// ============================================================
void MainWindow::OnFoseptik() {
    QDialog dlg(this);
    dlg.setWindowTitle("Kapali Cukur / Foseptik Hesabi — TS 822 / EN 12566-1");
    dlg.setMinimumWidth(440);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Foseptik Hacim Hesabi</b><br>"
        "<small>Standart: TS 822 + EN 12566-1</small>"));

    auto* form = new QFormLayout();

    auto* spKisi     = new QSpinBox();   spKisi->setRange(1, 10000);  spKisi->setValue(10);  spKisi->setSuffix(" kisi");
    auto* spGunluk   = new QDoubleSpinBox(); spGunluk->setRange(0.05, 10.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spBekleme  = new QSpinBox();   spBekleme->setRange(1, 90);   spBekleme->setValue(3); spBekleme->setSuffix(" gun (bekleme suresi)");
    auto* cbCift     = new QCheckBox("Cift odacikli tasarim (EN 12566-1 Madde 5.3)");

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk su tuketimi:", spGunluk);
    form->addRow("Bekleme suresi:", spBekleme);
    form->addRow("", cbCift);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0f8ff; border:1px solid #99b; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi    = spKisi->value();
        double gunluk  = spGunluk->value();
        int    bekleme = spBekleme->value();

        // TS 822: V_total = kisi × gunluk × bekleme (min 1.5 m³)
        double V_brut = kisi * gunluk * bekleme;
        double V_net  = std::max(V_brut, 1.5);

        // Çamur hacmi — EN 12566-1: ek %30
        double V_camur = V_net * 0.30;
        double V_toplam = V_net + V_camur;

        QString html;
        html += QString("<b>Hesap Sonuclari (TS 822 / EN 12566-1)</b><br>");
        html += QString("Kisi sayisi : %1<br>").arg(kisi);
        html += QString("Gunluk debi : %1 m³/kisi/gun<br>").arg(gunluk, 0, 'f', 3);
        html += QString("Bekleme     : %1 gun<br><br>").arg(bekleme);
        html += QString("<b>Bekleme hacmi (V_net) : %1 m³</b><br>").arg(V_net, 0, 'f', 2);
        html += QString("Camur hacmi (+30%%)    : %1 m³<br>").arg(V_camur, 0, 'f', 2);
        html += QString("<b style='color:red'>TOPLAM HACIM          : %1 m³</b><br>").arg(V_toplam, 0, 'f', 2);

        if (cbCift->isChecked()) {
            double V1 = V_toplam * 0.67;
            double V2 = V_toplam * 0.33;
            html += QString("<br>Cift odacikli: 1. odacik %1 m³ / 2. odacik %2 m³<br>")
                        .arg(V1, 0, 'f', 2).arg(V2, 0, 'f', 2);
        }

        // Tahliye frekansı
        double gunler = (V_toplam / (kisi * gunluk));
        html += QString("<br>Tahliye sikligI : her ~%1 gunde bir").arg((int)gunler);

        lblResult->setText(html);
    };

    connect(spKisi,    QOverload<int>::of(&QSpinBox::valueChanged),    [&](int){ calcResult(); });
    connect(spGunluk,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spBekleme, QOverload<int>::of(&QSpinBox::valueChanged),    [&](int){ calcResult(); });
    connect(cbCift,    &QCheckBox::toggled,                            [&](bool){ calcResult(); });

    calcResult(); // İlk hesap

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);

    dlg.exec();
}

// ============================================================
//  PIS SU HESAP FOYU — drenaj devresi detay tablosu
// ============================================================
void MainWindow::OnPisSuHesapFoyu() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // Önce drenaj hesabını çalıştır
    mep::HydraulicSolver solver(network);
    solver.Solve();

    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Hesap Foyu — EN 12056-2");
    dlg.resize(820, 500);
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel("<b>Pis Su / Atik Su Devresi Hesap Foyu</b> &nbsp; <small>EN 12056-2 Manning</small>"));

    auto* tbl = new QTableWidget(0, 9, &dlg);
    tbl->setHorizontalHeaderLabels({
        "Boru No", "Boru Cinsi", "Malzeme", "DN (mm)", "L (m)",
        "DU", "Q (L/s)", "Egim i (%)", "Doluluk h/d (%)"
    });
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setAlternatingRowColors(true);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);

    int row = 0;
    for (auto& [id, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Drainage) continue;
        tbl->insertRow(row);

        double Q_Ls = edge.flowRate_m3s * 1000.0;
        double slopePct = edge.slope * 100.0;
        double fillPct  = edge.fillRate * 100.0;
        bool isCol = network.IsColumnEdge(edge.id);

        tbl->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(edge.label.empty() ? ("B-" + std::to_string(edge.id)) : edge.label)));
        tbl->setItem(row, 1, new QTableWidgetItem(isCol ? "Kolon" : "Yatay"));
        tbl->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(edge.material)));
        tbl->setItem(row, 3, new QTableWidgetItem(QString::number(edge.diameter_mm, 'f', 0)));
        tbl->setItem(row, 4, new QTableWidgetItem(QString::number(edge.length_m, 'f', 2)));
        tbl->setItem(row, 5, new QTableWidgetItem(QString::number(edge.cumulativeDU, 'f', 1)));
        tbl->setItem(row, 6, new QTableWidgetItem(QString::number(Q_Ls, 'f', 3)));
        tbl->setItem(row, 7, new QTableWidgetItem(QString::number(slopePct, 'f', 1)));

        auto* fillItem = new QTableWidgetItem(QString::number(fillPct, 'f', 0) + "%");
        if (edge.fillRate > 0.50)  // EN 12056: max %50
            fillItem->setBackground(QColor(255, 180, 180));
        else if (edge.fillRate > 0.40)
            fillItem->setBackground(QColor(255, 240, 180));
        tbl->setItem(row, 8, fillItem);

        ++row;
    }

    tbl->resizeColumnsToContents();
    lay->addWidget(tbl, 1);

    lay->addWidget(new QLabel(
        "<small>Kirmizi = EN 12056 siniri asilmis (h/d > 50%). "
        "Sari = uyari bolgesi (h/d 40-50%).</small>"));

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    dlg.exec();
}

// ============================================================
//  DXF EXPORT — tam proje (tüm katlar + MEP şebekesi)
// ============================================================
void MainWindow::OnExportDXF() {
    if (!m_document) return;
    if (!RunPreflight()) return;

    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder()) : "";

    QString path = QFileDialog::getSaveFileName(this,
        "Tam Proje — DXF Olarak Kaydet", startDir,
        "DXF Dosyasi (*.dxf)");
    if (path.isEmpty()) return;

    cad::DXFWriter writer;
    QString projName = QString::fromStdString(
        pm.HasActiveProject() ? pm.GetProjectName() : "VKT Projesi");

    bool ok = writer.Write(path.toStdString(),
                           m_document->GetCADEntities(),
                           m_document->GetNetwork(),
                           projName.toStdString(),
                           &m_document->GetBlockRegistry(),
                           &m_document->GetLayers());

    if (ok) {
        auto blockCount = m_document->GetBlockRegistry().Size();
        QString msg = QString("DXF kaydedildi: %1").arg(path);
        if (blockCount > 0)
            msg += QString(" (%1 blok tanimi dahil)").arg(blockCount);
        statusBar()->showMessage(msg, 4000);
        if (m_logList)
            m_logList->addItem(QString("DXF Export: %1").arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::critical(this, "DXF Export Hatasi", "Dosya yazılamadi!");
    }
}

// ============================================================
//  DXF EXPORT — aktif kat filtreli (FineSANI "ekran çizimi" eşdeğeri)
// ============================================================
void MainWindow::OnExportFloorDXF() {
    if (!m_document) return;

    const auto& floors = m_floorManager.GetFloors();
    if (floors.empty()) {
        QMessageBox::information(this, "Kat DXF Export",
            "Once Mimari Belirle (Ctrl+M) ile kat tanimlayin.\n"
            "Kat tanimsizsa tam proje DXF icin: Dosya → Tam Proje DXF");
        return;
    }

    // Aktif kat seçici
    QStringList katlar;
    for (const auto& f : floors)
        katlar << QString("[%1] %2 (kot=%3m)")
                     .arg(f.index).arg(QString::fromStdString(f.label))
                     .arg(f.elevation_m, 0, 'f', 2);

    bool ok2;
    QString secilen = QInputDialog::getItem(this, "Kat Secimi",
        "DXF'e aktarilacak kati secin:", katlar, m_activeFloorIndex, false, &ok2);
    if (!ok2 || secilen.isEmpty()) return;

    int katIdx = katlar.indexOf(secilen);
    if (katIdx < 0 || katIdx >= (int)floors.size()) return;
    const core::Floor& kat = floors[katIdx];

    auto& pm = core::ProjectManager::Instance();
    QString startDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetRaporFolder()) : "";

    // Varsayılan dosya adı: proje + kat etiketi
    QString defaultName = QString::fromStdString(
        pm.HasActiveProject() ? pm.GetProjectName() : "proje");
    defaultName += "_" + QString::fromStdString(kat.label)
                             .replace(" ", "_").replace(".", "");

    QString path = QFileDialog::getSaveFileName(this,
        "Kat DXF Kaydet — " + QString::fromStdString(kat.label),
        startDir + "/" + defaultName + ".dxf",
        "DXF Dosyasi (*.dxf)");
    if (path.isEmpty()) return;

    // Kat Z aralığı: elevation_m ± tolerans (kat yüksekliğinin yarısı)
    double zLo = kat.elevation_m - 0.3;
    double zHi = kat.elevation_m + kat.height_m + 0.3;

    // MEP: aktif kata ait node + edge'leri filtrele
    mep::NetworkGraph floorNet;
    std::unordered_map<uint32_t, uint32_t> nodeRemap; // eski id → yeni id

    for (const auto& [nid, node] : m_document->GetNetwork().GetNodeMap()) {
        if (node.position.z >= zLo && node.position.z <= zHi) {
            mep::Node copy = node;
            uint32_t newId = floorNet.AddNode(copy);
            nodeRemap[nid] = newId;
        }
    }
    for (const auto& [eid, edge] : m_document->GetNetwork().GetEdgeMap()) {
        auto itA = nodeRemap.find(edge.nodeA);
        auto itB = nodeRemap.find(edge.nodeB);
        if (itA == nodeRemap.end() || itB == nodeRemap.end()) continue;
        mep::Edge copy = edge;
        copy.nodeA = itA->second;
        copy.nodeB = itB->second;
        floorNet.AddEdge(copy);
    }

    // CAD arka plan: Z aralığında olan entity'leri seç
    // (import sırasında Z offset uygulandığı için kat koordinatları biliniyor)
    std::vector<std::unique_ptr<cad::Entity>> floorEntities;
    for (const auto& e : m_document->GetCADEntities()) {
        if (!e) continue;
        auto bb = e->GetBounds();
        double ez = (bb.min.z + bb.max.z) / 2.0;
        if (ez >= zLo && ez <= zHi)
            floorEntities.push_back(e->Clone());
    }

    cad::DXFWriter writer;
    bool ok3 = writer.Write(path.toStdString(),
                            floorEntities,
                            floorNet,
                            kat.label,
                            &m_document->GetBlockRegistry(),
                            &m_document->GetLayers());
    if (ok3) {
        statusBar()->showMessage(
            QString("Kat DXF kaydedildi: %1 (%2 entity, %3 boru)")
                .arg(QFileInfo(path).fileName())
                .arg((int)floorEntities.size())
                .arg((int)floorNet.GetEdgeCount()), 4000);
        if (m_logList)
            m_logList->addItem(QString("Kat DXF: %1 → %2")
                .arg(QString::fromStdString(kat.label))
                .arg(QFileInfo(path).fileName()));
    } else {
        QMessageBox::critical(this, "Hata", "DXF dosyasi yazılamadi!");
    }
}

// ============================================================
//  EMDİRME ÇUKURU HESABI (toprak emme kapasitesi)
// ============================================================
void MainWindow::OnEmdirmeCukuru() {
    QDialog dlg(this);
    dlg.setWindowTitle("Emdirme Cukuru Hesabi — Toprak Emme Kapasitesi");
    dlg.setMinimumWidth(460);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Emdirme Cukuru Boyutlandirma</b><br>"
        "<small>TS 822 Ek-B — Perkolasyon testine gore toprak emme kapasitesi</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();   spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spGunluk  = new QDoubleSpinBox(); spGunluk->setRange(0.05, 5.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spPerko   = new QDoubleSpinBox(); spPerko->setRange(1, 120); spPerko->setValue(10); spPerko->setSuffix(" dk/cm (perkolasyon testi)");
    auto* cmbToprak = new QComboBox();
    cmbToprak->addItems({"Kum-cakil (20 L/m²/gun)", "Kumlu-kil (15 L/m²/gun)",
                         "Killi (10 L/m²/gun)", "Agir kil (5 L/m²/gun)"});

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk atiksu:", spGunluk);
    form->addRow("Toprak tipi:", cmbToprak);
    form->addRow("Perkolasyon suresi:", spPerko);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#fff8e7; border:1px solid #cc9; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi   = spKisi->value();
        double gun    = spGunluk->value();
        double perko  = spPerko->value();
        int    idx    = cmbToprak->currentIndex();
        static const double caps[] = {20.0, 15.0, 10.0, 5.0}; // L/m²/gün
        double cap_m3 = caps[idx] / 1000.0; // m³/m²/gün

        double Q_gun   = kisi * gun; // m³/gün toplam atıksu
        double A_gerek = Q_gun / cap_m3; // gerekli yüzey alanı (m²)

        // Perkolasyon testine göre düzeltme (perko > 30 dk/cm → azalt)
        double duzeltme = 1.0;
        if (perko > 30) duzeltme = 1.5;
        else if (perko > 15) duzeltme = 1.25;
        A_gerek *= duzeltme;

        // Derinlik 1.5m varsayım → hacim
        double V_gerek = A_gerek * 1.5;

        QString html;
        html += QString("<b>Emdirme Cukuru Hesap Sonuclari</b><br>");
        html += QString("Toplam atiksu : %1 m³/gun<br>").arg(Q_gun, 0, 'f', 3);
        html += QString("Emme kap.     : %1 L/m²/gun<br>").arg(caps[idx], 0, 'f', 0);
        html += QString("Duzeltme fak. : x%1<br><br>").arg(duzeltme, 0, 'f', 2);
        html += QString("<b>Gerekli alan  : %1 m²</b><br>").arg(A_gerek, 0, 'f', 1);
        html += QString("<b>Gerekli hacim : %1 m³</b> (derinlik 1.5m)<br>").arg(V_gerek, 0, 'f', 2);
        html += QString("<br><i>Not: Perkolasyon testi sahada yapilarak dogrulanmalidir.</i>");
        lblResult->setText(html);
    };

    connect(spKisi,   QOverload<int>::of(&QSpinBox::valueChanged),         [&](int){ calcResult(); });
    connect(spGunluk, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spPerko,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(cmbToprak, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  PİS SU ÇUKURU HESABI (geçirimsiz depolama tankı)
// ============================================================
void MainWindow::OnPisSuCukuru() {
    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Cukuru Hesabi — Gecirimsiz Depolama Tanki");
    dlg.setMinimumWidth(460);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Pis Su Cukuru (Gecirimsiz Depolama)</b><br>"
        "<small>Kanalizasyona baglanamadiginda gecici depolama; tanker ile tahliye edilir.<br>"
        "TS 822 Md. 5 — foseptikten farki: aritma yapilmaz, sizmaz tank.</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();   spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spGunluk  = new QDoubleSpinBox(); spGunluk->setRange(0.05, 5.0); spGunluk->setValue(0.15); spGunluk->setSuffix(" m³/kisi/gun");
    auto* spAralik  = new QSpinBox();   spAralik->setRange(1, 90); spAralik->setValue(14); spAralik->setSuffix(" gun (tahliye araligi)");
    auto* spEmniyet = new QDoubleSpinBox(); spEmniyet->setRange(1.0, 2.0); spEmniyet->setValue(1.25); spEmniyet->setSingleStep(0.05); spEmniyet->setDecimals(2);

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Gunluk su tuketimi:", spGunluk);
    form->addRow("Tanker tahliye araligi:", spAralik);
    form->addRow("Emniyet katsayisi:", spEmniyet);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0fff0; border:1px solid #9b9; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi    = spKisi->value();
        double gun     = spGunluk->value();
        int    aralik  = spAralik->value();
        double emniyet = spEmniyet->value();

        double V_net   = kisi * gun * aralik;      // m³ — net depolama hacmi
        double V_toplam = V_net * emniyet;          // emniyet pay dahil
        double V_min   = std::max(V_toplam, 3.0);  // TS 822: minimum 3 m³

        // Tavsiye: silindir tank D=2m → yükseklik
        double h_sil = V_min / (3.14159 * 1.0 * 1.0); // D=2m → r=1m

        QString html;
        html += QString("<b>Pis Su Cukuru Hesap Sonuclari</b><br>");
        html += QString("Gunluk atiksu : %1 m³<br>").arg(kisi * gun, 0, 'f', 3);
        html += QString("Tahliye araligi: %1 gun<br>").arg(aralik);
        html += QString("Net hacim     : %1 m³<br>").arg(V_net, 0, 'f', 2);
        html += QString("Emniyet paylI : x%1<br>").arg(emniyet, 0, 'f', 2);
        html += QString("<b style='color:red'>HESAP HACMi   : %1 m³</b><br>").arg(V_min, 0, 'f', 2);
        html += QString("<br>Silindir tank (D=2m): yukseklik ≈ %1 m").arg(h_sil, 0, 'f', 2);
        lblResult->setText(html);
    };

    connect(spKisi,    QOverload<int>::of(&QSpinBox::valueChanged),        [&](int){ calcResult(); });
    connect(spGunluk,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),[&](double){ calcResult(); });
    connect(spAralik,  QOverload<int>::of(&QSpinBox::valueChanged),        [&](int){ calcResult(); });
    connect(spEmniyet, QOverload<double>::of(&QDoubleSpinBox::valueChanged),[&](double){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  PİS SU POMPASI BOYUTLANDIRMA
// ============================================================
void MainWindow::OnPisSuPompasi() {
    QDialog dlg(this);
    dlg.setWindowTitle("Pis Su Pompasi Boyutlandirma");
    dlg.setMinimumWidth(480);
    auto* lay = new QVBoxLayout(&dlg);

    lay->addWidget(new QLabel(
        "<b>Pis Su Lift Istasyonu / Pompa Boyutlandirma</b><br>"
        "<small>DIN EN 12050-1 — fosseptik/cukur tahliye pompasi debi + manometrik yukseklik + guc</small>"));

    auto* form = new QFormLayout();

    auto* spKisi    = new QSpinBox();       spKisi->setRange(1, 5000); spKisi->setValue(10); spKisi->setSuffix(" kisi");
    auto* spStatik  = new QDoubleSpinBox(); spStatik->setRange(0, 50); spStatik->setValue(5.0); spStatik->setSuffix(" m (statik yukseklik)");
    auto* spBoru_m  = new QDoubleSpinBox(); spBoru_m->setRange(1, 500); spBoru_m->setValue(30); spBoru_m->setSuffix(" m (boru boyu)");
    auto* spDN      = new QComboBox();
    spDN->addItems({"DN40", "DN50", "DN65", "DN80", "DN100"});
    spDN->setCurrentIndex(1); // DN50
    auto* spEta     = new QDoubleSpinBox(); spEta->setRange(0.3, 0.95); spEta->setValue(0.70); spEta->setDecimals(2); spEta->setSuffix(" (pompa verimi)");

    form->addRow("Kisi sayisi:", spKisi);
    form->addRow("Statik yukseklik:", spStatik);
    form->addRow("Boru boyu:", spBoru_m);
    form->addRow("Boru capi:", spDN);
    form->addRow("Pompa verimi η:", spEta);
    lay->addLayout(form);

    auto* lblResult = new QLabel();
    lblResult->setWordWrap(true);
    lblResult->setStyleSheet("QLabel { background:#f0f0ff; border:1px solid #99b; padding:8px; font-family:monospace; }");
    lay->addWidget(lblResult);

    auto calcResult = [&]() {
        int    kisi   = spKisi->value();
        double H_stat = spStatik->value();
        double L_boru = spBoru_m->value();
        double eta    = spEta->value();

        // Debi: EN 12050-1 — 0.15 L/s per kişi pikde
        double Q_Ls  = kisi * 0.15; // L/s
        double Q_m3h = Q_Ls * 3.6;  // m³/h

        // Boru çapı (mm)
        static const double dns[] = {40, 50, 65, 80, 100};
        double D_mm = dns[spDN->currentIndex()];
        double D_m  = D_mm / 1000.0;
        double A    = 3.14159 * D_m * D_m / 4.0;
        double v    = (Q_Ls / 1000.0) / A; // m/s

        // Darcy-Weisbach tahmini kayıp: f=0.025, pis su λ≈0.03
        double f    = 0.03;
        double hf   = f * (L_boru / D_m) * (v * v) / (2 * 9.81);

        // Manometrik yükseklik
        double H_man = H_stat + hf * 1.15; // %15 lokal kayıp payı

        // Güç: P = ρ × g × Q × H / η
        double P_kW = (1000.0 * 9.81 * (Q_Ls / 1000.0) * H_man) / (eta * 1000.0);

        // Standart motor seçimi (küçük üstü)
        double P_motor = 0.37;
        for (double s : {0.37, 0.55, 0.75, 1.1, 1.5, 2.2, 3.0, 4.0, 5.5, 7.5}) {
            if (s >= P_kW) { P_motor = s; break; }
        }

        QString html;
        html += QString("<b>Pompa Hesap Sonuclari</b><br>");
        html += QString("Debi Q      : %1 L/s = %2 m³/h<br>").arg(Q_Ls, 0, 'f', 2).arg(Q_m3h, 0, 'f', 2);
        html += QString("Hiz v       : %1 m/s<br>").arg(v, 0, 'f', 2);
        html += QString("Boru kaybi  : %1 m<br>").arg(hf, 0, 'f', 2);
        html += QString("<b>Manometrik H: %1 m</b><br>").arg(H_man, 0, 'f', 2);
        html += QString("Hesap gucu  : %1 kW<br>").arg(P_kW, 0, 'f', 3);
        html += QString("<b style='color:blue'>Secilen motor: %1 kW</b>").arg(P_motor, 0, 'f', 2);
        lblResult->setText(html);
    };

    connect(spKisi,   QOverload<int>::of(&QSpinBox::valueChanged),         [&](int){ calcResult(); });
    connect(spStatik, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spBoru_m, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    connect(spDN,     QOverload<int>::of(&QComboBox::currentIndexChanged),  [&](int){ calcResult(); });
    connect(spEta,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcResult(); });
    calcResult();

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btns);
    dlg.exec();
}

// ============================================================
//  MEMBRANLΙ GENLEŞİM TANKI BOYUTLANDIRMA (EN 12828)
// ============================================================
void MainWindow::OnGenlesimTanki() {
    QDialog dlg(this);
    dlg.setWindowTitle("Membranlı Genleşme Tankı — EN 12828");
    dlg.resize(480, 440);
    auto* lay  = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();

    // Sistem hacmini boru ağından otomatik hesapla
    double autoVol = 0.0;
    if (m_document) {
        for (const auto& [id, edge] : m_document->GetNetwork().GetEdgeMap()) {
            if (edge.type == mep::EdgeType::HotWater || edge.type == mep::EdgeType::Supply) {
                double D_m = edge.diameter_mm / 1000.0;
                double A   = 3.14159265 * (D_m / 2.0) * (D_m / 2.0);
                autoVol   += A * edge.length_m * 1000.0; // m³ → L
            }
        }
    }

    auto* spVsys   = new QDoubleSpinBox(); spVsys->setRange(1, 50000); spVsys->setDecimals(1); spVsys->setSuffix(" L"); spVsys->setValue(std::max(autoVol, 10.0));
    auto* spTcold  = new QDoubleSpinBox(); spTcold->setRange(0, 30);   spTcold->setDecimals(0); spTcold->setSuffix(" °C"); spTcold->setValue(10.0);
    auto* spThot   = new QDoubleSpinBox(); spThot->setRange(40, 100);  spThot->setDecimals(0); spThot->setSuffix(" °C"); spThot->setValue(60.0);
    auto* spHstat  = new QDoubleSpinBox(); spHstat->setRange(0, 100);  spHstat->setDecimals(1); spHstat->setSuffix(" m (statik yük)"); spHstat->setValue(10.0);
    auto* spPsafety= new QDoubleSpinBox(); spPsafety->setRange(2, 10); spPsafety->setDecimals(1); spPsafety->setSuffix(" bar (emniyet ventili)"); spPsafety->setValue(3.0);

    if (autoVol > 0.1)
        spVsys->setToolTip(QString("Ağdan otomatik hesap: %1 L").arg(autoVol, 0, 'f', 2));

    form->addRow("Sistem hacmi V_sys:", spVsys);
    form->addRow("Dolum sıcaklığı T_cold:", spTcold);
    form->addRow("Maks. çalışma sıcaklığı T_hot:", spThot);
    form->addRow("Statik yük H:", spHstat);
    form->addRow("Emniyet ventili basıncı P_S:", spPsafety);
    lay->addLayout(form);

    auto* lblRes = new QLabel();
    lblRes->setWordWrap(true);
    lblRes->setStyleSheet("QLabel{background:#f0fff4;border:1px solid #8c8;padding:8px;font-family:monospace;}");
    lay->addWidget(lblRes);

    // EN 12828 su genleşme katsayısı tablosu (10°C referans)
    auto expansionCoeff = [](double Tcold, double Thot) -> double {
        // Doğrusal interpolasyon: ρ_water(T) ≈ 1/(1 + e_v(T)) baz alındı
        // e_v: genleşme oranı (10°C → T_hot)
        static const double T[] = {10, 20, 40, 60, 70, 80, 90};
        static const double e[] = {0.0, 0.0002, 0.0078, 0.0170, 0.0225, 0.0286, 0.0356};
        constexpr int N = 7;
        // ΔT etkin: dolum sıcaklığını referans al
        double eHot = 0.0, eCold = 0.0;
        for (int i = 1; i < N; ++i) {
            if (Thot <= T[i]) {
                double f = (Thot - T[i-1]) / (T[i] - T[i-1]);
                eHot = e[i-1] + f * (e[i] - e[i-1]);
                break;
            } else if (i == N-1) eHot = e[N-1];
        }
        for (int i = 1; i < N; ++i) {
            if (Tcold <= T[i]) {
                double f = (Tcold - T[i-1]) / (T[i] - T[i-1]);
                eCold = e[i-1] + f * (e[i] - e[i-1]);
                break;
            }
        }
        return std::max(eHot - eCold, 0.001);
    };

    auto calcTank = [&]() {
        double V_sys    = spVsys->value();
        double T_cold   = spTcold->value();
        double T_hot    = spThot->value();
        double H_stat   = spHstat->value();
        double P_safety = spPsafety->value();

        double e_v  = expansionCoeff(T_cold, T_hot);
        // Basınçlar (bar mano.)
        double P_min = H_stat / 10.0 + 0.3; // dolum basıncı = statik + 0.3 bar ön şarj toleransı
        double P_max = P_safety - 0.5;        // güvenlik marjı
        if (P_max <= P_min) P_max = P_min + 0.5;
        // EN 12828: V_tank = V_sys × e_v × (P_max + 1) / (P_max - P_min)   [bar mutlak]
        double V_tank = V_sys * e_v * (P_max + 1.0) / (P_max - P_min);
        double V_nominal = V_tank * 1.25; // %25 emniyet

        // Standart tank boyutları (L)
        static const double std_sizes[] = {2,4,8,12,18,25,35,50,80,100,150,200,300,500};
        double sel = 500;
        for (double s : std_sizes) { if (s >= V_nominal) { sel = s; break; } }

        QString html;
        html += QString("<b>Genleşme Katsayısı e_v:</b> %1 (%2 → %3°C)<br>").arg(e_v, 0, 'f', 4).arg(T_cold, 0, 'f', 0).arg(T_hot, 0, 'f', 0);
        html += QString("<b>P_min (dolum):</b> %1 bar — <b>P_max (çalışma):</b> %2 bar<br>").arg(P_min, 0, 'f', 2).arg(P_max, 0, 'f', 2);
        html += QString("<b>Hesap hacmi V_n:</b> %1 L<br>").arg(V_tank, 0, 'f', 1);
        html += QString("<b>%25 marjlı nominál:</b> %1 L<br>").arg(V_nominal, 0, 'f', 1);
        html += QString("<b style='color:#006600'>Seçilen standart tank: <big>%1 L</big></b><br>").arg(sel, 0, 'f', 0);
        html += QString("Ön şarj basıncı: %1 bar").arg(P_min, 0, 'f', 2);
        lblRes->setText(html);
    };

    connect(spVsys,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spTcold,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spThot,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spHstat,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    connect(spPsafety, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [&](double){ calcTank(); });
    calcTank();

    lay->addWidget(new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, &dlg));
    connect(lay->itemAt(lay->count()-1)->widget(), SIGNAL(accepted()), &dlg, SLOT(accept()));
    dlg.exec();

    if (m_logList)
        m_logList->addItem("Genleşme tankı hesabı yapıldı (EN 12828).");
}

// ============================================================
//  YAĞMUR DÜŞME ALANI — çizimden poligon seçimi
// ============================================================
void MainWindow::OnYagmurAlani() {
    m_polyAreaPoints.clear();
    m_currentToolMode = ToolMode::DrawPolyArea;
    m_drawState       = DrawState::WaitingFirstPoint;
    if (m_snapOverlay) m_snapOverlay->show();
    statusBar()->showMessage("Yağmur alanı: Köşe noktalarını tıklayın — Enter ile kapat, ESC iptal");
    if (m_commandBar) m_commandBar->SetPrompt("Köşe noktası");
}

void MainWindow::FinishPolyArea() {
    if (m_polyAreaPoints.size() < 3) {
        statusBar()->showMessage("En az 3 nokta gerekli.");
        return;
    }
    // Shoelace alanı (mm² → m²)
    double area_mm2 = 0.0;
    int n = static_cast<int>(m_polyAreaPoints.size());
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area_mm2 += m_polyAreaPoints[i].x * m_polyAreaPoints[j].y;
        area_mm2 -= m_polyAreaPoints[j].x * m_polyAreaPoints[i].y;
    }
    double area_m2 = std::abs(area_mm2) / 2.0 / 1e6; // mm² → m²

    m_polyAreaPoints.clear();
    m_currentToolMode = ToolMode::Select;
    m_drawState = DrawState::Idle;
    if (m_snapOverlay) { m_snapOverlay->ClearRubberBand(); m_snapOverlay->Hide(); }

    // Yağmur hesabı — yüzey tipi seçimi
    QStringList surfaceTypes;
    surfaceTypes << "Çatı / Çatı kaplaması (C=1.0)"
                 << "Beton / Asfalt zemin (C=0.9)"
                 << "Yeşil çatı (C=0.5)"
                 << "Çakıllı geçirimli zemin (C=0.6)";
    bool ok = false;
    QString surface = QInputDialog::getItem(this,
        "Yüzey Tipi", QString("Ölçülen alan: <b>%1 m²</b><br>Yüzey tipi:").arg(area_m2, 0, 'f', 2),
        surfaceTypes, 0, false, &ok);
    if (!ok) return;

    double C = 1.0;
    if (surface.contains("Beton")) C = 0.9;
    else if (surface.contains("Yeşil")) C = 0.5;
    else if (surface.contains("akıllı") || surface.contains("Çakıl")) C = 0.6;

    double rD = 0.03;
    double Q_ls = C * area_m2 * rD;

    static const struct { double dn; double cap_ls; } kTable[] = {
        {50,1.2},{75,3.5},{100,8.0},{125,15.0},{150,25.0},{200,50.0},{0,0}
    };
    double dnPerPipe = 200.0;
    double numPipes = 1;
    if (Q_ls > 50.0) numPipes = std::ceil(Q_ls / 50.0);
    double qPer = Q_ls / numPipes;
    for (int i = 0; kTable[i].dn > 0; ++i) {
        if (kTable[i].cap_ls >= qPer) { dnPerPipe = kTable[i].dn; break; }
    }

    QString msg;
    msg += QString("<h3>Yağmur Suyu Tahliye — Çizimden Ölçüm (EN 12056-3)</h3>");
    msg += QString("<table border='0' cellspacing='4'>");
    msg += QString("<tr><td>Ölçülen alan:</td><td><b>%1 m²</b></td></tr>").arg(area_m2, 0, 'f', 2);
    msg += QString("<tr><td>Yüzey katsayısı (C):</td><td>%1</td></tr>").arg(C, 0, 'f', 2);
    msg += QString("<tr><td>r_D:</td><td>0.030 l/(s·m²)</td></tr>");
    msg += QString("<tr><td><b>Tasarım debisi Q:</b></td><td><font color='red'><b>%1 l/s</b></font></td></tr>").arg(Q_ls, 0, 'f', 2);
    msg += QString("<tr><td>Önerilen boru:</td><td><b>%1× DN%2</b></td></tr>").arg((int)numPipes).arg((int)dnPerPipe);
    msg += QString("</table>");

    QMessageBox dlg2(this);
    dlg2.setWindowTitle("Yağmur Suyu — Poligon Alanı");
    dlg2.setTextFormat(Qt::RichText);
    dlg2.setText(msg);
    dlg2.setIcon(QMessageBox::Information);
    dlg2.exec();

    if (m_logList)
        m_logList->addItem(QString("Yağmur (poligon): A=%1m², Q=%2l/s → %3×DN%4")
            .arg(area_m2,0,'f',1).arg(Q_ls,0,'f',2).arg((int)numPipes).arg((int)dnPerPipe));
    statusBar()->showMessage(QString("Yağmur alanı: %1 m² → Q=%2 l/s")
        .arg(area_m2,0,'f',2).arg(Q_ls,0,'f',2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Norm Karşılaştırma — EN 806-3 ile DIN 1988-300 yan yana
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnNormKarsilastirma() {
    auto* doc = m_document;
    if (!doc) { QMessageBox::information(this, "Norm Karşılaştırma", "Açık proje yok."); return; }
    auto& network = doc->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Norm Karşılaştırma", "Ağda boru bulunamadı.");
        return;
    }

    // EN 806-3 hesabı
    mep::HydroNorm savedNorm = mep::HydraulicSolver::GlobalNorm();
    mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::EN806_3;
    mep::HydraulicSolver solverEN(network);
    solverEN.SetNorm(mep::HydroNorm::EN806_3);
    solverEN.Solve();

    struct EdgeResult {
        uint32_t id;
        double dn_en, q_nom_en, q_hes_en, v_en, dh_en;
        double dn_din, q_nom_din, q_hes_din, v_din, dh_din;
    };
    std::vector<EdgeResult> rows;
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
        EdgeResult r;
        r.id      = eid;
        r.dn_en   = edge.diameter_mm;
        r.q_nom_en = edge.nominalFlow_Ls;
        r.q_hes_en = edge.flowRate_m3s * 1000.0;
        r.v_en    = edge.velocity_ms;
        r.dh_en   = edge.headLoss_m;
        rows.push_back(r);
    }

    // DIN 1988-300 hesabı — aynı ağ üzerinde (field'ları geçici yazar)
    mep::HydraulicSolver::GlobalNorm() = mep::HydroNorm::DIN1988;
    mep::HydraulicSolver solverDIN(network);
    solverDIN.SetNorm(mep::HydroNorm::DIN1988);
    solverDIN.Solve();

    for (auto& r : rows) {
        auto it = network.GetEdgeMap().find(r.id);
        if (it == network.GetEdgeMap().end()) continue;
        const auto& e = it->second;
        r.dn_din   = e.diameter_mm;
        r.q_nom_din = e.nominalFlow_Ls;
        r.q_hes_din = e.flowRate_m3s * 1000.0;
        r.v_din    = e.velocity_ms;
        r.dh_din   = e.headLoss_m;
    }

    // Aktif normu geri yükle
    mep::HydraulicSolver::GlobalNorm() = savedNorm;

    // HTML tablo oluştur
    QString html;
    html += "<h3>Norm Karşılaştırma — EN 806-3 vs DIN 1988-300</h3>";
    html += "<table border='1' cellspacing='0' cellpadding='4'>";
    html += "<tr style='background:#ddeeff'>"
            "<th>Boru ID</th>"
            "<th colspan='4'>EN 806-3</th>"
            "<th colspan='4'>DIN 1988-300</th>"
            "<th>DN Fark</th></tr>";
    html += "<tr style='background:#eef4ff'>"
            "<th></th>"
            "<th>DN</th><th>Q_nom (l/s)</th><th>Q_hes (l/s)</th><th>v (m/s)</th><th>ΔH (m)</th>"
            "<th>DN</th><th>Q_nom (l/s)</th><th>Q_hes (l/s)</th><th>v (m/s)</th><th>ΔH (m)</th>"
            "<th></th></tr>";

    int upCount = 0, downCount = 0, sameCount = 0;
    for (const auto& r : rows) {
        bool bigger  = r.dn_din > r.dn_en;
        bool smaller = r.dn_din < r.dn_en;
        if (bigger) ++upCount; else if (smaller) ++downCount; else ++sameCount;
        QString rowColor = bigger ? "#fff8e1" : (smaller ? "#e8f5e9" : "white");
        QString diffStr  = bigger ? QString("+%1mm").arg(r.dn_din - r.dn_en, 0, 'f', 0)
                         : smaller ? QString("-%1mm").arg(r.dn_en - r.dn_din, 0, 'f', 0)
                         : "=";
        html += QString("<tr style='background:%1'>"
                        "<td>%2</td>"
                        "<td>DN%3</td><td>%4</td><td>%5</td><td>%6</td><td>%7</td>"
                        "<td>DN%8</td><td>%9</td><td>%10</td><td>%11</td><td>%12</td>"
                        "<td><b>%13</b></td></tr>")
            .arg(rowColor)
            .arg(r.id)
            .arg(r.dn_en, 0, 'f', 0)
            .arg(r.q_nom_en, 0, 'f', 3)
            .arg(r.q_hes_en, 0, 'f', 3)
            .arg(r.v_en, 0, 'f', 2)
            .arg(r.dh_en, 0, 'f', 3)
            .arg(r.dn_din, 0, 'f', 0)
            .arg(r.q_nom_din, 0, 'f', 3)
            .arg(r.q_hes_din, 0, 'f', 3)
            .arg(r.v_din, 0, 'f', 2)
            .arg(r.dh_din, 0, 'f', 3)
            .arg(diffStr);
    }
    html += "</table>";
    html += QString("<br><b>Özet:</b> DIN vs EN — Büyük çap: <font color='orange'>%1</font> boru | "
                    "Küçük çap: <font color='green'>%2</font> boru | "
                    "Aynı: %3 boru").arg(upCount).arg(downCount).arg(sameCount);
    html += "<br><small>Sarı satır: DIN daha büyük DN seçti | Yeşil satır: DIN daha küçük DN seçti</small>";

    QDialog dlg(this);
    dlg.setWindowTitle("Norm Karşılaştırma — EN 806-3 vs DIN 1988-300");
    dlg.resize(960, 560);
    auto* lay = new QVBoxLayout(&dlg);
    auto* tb  = new QTextBrowser(&dlg);
    tb->setHtml(html);
    lay->addWidget(tb);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(btn);
    dlg.exec();

    if (m_logList)
        m_logList->addItem(QString("Norm karş.: %1 boru — DIN büyük:%2 küçük:%3 aynı:%4")
            .arg(rows.size()).arg(upCount).arg(downCount).arg(sameCount));
}

// ─────────────────────────────────────────────────────────────────────────────
// Hesap Kararı — "Neden bu çap?" per-pipe gerekçe
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnHesapKarari() {
    auto* doc = m_document;
    if (!doc) { QMessageBox::information(this, "Hesap Kararı", "Açık proje yok."); return; }
    auto& network = doc->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        QMessageBox::information(this, "Hesap Kararı", "Ağda boru bulunamadı.");
        return;
    }

    // Güncel hesabı çalıştır
    mep::HydraulicSolver solver(network);
    solver.Solve();

    // DN serisi (mm)
    static const double kDNSeries[] = {10,15,20,25,32,40,50,65,80,100,125,150,200,0};
    auto nextDN = [](double dn) -> double {
        for (int i = 0; kDNSeries[i] > 0; ++i)
            if (kDNSeries[i] > dn) return kDNSeries[i];
        return dn;
    };
    auto prevDN = [](double dn) -> double {
        double prev = 0;
        for (int i = 0; kDNSeries[i] > 0; ++i) {
            if (kDNSeries[i] >= dn) return (prev > 0 ? prev : dn);
            prev = kDNSeries[i];
        }
        return dn;
    };

    QDialog dlg(this);
    dlg.setWindowTitle("Hesap Kararı — Neden Bu Çap?");
    dlg.resize(820, 480);
    auto* lay = new QVBoxLayout(&dlg);
    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({
        "Boru ID", "DN (mm)", "Q (l/s)", "v (m/s)",
        "DN-1 → v", "DN+1 → v", "Gerekçe"
    });
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    const double V_MAX = 3.0;  // EN 806-3 max hız m/s
    const double V_MIN = 0.5;  // minimum akış hızı m/s

    int row = 0;
    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
        table->insertRow(row);

        double q_m3s = edge.flowRate_m3s;
        double dn    = edge.diameter_mm;
        double v_cur = edge.velocity_ms;

        // Hız hesabı için yardımcı lambda
        auto velForDN = [&](double d_mm) -> double {
            if (d_mm <= 0) return 0;
            double r = d_mm / 2000.0;
            double area = 3.14159265 * r * r;
            return area > 0 ? q_m3s / area : 0;
        };

        double dn_prev = prevDN(dn);
        double dn_next = nextDN(dn);
        double v_prev  = (dn_prev != dn) ? velForDN(dn_prev) : -1.0;
        double v_next  = (dn_next != dn) ? velForDN(dn_next) : -1.0;

        // Gerekçe belirle
        QString reason;
        if (v_prev > 0 && v_prev <= V_MAX && v_prev >= V_MIN)
            reason = "Bir küçük DN hız limiti içinde → Ekonomi";
        else if (v_cur > V_MAX)
            reason = "Mevcut hız max sınırı aşıyor!";
        else if (v_next > 0 && v_next < V_MIN)
            reason = "Bir büyük DN min hızı sağlamıyor";
        else if (dn_prev == dn)
            reason = "Serinin en küçük boyutu";
        else
            reason = "EN 806-3: hız + basınç dengesi";

        auto makeItem = [](const QString& txt, Qt::AlignmentFlag align = Qt::AlignHCenter) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(align | Qt::AlignVCenter);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };

        table->setItem(row, 0, makeItem(QString::number(eid)));
        table->setItem(row, 1, makeItem(QString("DN%1").arg(dn, 0, 'f', 0)));
        table->setItem(row, 2, makeItem(QString("%1").arg(q_m3s * 1000.0, 0, 'f', 3)));
        table->setItem(row, 3, makeItem(QString("%1").arg(v_cur, 0, 'f', 2)));

        QString prevStr = (dn_prev != dn && v_prev >= 0)
            ? QString("DN%1→%2 m/s").arg(dn_prev, 0, 'f', 0).arg(v_prev, 0, 'f', 2)
            : "—";
        QString nextStr = (dn_next != dn && v_next >= 0)
            ? QString("DN%1→%2 m/s").arg(dn_next, 0, 'f', 0).arg(v_next, 0, 'f', 2)
            : "—";

        table->setItem(row, 4, makeItem(prevStr));
        table->setItem(row, 5, makeItem(nextStr));
        auto* reasonItem = makeItem(reason, Qt::AlignLeft);
        if (v_cur > V_MAX)
            reasonItem->setBackground(QColor(255, 200, 200));
        else if (reason.contains("Ekonomi"))
            reasonItem->setBackground(QColor(255, 255, 180));
        table->setItem(row, 6, reasonItem);

        ++row;
    }

    lay->addWidget(table);
    auto* btn = new QPushButton("Kapat", &dlg);
    connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(btn);
    dlg.exec();

    if (m_logList)
        m_logList->addItem(QString("Hesap kararı: %1 boru için gerekçe gösterildi").arg(row));
}

// ═══════════════════════════════════════════════════════════
//  OTOMATİK ÖLÇÜLENDİRME — paralel boru stacking + anti-overlap
// ═══════════════════════════════════════════════════════════
void MainWindow::OnAutoOlculendir() {
    if (!m_document) return;

    const auto& network = m_document->GetNetwork();
    if (network.GetEdgeMap().empty()) {
        statusBar()->showMessage("Ölçülendirilecek MEP borusu yok", 2000);
        return;
    }

    // Mevcut VKT-DIM ölçülerini temizle mi?
    bool hasDims = false;
    for (const auto& ent : m_document->GetCADEntities()) {
        if (ent && ent->GetLayerName() == "VKT-DIM") { hasDims = true; break; }
    }
    if (hasDims) {
        auto btn = QMessageBox::question(this, "Otomatik Ölçülendirme",
            "Mevcut VKT-DIM ölçüleri bulundu.\n"
            "Sil ve yeniden oluştur?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) return;
        if (btn == QMessageBox::Yes) {
            m_document->RemoveCADEntitiesByLayer("VKT-DIM");
        }
    }

    // Ölçülendirilecek her kenar için hesap bilgisi
    struct DimInfo {
        uint32_t  edgeId;
        double    midX, midY;
        double    nx, ny;      // birim normal (boruya dik)
        int       angleSlot;   // 15° adımlı quantize
        int       offsetLevel; // kaçıncı offset sırası (0=1500, 1=3000, ...)
    };

    static constexpr double BASE_OFFSET  = 1500.0;  // ilk ölçü ofseti (mm)
    static constexpr double NEARBY_PIPE  = 3000.0;  // bu mesafedeki paralel borular stack
    static constexpr double PI_L = 3.14159265358979323846;

    std::vector<DimInfo> infos;
    infos.reserve(network.GetEdgeMap().size());

    for (const auto& [eid, edge] : network.GetEdgeMap()) {
        if (edge.type != mep::EdgeType::Supply   &&
            edge.type != mep::EdgeType::HotWater &&
            edge.type != mep::EdgeType::Gas       &&
            edge.type != mep::EdgeType::Heating   &&
            edge.type != mep::EdgeType::FireLine) continue;

        const mep::Node* nA = network.GetNode(edge.nodeA);
        const mep::Node* nB = network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        double dx = nB->position.x - nA->position.x;
        double dy = nB->position.y - nA->position.y;
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < 1.0) continue;

        // Boru açısını [0,180) aralığına normalize et (karşılıklı borular aynı slot)
        double ang = std::atan2(dy, dx) * 180.0 / PI_L;
        if (ang < 0) ang += 180.0;

        DimInfo di;
        di.edgeId = eid;
        di.midX = (nA->position.x + nB->position.x) / 2.0;
        di.midY = (nA->position.y + nB->position.y) / 2.0;
        di.nx   = -dy / len;   // normal (sola dik)
        di.ny   =  dx / len;
        di.angleSlot  = static_cast<int>(std::round(ang / 15.0)) % 12;
        di.offsetLevel = 0;
        infos.push_back(di);
    }

    // Anti-overlap: paralel ve yakın borulara artan offset seviyesi ata
    for (size_t i = 0; i < infos.size(); ++i) {
        int level = 0;
        bool conflict = true;
        while (conflict) {
            conflict = false;
            for (size_t j = 0; j < i; ++j) {
                if (infos[j].angleSlot   != infos[i].angleSlot)   continue;
                if (infos[j].offsetLevel != level)                 continue;
                double ddx = infos[j].midX - infos[i].midX;
                double ddy = infos[j].midY - infos[i].midY;
                // Normal eksen boyunca mesafe (perpendikler ayrım)
                double projN = std::abs(ddx * infos[i].nx + ddy * infos[i].ny);
                // Boru ekseni boyunca mesafe
                double projP = std::abs(ddx * infos[i].ny + ddy * (-infos[i].nx));
                if (projN < 2 * BASE_OFFSET && projP < NEARBY_PIPE) {
                    conflict = true;
                    ++level;
                    break;
                }
            }
        }
        infos[i].offsetLevel = level;
    }

    // Dimension entity'leri oluştur
    int added = 0;
    for (const auto& di : infos) {
        const auto& edge = network.GetEdgeMap().at(di.edgeId);
        const mep::Node* nA = network.GetNode(edge.nodeA);
        const mep::Node* nB = network.GetNode(edge.nodeB);
        if (!nA || !nB) continue;

        double offset = (di.offsetLevel + 1) * BASE_OFFSET;
        geom::Vec3 dimLinePos{ di.midX + di.nx * offset,
                               di.midY + di.ny * offset, 0.0 };

        std::string txt = "DN" + std::to_string(static_cast<int>(edge.diameter_mm));
        if (edge.length_m > 0.0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " / %.2fm", edge.length_m);
            txt += buf;
        }
        // Debi varsa ekle (m³/s → l/s dönüşümü)
        if (edge.flowRate_m3s > 0.0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " | %.2fL/s", edge.flowRate_m3s * 1000.0);
            txt += buf;
        }

        auto dim = std::make_unique<cad::Dimension>(
            nA->position, nB->position, dimLinePos, cad::DimensionType::Aligned);
        dim->SetOverrideText(txt);
        dim->SetTextHeight(300.0);
        dim->SetLayerName("VKT-DIM");
        m_document->AddCADEntity(std::move(dim));
        ++added;
    }

    if (added > 0) {
        auto& layers = m_document->GetLayersMutable();
        if (!layers.count("VKT-DIM")) {
            cad::Layer dimLayer("VKT-DIM");
            dimLayer.SetColor({0, 200, 200, 255}); // cyan
            layers["VKT-DIM"] = dimLayer;
        }
        RebuildCADEntityCache();
        InvalidateRenderer();
        m_document->SetModified(true);
        int maxLevel = 0;
        for (const auto& di : infos) maxLevel = std::max(maxLevel, di.offsetLevel);
        statusBar()->showMessage(
            QString("%1 boru ölçüsü eklendi (maks. %2 offset seviyesi — VKT-DIM)")
                .arg(added).arg(maxLevel + 1));
        if (m_logList) m_logList->addItem(
            QString("Otomatik ölçülendirme: %1 Dimension, %2 stacking seviyesi")
                .arg(added).arg(maxLevel + 1));
    } else {
        statusBar()->showMessage("Ölçülendirilecek boru bulunamadı", 2000);
    }
}

// ═══════════════════════════════════════════════════════════
//  BASINÇ BÖLGESİ YÖNETİMİ
// ═══════════════════════════════════════════════════════════
void MainWindow::OnBaskiBolgesi() {
    if (!m_document) return;
    auto& network = m_document->GetNetwork();

    // Kaç kaynak (Source) var?
    std::vector<uint32_t> sources;
    for (const auto& [nid, node] : network.GetNodeMap())
        if (node.type == mep::NodeType::Source)
            sources.push_back(nid);

    if (sources.empty()) {
        QMessageBox::warning(this, "Basınç Bölgesi",
            "Şebekede kaynak (Source) bulunamadı.\n"
            "Önce en az bir su kaynağı yerleştirin.");
        return;
    }

    // Bilgi diyaloğu: kaynak sayısı, bölge bilgisi
    QString info;
    info += QString("<b>%1 kaynak (basınç bölgesi) tespit edildi:</b><br><br>").arg(sources.size());

    mep::HydraulicSolver solver(network);
    solver.SetNorm(mep::HydraulicSolver::GlobalNorm());
    solver.Solve();

    for (uint32_t sid : sources) {
        const mep::Node* src = network.GetNode(sid);
        if (!src) continue;

        // Bu kaynağa bağlı edge'leri bul ve toplam LU hesapla
        double totalLU = 0.0;
        int pipeCount  = 0;
        for (const auto& [eid, edge] : network.GetEdgeMap()) {
            if (edge.type != mep::EdgeType::Supply && edge.type != mep::EdgeType::HotWater) continue;
            totalLU += edge.flowRate_m3s * 1000.0; // l/s gösterg
            ++pipeCount;
        }

        // Kritik yol başlangıç noktasından kritik kayıp
        auto critRes = solver.CalculateCriticalPath();
        double critLoss = critRes.totalHeadLoss_m;

        info += QString("• Kaynak #%1 (<i>%2</i>)<br>")
                    .arg(sid).arg(QString::fromStdString(src->label));
        info += QString("&nbsp;&nbsp;Toplam boru: %1 | Kritik kayıp: %2 m su sütunu<br>")
                    .arg(pipeCount).arg(critLoss, 0, 'f', 2);
        if (critRes.requiredPumpHead_m > 0) {
            info += QString("&nbsp;&nbsp;⚠ Pompa gerekli: %1 m / %2 m³/h<br>")
                        .arg(critRes.requiredPumpHead_m, 0, 'f', 1)
                        .arg(critRes.requiredFlow_m3h, 0, 'f', 2);
            info += QString("&nbsp;&nbsp;Öneri: <b>%1</b><br>")
                        .arg(QString::fromStdString(critRes.suggestedPumpModel));
        } else {
            info += QString("&nbsp;&nbsp;✅ Şebeke basıncı yeterli<br>");
        }
        info += "<br>";
        break; // Şimdilik tek kaynak modeli
    }

    if (sources.size() > 1) {
        info += "<i>Not: Çoklu kaynaklı sistemde her bölge ayrı hidrolik çevre oluşturur.<br>"
                "Bölge sınırını belirlemek için her kaynağa ait NODE'ları farklı katmanlara taşıyın.</i>";
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Basınç Bölgesi Analizi");
    dlg.setMinimumWidth(480);
    auto* vl  = new QVBoxLayout(&dlg);
    auto* lbl = new QLabel(info);
    lbl->setWordWrap(true);
    vl->addWidget(lbl);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vl->addWidget(bb);
    dlg.exec();
}

// ═══════════════════════════════════════════════════════════
//  ÜRETİCİ KATALOG — Wavin / Valsir / Henco boru veritabanı
// ═══════════════════════════════════════════════════════════
void MainWindow::OnUreticiKatalog() {
    // Katalog diyaloğu — boru malzeme ve fiyat bilgisi
    QDialog dlg(this);
    dlg.setWindowTitle("Üretici Katalog — Boru Seçimi");
    dlg.setMinimumWidth(620);
    dlg.setMinimumHeight(420);
    auto* vl = new QVBoxLayout(&dlg);
    vl->addWidget(new QLabel("<b>Boru Üreticisi Seçimi (Wavin / Valsir / Henco)</b>"));

    struct CatalogEntry {
        QString uretici;
        QString malzeme;
        QString seri;
        QVector<int> dnliste;
        double fiyat_m;   // TL/m DN20 referans
        double roughness; // mm
    };

    QVector<CatalogEntry> catalog = {
        {"Wavin",  "PVC-U",  "Ekoplastik",     {16,20,25,32,40,50,63,75,90,110},  42.0, 0.0015},
        {"Wavin",  "PP-R",   "Tigris M",        {16,20,25,32,40,50,63},            58.0, 0.0015},
        {"Wavin",  "PE-Xc",  "Tigris Blue",     {16,20,25,32},                     75.0, 0.0015},
        {"Valsir", "PP",     "Triplus 3",       {32,40,50,75,90,110,125,160},      38.0, 0.0015},
        {"Valsir", "PVC-U",  "Pushing PVC",     {32,40,50,75,90,110,125,160,200},  35.0, 0.0015},
        {"Valsir", "HDPE",   "Silere",          {75,90,110,125,160,200},           65.0, 0.0010},
        {"Henco",  "PE-RT",  "Henco Pert II",   {16,20,25,32},                     82.0, 0.0015},
        {"Henco",  "Bakır",  "Henco Copper",    {15,18,22,28,35,42,54},           235.0, 0.0010},
        {"Henco",  "Çelik",  "Isıtma Çeliği",  {15,20,25,32,40,50,65,80,100},     98.0, 0.0460},
    };

    auto* tbl = new QTableWidget(catalog.size(), 6, &dlg);
    tbl->setHorizontalHeaderLabels({"Üretici","Malzeme","Seri","DN Aralığı","Fiyat TL/m","ε (mm)"});
    tbl->horizontalHeader()->setStretchLastSection(true);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int r = 0; r < catalog.size(); ++r) {
        const auto& e = catalog[r];
        QStringList dns;
        for (int d : e.dnliste) dns << QString("DN%1").arg(d);
        tbl->setItem(r, 0, new QTableWidgetItem(e.uretici));
        tbl->setItem(r, 1, new QTableWidgetItem(e.malzeme));
        tbl->setItem(r, 2, new QTableWidgetItem(e.seri));
        tbl->setItem(r, 3, new QTableWidgetItem(dns.join(", ")));
        tbl->setItem(r, 4, new QTableWidgetItem(QString::number(e.fiyat_m, 'f', 0)));
        tbl->setItem(r, 5, new QTableWidgetItem(QString::number(e.roughness, 'g', 4)));
    }
    tbl->resizeColumnsToContents();
    vl->addWidget(tbl);

    vl->addWidget(new QLabel("<i>Seçili satırı projeye ekle → malzeme panelinde kullanılabilir</i>"));

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, [&]() {
        // Seçili satırı Database'e ekle (mevcut malzeme listesine)
        auto rows = tbl->selectionModel()->selectedRows();
        if (rows.isEmpty()) { dlg.reject(); return; }
        int r = rows.first().row();
        const auto& e = catalog[r];
        mep::PipeData pd;
        pd.material    = e.malzeme.toStdString();
        pd.roughness_mm= e.roughness;
        pd.unitPrice_TL= e.fiyat_m;
        for (int d : e.dnliste) pd.availableDiameters_mm.push_back(d);
        // Database read-only singleton — yeni malzeme bilgisini log'a yaz
        if (m_logList) m_logList->addItem(
            QString("Katalog: %1 %2 (%3) eklendi — %.4f mm pürüzlülük, %4 TL/m")
                .arg(e.uretici).arg(e.malzeme).arg(e.seri)
                .arg(e.roughness).arg(e.fiyat_m, 0, 'f', 0));
        statusBar()->showMessage(
            QString("%1 %2 seçildi — özellik panelinden 'Malzeme' olarak kullanın")
                .arg(e.uretici).arg(e.malzeme), 5000);
        dlg.accept();
    });
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(bb);
    dlg.exec();
}

// ═══════════════════════════════════════════════════════════
//  CLASH DETECTION — 3D çakışma analizi + viewport highlight
// ═══════════════════════════════════════════════════════════
void MainWindow::OnClashDetect() {
    if (!m_document) return;

    mep::ClashEngine engine(
        m_document->GetNetwork(),
        m_document->GetCADEntities()
    );
    m_lastClashResults = engine.RunAnalysis();

    // Extract clashing edge IDs for viewport highlight
    std::vector<uint32_t> clashEdgeIds;
    clashEdgeIds.reserve(m_lastClashResults.size());
    for (const auto& r : m_lastClashResults)
        clashEdgeIds.push_back(r.edgeId);

    if (m_vulkanWindow)
        m_vulkanWindow->SetClashHighlightEdges(clashEdgeIds);

    // Build summary HTML
    int hardCount = 0, softCount = 0;
    for (const auto& r : m_lastClashResults) {
        if (r.severity == mep::ClashSeverity::Hard) ++hardCount;
        else ++softCount;
    }

    QString html = "<html><body>";
    html += QString("<h3>Çakışma Analizi Sonuçları</h3>");
    html += QString("<p><b>Toplam:</b> %1 çakışma — "
                    "<span style='color:red'>%2 Hard</span> | "
                    "<span style='color:orange'>%3 Soft</span></p>")
                .arg(m_lastClashResults.size()).arg(hardCount).arg(softCount);

    if (!m_lastClashResults.empty()) {
        html += "<table border='1' cellpadding='3' style='border-collapse:collapse'>"
                "<tr style='background:#ddd'><th>#</th><th>Tip</th><th>Boru ID</th>"
                "<th>Mimari ID</th><th>Konum (m)</th><th>Açıklama</th></tr>";

        for (const auto& r : m_lastClashResults) {
            QString sev = (r.severity == mep::ClashSeverity::Hard)
                ? "<span style='color:red'>Hard</span>"
                : "<span style='color:orange'>Soft</span>";
            QString pos = QString("(%1, %2, %3)")
                .arg(r.clashPoint.x, 0, 'f', 2)
                .arg(r.clashPoint.y, 0, 'f', 2)
                .arg(r.clashPoint.z, 0, 'f', 2);
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                .arg(r.id).arg(sev)
                .arg(r.edgeId)
                .arg(static_cast<unsigned long long>(r.architecturalId))
                .arg(pos)
                .arg(QString::fromStdString(r.description));
        }
        html += "</table>";
    } else {
        html += "<p style='color:green'><b>Çakışma tespit edilmedi.</b></p>";
    }
    html += "</body></html>";

    QDialog dlg(this);
    dlg.setWindowTitle("Çakışma Analizi");
    dlg.setMinimumWidth(700);
    dlg.setMinimumHeight(420);
    auto* vl = new QVBoxLayout(&dlg);
    auto* browser = new QTextBrowser(&dlg);
    browser->setHtml(html);
    vl->addWidget(browser);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    auto* btnRpt = bb->addButton("Rapor Kaydet...", QDialogButtonBox::ActionRole);
    connect(btnRpt, &QPushButton::clicked, this, &MainWindow::OnClashReport);
    auto* btnClear = bb->addButton("Vurgulamayi Kaldir", QDialogButtonBox::ActionRole);
    connect(btnClear, &QPushButton::clicked, this, [&]() {
        if (m_vulkanWindow) m_vulkanWindow->ClearClashHighlight();
    });
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vl->addWidget(bb);

    statusBar()->showMessage(
        QString("Çakışma: %1 Hard, %2 Soft — çakışan borular kırmızı")
            .arg(hardCount).arg(softCount), 8000);
    if (m_logList)
        m_logList->addItem(QString("[CLASH] %1 hard + %2 soft").arg(hardCount).arg(softCount));

    dlg.exec();
}

void MainWindow::OnClashReport() {
    if (m_lastClashResults.empty()) {
        QMessageBox::information(this, "Çakışma Raporu",
            "Henüz çakışma analizi yapılmadı.\nÖnce 'Çakışma Analizi' çalıştırın.");
        return;
    }

    QString defPath;
    auto& pm = core::ProjectManager::Instance();
    if (pm.HasActiveProject())
        defPath = QString::fromStdString(pm.GetRaporFolder()) + "cakisma_raporu.xls";
    else
        defPath = QDir::homePath() + "/cakisma_raporu.xls";

    QString path = QFileDialog::getSaveFileName(this, "Çakışma Raporunu Kaydet",
                                                defPath, "Excel (*.xls);;PDF (*.pdf)");
    if (path.isEmpty()) return;

    if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
        // PDF rapor — HTML via QPrinter
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        printer.setPageOrientation(QPageLayout::Landscape);
        printer.setPageSize(QPageSize::A4);

        QTextDocument doc;
        QString html = "<html><body><h2>Çakışma Analizi Raporu</h2>"
                       "<table border='1' cellpadding='4' style='border-collapse:collapse;font-size:10pt'>"
                       "<tr style='background:#ccc'><th>#</th><th>Tip</th><th>Boru ID</th>"
                       "<th>Mimari ID</th><th>X(m)</th><th>Y(m)</th><th>Z(m)</th><th>Açıklama</th></tr>";
        for (const auto& r : m_lastClashResults) {
            QString sev = (r.severity == mep::ClashSeverity::Hard) ? "Hard" : "Soft";
            html += QString("<tr><td>%1</td><td><b>%2</b></td><td>%3</td><td>%4</td>"
                            "<td>%5</td><td>%6</td><td>%7</td><td>%8</td></tr>")
                .arg(r.id).arg(sev).arg(r.edgeId)
                .arg(static_cast<unsigned long long>(r.architecturalId))
                .arg(r.clashPoint.x, 0, 'f', 2)
                .arg(r.clashPoint.y, 0, 'f', 2)
                .arg(r.clashPoint.z, 0, 'f', 2)
                .arg(QString::fromStdString(r.description));
        }
        html += "</table></body></html>";
        doc.setHtml(html);
        doc.print(&printer);
        statusBar()->showMessage("Çakışma raporu PDF'e kaydedildi: " + path, 6000);
    } else {
        // XLS rapor
        using Row = std::vector<std::string>;
        std::vector<Row> rows;
        rows.push_back({"#","Tip","Boru ID","Mimari ID","X(m)","Y(m)","Z(m)","Aciklama"});
        for (const auto& r : m_lastClashResults) {
            rows.push_back({
                std::to_string(r.id),
                (r.severity == mep::ClashSeverity::Hard) ? "Hard" : "Soft",
                std::to_string(r.edgeId),
                std::to_string(static_cast<unsigned long long>(r.architecturalId)),
                QString::number(r.clashPoint.x, 'f', 2).toStdString(),
                QString::number(r.clashPoint.y, 'f', 2).toStdString(),
                QString::number(r.clashPoint.z, 'f', 2).toStdString(),
                r.description
            });
        }

        // Write simple XLS (tab-delimited BIFF2-compatible)
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            for (const auto& row : rows) {
                QStringList cols;
                for (const auto& c : row) cols << QString::fromStdString(c);
                ts << cols.join("\t") << "\n";
            }
            f.close();
        }
        statusBar()->showMessage("Çakışma raporu kaydedildi: " + path, 6000);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Birleşik Yerleştirme Modu toggle
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::OnBirleskMod() {
    m_birleskMod = !m_birleskMod;
    if (m_actBirleskMod)
        m_actBirleskMod->setChecked(m_birleskMod);
    QString msg = m_birleskMod
        ? "Birleşik Mod AÇIK — Armatür yerleştirince otomatik boru hattına bağlanır"
        : "Birleşik Mod KAPALI";
    statusBar()->showMessage(msg);
    if (m_logList) m_logList->addItem(msg);
}

}} // namespace vkt::ui
