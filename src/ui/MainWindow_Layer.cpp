/**
 * @file MainWindow_Layer.cpp
 * @brief Layer management methods for MainWindow
 *
 * LAYISO/LAYUNISO, lock/unlock, purge, select-by-layer, move-to-layer,
 * LAYERSTATE save/restore, layer standard presets, RefreshLayerPanel,
 * HighlightLayerInPanel.
 */

#include "ui/MainWindow.hpp"
#include "render/VulkanWindow.hpp"
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QTreeWidget>
#include <QFont>
#include <QColor>
#include <algorithm>
#include <unordered_set>

namespace vkt {
namespace ui {

// ═══════════════════════════════════════════════════════════
//  LAYISO — seçili entity katmanını izole et
// ═══════════════════════════════════════════════════════════
void MainWindow::OnLayIso() {
    if (!m_document) return;

    std::string targetLayer;
    if (m_selectedCADEntityId != 0) {
        auto it = m_cadEntityCache.find(m_selectedCADEntityId);
        if (it != m_cadEntityCache.end() && it->second)
            targetLayer = it->second->GetLayerName();
    }
    if (targetLayer.empty() && !m_selectedCADEntityIds.empty()) {
        auto it = m_cadEntityCache.find(*m_selectedCADEntityIds.begin());
        if (it != m_cadEntityCache.end() && it->second)
            targetLayer = it->second->GetLayerName();
    }
    if (targetLayer.empty()) {
        QStringList layerNames;
        for (const auto& [n, _] : m_document->GetLayers())
            layerNames << QString::fromStdString(n);
        layerNames.sort();
        bool ok = false;
        QString chosen = QInputDialog::getItem(this, "LAYISO", "İzole edilecek katman:", layerNames, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        targetLayer = chosen.toStdString();
    }

    m_preIsoVisibility.clear();
    for (auto& [name, layer] : m_document->GetLayersMutable()) {
        m_preIsoVisibility[name] = layer.IsVisible();
        layer.SetVisible(name == targetLayer);
    }
    m_layisoActive = true;

    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage(QString("LAYISO: '%1' katmanı izole edildi — LAYUNISO ile geri al")
        .arg(QString::fromStdString(targetLayer)), 4000);
}

void MainWindow::OnLayUnIso() {
    if (!m_document) return;
    if (!m_layisoActive) {
        statusBar()->showMessage("LAYUNISO: Aktif izolasyon yok", 2000);
        return;
    }
    for (auto& [name, layer] : m_document->GetLayersMutable()) {
        auto it = m_preIsoVisibility.find(name);
        layer.SetVisible(it != m_preIsoVisibility.end() ? it->second : true);
    }
    m_layisoActive = false;
    m_preIsoVisibility.clear();
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage("LAYUNISO: Tüm katmanlar eski haline döndürüldü", 2000);
}

void MainWindow::OnLayLockAll() {
    if (!m_document) return;
    for (auto& [name, layer] : m_document->GetLayersMutable())
        layer.SetLocked(true);
    RefreshLayerPanel();
    statusBar()->showMessage("Tüm katmanlar kilitlendi", 2000);
}

void MainWindow::OnLayUnlockAll() {
    if (!m_document) return;
    for (auto& [name, layer] : m_document->GetLayersMutable())
        layer.SetLocked(false);
    RefreshLayerPanel();
    statusBar()->showMessage("Tüm katman kilitleri kaldırıldı", 2000);
}

void MainWindow::OnPurge() {
    if (!m_document) return;

    std::unordered_set<std::string> usedLayers;
    for (const auto& e : m_document->GetCADEntities())
        if (e) usedLayers.insert(e->GetLayerName());
    usedLayers.insert("0");

    auto& layers = m_document->GetLayersMutable();
    std::vector<std::string> toRemove;
    for (const auto& [name, _] : layers)
        if (!usedLayers.count(name)) toRemove.push_back(name);

    if (toRemove.empty()) {
        QMessageBox::information(this, "PURGE", "Kullanılmayan katman veya blok bulunamadı.");
        return;
    }

    QString msg = QString("%1 kullanılmayan katman bulundu:\n").arg(toRemove.size());
    for (const auto& n : toRemove) msg += QString("  • %1\n").arg(QString::fromStdString(n));
    msg += "\nSilmek istiyor musunuz?";
    if (QMessageBox::question(this, "PURGE", msg) != QMessageBox::Yes) return;

    for (const auto& n : toRemove) layers.erase(n);
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(m_document->GetLayers());
    RefreshLayerPanel();
    m_document->SetModified(true);
    statusBar()->showMessage(QString("PURGE: %1 katman silindi").arg(toRemove.size()), 3000);
}

void MainWindow::OnSelectByLayer(const QString& layerName) {
    if (!m_document) return;
    const std::string layStd = layerName.toStdString();

    m_selectedCADEntityIds.clear();
    m_selectedCADEntityId = 0;

    for (const auto& e : m_document->GetCADEntities()) {
        if (!e || !e->IsVisible()) continue;
        const auto& layers = m_document->GetLayers();
        auto lit = layers.find(e->GetLayerName());
        if (lit != layers.end() && (lit->second.IsLocked() || !lit->second.IsDrawable())) continue;
        if (e->GetLayerName() == layStd)
            m_selectedCADEntityIds.insert(e->GetId());
    }

    if (!m_selectedCADEntityIds.empty()) {
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
        statusBar()->showMessage(
            QString("'%1' katmanında %2 entity seçildi").arg(layerName).arg(m_selectedCADEntityIds.size()), 4000);
    } else {
        statusBar()->showMessage(QString("'%1' katmanında seçilebilir entity bulunamadı").arg(layerName), 2000);
    }
}

void MainWindow::OnMoveToLayer() {
    if (!m_document) return;

    if (m_moveToLayerTarget.isEmpty()) {
        QStringList layerNames;
        for (const auto& [n, _] : m_document->GetLayers())
            layerNames << QString::fromStdString(n);
        layerNames.sort();
        bool ok = false;
        QString chosen = QInputDialog::getItem(this, "Katmana Taşı", "Hedef katman:", layerNames, 0, false, &ok);
        if (!ok || chosen.isEmpty()) return;
        m_moveToLayerTarget = chosen;
    }

    const std::string targetStd = m_moveToLayerTarget.toStdString();
    m_moveToLayerTarget.clear();

    auto& layers = m_document->GetLayersMutable();
    if (!layers.count(targetStd))
        layers[targetStd] = cad::Layer(targetStd);

    auto moveIds = m_selectedCADEntityIds;
    if (moveIds.empty() && m_selectedCADEntityId != 0) moveIds.insert(m_selectedCADEntityId);

    int moved = 0;
    for (const auto& e : m_document->GetCADEntities()) {
        if (!e || !moveIds.count(e->GetId())) continue;
        e->SetLayerName(targetStd);
        ++moved;
    }

    if (moved > 0) {
        m_document->SetModified(true);
        RebuildCADEntityCache();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetLayerMap(layers);
        InvalidateRenderer();
        RefreshLayerPanel();
        statusBar()->showMessage(
            QString("%1 entity '%2' katmanına taşındı").arg(moved).arg(QString::fromStdString(targetStd)), 3000);
    }
}

// ═══════════════════════════════════════════════════════════
//  LAYERSTATE — adlı katman anlık görüntüsü
// ═══════════════════════════════════════════════════════════
void MainWindow::OnLayerStateSave() {
    if (!m_document) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, "Katman Durumu Kaydet",
        "Anlık görüntü adı:", QLineEdit::Normal,
        QString("Durum_%1").arg(m_layerStates.size() + 1), &ok);
    if (!ok || name.isEmpty()) return;

    LayerSnapshot snap;
    snap.name = name.toStdString();
    for (const auto& [n, layer] : m_document->GetLayers()) {
        snap.visibility[n] = layer.IsVisible();
        snap.locked[n]     = layer.IsLocked();
        snap.frozen[n]     = layer.IsFrozen();
    }
    for (auto& s : m_layerStates) {
        if (s.name == snap.name) { s = snap; goto saved; }
    }
    m_layerStates.push_back(snap);
saved:
    statusBar()->showMessage(QString("Katman durumu kaydedildi: %1").arg(name), 3000);
    if (m_logList) m_logList->addItem(QString("LAYERSTATE: '%1' kaydedildi (%2 katman)")
        .arg(name).arg(snap.visibility.size()));
}

void MainWindow::OnLayerStateRestore() {
    if (!m_document || m_layerStates.empty()) {
        statusBar()->showMessage("Kayıtlı katman durumu yok — önce LAYERSTATE ile kaydedin", 3000);
        return;
    }
    QStringList names;
    for (const auto& s : m_layerStates) names << QString::fromStdString(s.name);
    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Katman Durumu Yükle",
        "Hangi anlık görüntü?", names, 0, false, &ok);
    if (!ok) return;

    const LayerSnapshot* snap = nullptr;
    for (const auto& s : m_layerStates)
        if (s.name == chosen.toStdString()) { snap = &s; break; }
    if (!snap) return;

    auto& layers = m_document->GetLayersMutable();
    for (auto& [n, layer] : layers) {
        auto visIt = snap->visibility.find(n);
        if (visIt != snap->visibility.end()) layer.SetVisible(visIt->second);
        auto lckIt = snap->locked.find(n);
        if (lckIt != snap->locked.end()) layer.SetLocked(lckIt->second);
        auto frzIt = snap->frozen.find(n);
        if (frzIt != snap->frozen.end()) layer.SetFrozen(frzIt->second);
    }
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetLayerMap(layers);
    RefreshLayerPanel();
    InvalidateRenderer();
    statusBar()->showMessage(QString("Katman durumu yüklendi: %1").arg(chosen), 3000);
}

void MainWindow::OnLayerStateList() {
    if (m_layerStates.empty()) {
        statusBar()->showMessage("Kayıtlı katman durumu yok", 2000);
        return;
    }
    QStringList info;
    for (const auto& s : m_layerStates)
        info << QString("  • %1 (%2 katman)").arg(QString::fromStdString(s.name)).arg(s.visibility.size());
    QMessageBox::information(this, "Kayıtlı Katman Durumları",
        "LAYERSTATE anlık görüntüleri:\n" + info.join("\n"));
}

// ═══════════════════════════════════════════════════════════
//  KATMAN-STANDART — hazır katman şablonu uygula (TS/ISO/AIA)
// ═══════════════════════════════════════════════════════════
void MainWindow::OnLayerStandard() {
    if (!m_document) return;

    QStringList presets = {
        "TS/MEP — Türkiye MEP standartı (VKT varsayılan)",
        "ISO 13567 — Uluslararası yapı katman standartı",
        "AIA CAD Layer Guidelines — Amerikan mimar standartı"
    };

    bool ok;
    QString choice = QInputDialog::getItem(this, "Katman Şablonu",
        "Uygulanacak katman standardını seçin:", presets, 0, false, &ok);
    if (!ok) return;

    struct LayerDef { const char* name; const char* group; int r, g, b; };

    static const LayerDef kTS[] = {
        {"DUVAR",           "Mimari",       220, 220, 220},
        {"KAPI-PENCERE",    "Mimari",       180, 180, 180},
        {"KOLON",           "Mimari",       200, 200, 200},
        {"KIRIS",           "Mimari",       190, 190, 190},
        {"PENCERE",         "Mimari",       170, 210, 230},
        {"KAPI",            "Mimari",       170, 200, 170},
        {"TAVAN",           "Mimari",       160, 160, 160},
        {"ZEMIN",           "Mimari",       140, 140, 140},
        {"VKT-TEMIZ-SU",    "Su Tesisat",     0, 170, 255},
        {"VKT-SICAK-SU",    "Su Tesisat",   255,  60,  60},
        {"VKT-ATIK-SU",     "Su Tesisat",   139,  90,  43},
        {"VKT-GAZ",         "Gaz",          255, 255,   0},
        {"VKT-ISITMA",      "Isitma",       255, 140,   0},
        {"VKT-ISITMA-DONUS","Isitma",       200,  90,   0},
        {"VKT-YANGIN",      "Yangin",       200,   0,   0},
        {"VKT-DIM",         "Olculendirme",   0, 200, 200},
        {"VKT-NODE",        "MEP",          200, 200, 200},
        {"0",               "",             255, 255, 255},
    };

    static const LayerDef kISO[] = {
        {"A-WALL",          "Architecture", 220, 220, 220},
        {"A-DOOR",          "Architecture", 180, 180, 180},
        {"A-COLS",          "Architecture", 200, 200, 200},
        {"P-PIPE-SUPP",     "Plumbing",       0, 170, 255},
        {"P-PIPE-SANR",     "Plumbing",     139,  90,  43},
        {"M-HVAC-DUCT",     "Mechanical",   100, 200, 100},
        {"E-POWR",          "Electrical",   255, 200,   0},
        {"F-SPKL",          "Fire",         200,   0,   0},
        {"G-ANNO-DIMS",     "Annotation",     0, 200, 200},
        {"0",               "",             255, 255, 255},
    };

    static const LayerDef kAIA[] = {
        {"A-WALL",          "Architecture", 220, 220, 220},
        {"A-DOOR",          "Architecture", 180, 180, 180},
        {"P-PIPE",          "Plumbing",       0, 170, 255},
        {"P-DRAIN",         "Plumbing",     139,  90,  43},
        {"M-DUCT",          "Mechanical",   100, 200, 100},
        {"E-POWER",         "Electrical",   255, 200,   0},
        {"F-SPRNK",         "Fire",         200,   0,   0},
        {"A-ANNO-DIMS",     "Annotation",     0, 200, 200},
        {"G-ANNO-TEXT",     "Annotation",   200, 200, 200},
        {"0",               "",             255, 255, 255},
    };

    const LayerDef* defs = kTS;
    int count = static_cast<int>(sizeof(kTS)/sizeof(kTS[0]));
    if (choice.startsWith("ISO")) {
        defs = kISO; count = static_cast<int>(sizeof(kISO)/sizeof(kISO[0]));
    } else if (choice.startsWith("AIA")) {
        defs = kAIA; count = static_cast<int>(sizeof(kAIA)/sizeof(kAIA[0]));
    }

    auto& layerMap = m_document->GetLayersMutable();
    for (int i = 0; i < count; ++i) {
        const auto& d = defs[i];
        std::string n(d.name);
        if (layerMap.find(n) == layerMap.end())
            layerMap[n] = cad::Layer(n);
        layerMap[n].SetGroup(d.group);
        layerMap[n].SetColor(cad::Color((uint8_t)d.r, (uint8_t)d.g, (uint8_t)d.b));
    }

    m_document->SetModified(true);
    RefreshLayerPanel();
    statusBar()->showMessage(
        QString("Katman şablonu uygulandı: %1 — %2 katman").arg(choice.section(' ', 0, 0)).arg(count), 5000);
    if (m_logList)
        m_logList->addItem(QString("[KATMAN-STANDART] %1").arg(choice.section(' ', 0, 0)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Katman Yöneticisi panelini güncelle
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::RefreshLayerPanel() {
    if (!m_layerList) return;
    m_layerList->blockSignals(true);
    m_layerList->clear();
    if (!m_document) {
        m_layerList->blockSignals(false);
        return;
    }
    const auto& layers = m_document->GetLayers();
    std::vector<std::string> names;
    names.reserve(layers.size());
    for (const auto& [name, _] : layers) names.push_back(name);
    std::sort(names.begin(), names.end());

    std::unordered_map<std::string, QTreeWidgetItem*> groupItems;

    auto makeLayerRow = [&](const cad::Layer& layer,
                            const std::string& name,
                            QTreeWidgetItem* parent) -> QTreeWidgetItem* {
        QTreeWidgetItem* item = parent
            ? new QTreeWidgetItem(parent)
            : new QTreeWidgetItem(m_layerList);

        item->setText(0, layer.IsVisible() ? "●" : "○");
        item->setForeground(0, layer.IsVisible() ? QColor(80,200,80) : QColor(120,120,120));
        item->setToolTip(0, layer.IsVisible() ? "Görünür — tıkla: gizle" : "Gizli — tıkla: göster");

        item->setText(1, layer.IsLocked() ? "L" : "-");
        item->setForeground(1, layer.IsLocked() ? QColor(255,180,0) : QColor(80,80,80));
        item->setToolTip(1, layer.IsLocked() ? "Kilitli — tıkla: aç" : "Serbest — tıkla: kilitle");

        item->setText(2, layer.IsFrozen() ? "F" : "-");
        item->setForeground(2, layer.IsFrozen() ? QColor(100,180,255) : QColor(80,80,80));
        item->setToolTip(2, layer.IsFrozen() ? "Dondurulmuş — tıkla: çöz" : "Aktif — tıkla: dondur");

        item->setText(3, QString::fromStdString(name));
        cad::Color col = layer.GetColor();
        QColor qcol(col.r, col.g, col.b);
        if (col.r < 30 && col.g < 30 && col.b < 30) qcol = QColor(180,180,180);
        item->setForeground(3, qcol);

        if (layer.IsFrozen() || !layer.IsVisible()) {
            for (int c = 0; c < 4; ++c)
                item->setForeground(c, QColor(70,70,70));
        }
        return item;
    };

    for (const auto& name : names) {
        const cad::Layer& layer = layers.at(name);
        const std::string& grp  = layer.GetGroup();

        if (grp.empty()) {
            makeLayerRow(layer, name, nullptr);
        } else {
            if (groupItems.find(grp) == groupItems.end()) {
                auto* gItem = new QTreeWidgetItem(m_layerList);
                gItem->setText(3, QString::fromStdString(grp));
                gItem->setForeground(3, QColor(200, 200, 120));
                QFont f = gItem->font(3); f.setBold(true);
                gItem->setFont(3, f);
                gItem->setExpanded(true);
                groupItems[grp] = gItem;
            }
            makeLayerRow(layer, name, groupItems[grp]);
        }
    }
    m_layerList->setRootIsDecorated(!groupItems.empty());
    m_layerList->blockSignals(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Seçili entity'nin katmanını panelde vurgula
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::HighlightLayerInPanel(const std::string& layerName) {
    if (!m_layerList) return;
    const QString qName = QString::fromStdString(layerName);
    static const QColor kActiveBg(60, 55, 20);
    static const QColor kDefaultBg(30, 30, 30);
    for (int i = 0; i < m_layerList->topLevelItemCount(); ++i) {
        auto* item = m_layerList->topLevelItem(i);
        if (!item) continue;
        const bool isActive = (item->text(3) == qName);
        QFont f = item->font(3);
        if (f.bold() != isActive) {
            f.setBold(isActive);
            item->setFont(3, f);
        }
        const QColor needed = isActive ? kActiveBg : kDefaultBg;
        if (item->background(3).color() != needed)
            item->setBackground(3, needed);
        if (isActive)
            m_layerList->scrollToItem(item);
    }
    if (m_layerPanel) {
        m_layerPanel->setWindowTitle(qName.isEmpty()
            ? "Katmanlar"
            : QString("Katmanlar  [%1]").arg(qName));
    }
}

} // namespace ui
} // namespace vkt
