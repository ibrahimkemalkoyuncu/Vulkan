/**
 * @file MainWindow_CADEdit.cpp
 * @brief CAD editing methods split from MainWindow.cpp
 *
 * Contains: OnTrim, OnExtend, OnSelectAll, OnScale, OnRotate,
 *           OnStretch, OnWBlock, OnMirror, OnOffset, OnCopy, OnPaste, OnDelete
 */

#include "ui/MainWindow.hpp"
#include "core/Commands.hpp"
#include "core/ProjectManager.hpp"
#include "cad/Line.hpp"
#include "cad/Polyline.hpp"
#include "cad/DXFWriter.hpp"
#include "render/VulkanWindow.hpp"

#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

#include <cmath>
#include <algorithm>
#include <vector>

namespace vkt {
namespace ui {

// ============================================================
//  TRIM komutu  (#16) -- iki cizginin kesisimine kisalt
// ============================================================
void MainWindow::OnTrim() {
    if (!m_document) return;

    // Adim 1: Sinir entity sec (tek tikla)
    bool ok;
    QString info = "TRIM: Once sinir cizgiyi secin (entity ID girin).\n"
                   "Secim kutusu ile secili entity ID'yi kullanin veya ID yazin.";

    // Secili entity'yi sinir olarak kullan
    cad::EntityId boundId = m_selectedCADEntityId;
    if (m_selectedCADEntityIds.size() == 1) boundId = *m_selectedCADEntityIds.begin();

    if (boundId == 0) {
        QMessageBox::information(this, "TRIM",
            "Önce sınır olarak kullanılacak entity'yi seçin (tek tıklama),\n"
            "sonra TRIM komutunu çalıştırın.");
        return;
    }

    auto boundIt = m_cadEntityCache.find(boundId);
    if (boundIt == m_cadEntityCache.end() || !boundIt->second ||
        boundIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::information(this, "TRIM", "Sınır entity bir Line olmalıdır.");
        return;
    }
    const auto* boundLine = static_cast<const cad::Line*>(boundIt->second);

    // Adim 2: Kisaltilacak entity ID gir
    int trimId = QInputDialog::getInt(this, "TRIM — Kısaltılacak Entity",
        QString("Sınır: Entity #%1 (Line)\nKısaltılacak entity ID:").arg(boundId),
        0, 1, INT_MAX, 1, &ok);
    if (!ok || trimId == 0) return;

    auto trimIt = m_cadEntityCache.find(static_cast<cad::EntityId>(trimId));
    if (trimIt == m_cadEntityCache.end() || !trimIt->second ||
        trimIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::warning(this, "TRIM", "Kısaltılacak entity bir Line olmalıdır ve var olmalıdır.");
        return;
    }
    auto* trimLine = static_cast<cad::Line*>(trimIt->second);

    // Line-Line kesisim noktasini hesapla (2D)
    geom::Vec3 p1 = boundLine->GetStart(), p2 = boundLine->GetEnd();
    geom::Vec3 p3 = trimLine->GetStart(),  p4 = trimLine->GetEnd();

    double d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    double denom = d1x * d2y - d1y * d2x;
    if (std::abs(denom) < 1e-10) {
        QMessageBox::information(this, "TRIM", "Çizgiler paralel — kesişim yok.");
        return;
    }
    double t = ((p3.x - p1.x) * d2y - (p3.y - p1.y) * d2x) / denom;
    double s = ((p3.x - p1.x) * d1y - (p3.y - p1.y) * d1x) / denom;

    if (s < 0.0 || s > 1.0) {
        QMessageBox::information(this, "TRIM", "Kesişim nokta uzantıda — trim uygulanamaz.");
        return;
    }

    geom::Vec3 ix{p3.x + s * d2x, p3.y + s * d2y, p3.z};

    // Kisaltma: hangi tarafi kes? (sinira gore baslangica mi yoksa sona mi yakin?)
    QStringList sides = {"Başlangıç tarafını kes (intersection'dan sona doğru kalsın)",
                         "Son tarafını kes (başlangıçtan intersection'a doğru kalsın)"};
    QString side = QInputDialog::getItem(this, "TRIM — Taraf Seçimi",
        QString("Kesişim: (%.1f, %.1f)\nHangi taraf kesilsin?").arg(ix.x).arg(ix.y),
        sides, 0, false, &ok);
    if (!ok) return;

    // Revert + command ile undo stack
    geom::Vec3 origStart = trimLine->GetStart();
    geom::Vec3 origEnd   = trimLine->GetEnd();

    if (sides.indexOf(side) == 0) {
        // Baslangic kesilir -> start = intersection
        trimLine->SetStart(origStart); // revert
        auto cmd = std::make_unique<core::GripEditLineCommand>(trimLine, 0, origStart, ix);
        m_document->ExecuteCommand(std::move(cmd));
    } else {
        // Son kesilir -> end = intersection
        trimLine->SetEnd(origEnd); // revert
        auto cmd = std::make_unique<core::GripEditLineCommand>(trimLine, 2, origEnd, ix);
        m_document->ExecuteCommand(std::move(cmd));
    }

    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("TRIM uygulandı — Entity #%1 kesişim noktasına kısaltıldı (Ctrl+Z ile geri alınabilir)").arg(trimId), 3000);
}

// ============================================================
//  EXTEND komutu -- cizgiyi sinira uzat (TRIM'in tersi)
// ============================================================
void MainWindow::OnExtend() {
    if (!m_document) return;

    // Sinir entity secili olmali
    cad::EntityId boundId = m_selectedCADEntityId;
    if (m_selectedCADEntityIds.size() == 1) boundId = *m_selectedCADEntityIds.begin();

    if (boundId == 0) {
        QMessageBox::information(this, "EXTEND",
            "Önce sınır olarak kullanılacak entity'yi seçin,\n"
            "sonra EXTEND komutunu çalıştırın.");
        return;
    }

    auto boundIt = m_cadEntityCache.find(boundId);
    if (boundIt == m_cadEntityCache.end() || !boundIt->second ||
        boundIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::information(this, "EXTEND", "Sınır entity bir Line olmalıdır.");
        return;
    }
    const auto* boundLine = static_cast<const cad::Line*>(boundIt->second);

    bool ok;
    int extId = QInputDialog::getInt(this, "EXTEND — Uzatılacak Entity",
        QString("Sınır: Entity #%1 (Line)\nUzatılacak entity ID:").arg(boundId),
        0, 1, INT_MAX, 1, &ok);
    if (!ok || extId == 0) return;

    auto extIt = m_cadEntityCache.find(static_cast<cad::EntityId>(extId));
    if (extIt == m_cadEntityCache.end() || !extIt->second ||
        extIt->second->GetType() != cad::EntityType::Line) {
        QMessageBox::warning(this, "EXTEND", "Uzatılacak entity bir Line olmalıdır ve var olmalıdır.");
        return;
    }
    auto* extLine = static_cast<cad::Line*>(extIt->second);

    // Line-Line kesisim hesabi -- TRIM'den farkli: s siniri yok, uzantida da gecerli
    geom::Vec3 p1 = boundLine->GetStart(), p2 = boundLine->GetEnd();
    geom::Vec3 p3 = extLine->GetStart(),   p4 = extLine->GetEnd();

    double d1x = p2.x - p1.x, d1y = p2.y - p1.y;
    double d2x = p4.x - p3.x, d2y = p4.y - p3.y;
    double denom = d1x * d2y - d1y * d2x;
    if (std::abs(denom) < 1e-10) {
        QMessageBox::information(this, "EXTEND", "Çizgiler paralel — kesişim yok.");
        return;
    }
    double s = ((p3.x - p1.x) * d1y - (p3.y - p1.y) * d1x) / denom;

    // EXTEND icin s herhangi bir degerde olabilir (uzanti da dahil)
    // ama sinir cizginin kendi uzantisina gore t kontrol et
    double t = ((p3.x - p1.x) * d2y - (p3.y - p1.y) * d2x) / denom;
    if (t < -0.001 || t > 1.001) {
        QMessageBox::information(this, "EXTEND",
            "Uzatılacak çizgi sınırın uzantısında değil.");
        return;
    }

    geom::Vec3 ix{p3.x + s * d2x, p3.y + s * d2y, p3.z};

    // Hangi uc sinira daha yakin? O ucu uzat
    double distToStart = std::hypot(p3.x - ix.x, p3.y - ix.y);
    double distToEnd   = std::hypot(p4.x - ix.x, p4.y - ix.y);

    geom::Vec3 origStart = extLine->GetStart();
    geom::Vec3 origEnd   = extLine->GetEnd();

    if (distToEnd < distToStart) {
        // Bitis ucu sinira uzatilir
        auto cmd = std::make_unique<core::GripEditLineCommand>(extLine, 2, origEnd, ix);
        m_document->ExecuteCommand(std::move(cmd));
    } else {
        // Baslangic ucu sinira uzatilir
        auto cmd = std::make_unique<core::GripEditLineCommand>(extLine, 0, origStart, ix);
        m_document->ExecuteCommand(std::move(cmd));
    }

    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("EXTEND uygulandı — Entity #%1 sınıra uzatıldı (Ctrl+Z ile geri alınabilir)").arg(extId), 3000);
}

void MainWindow::OnSelectAll() {
    if (!m_document) return;
    m_selectedCADEntityIds.clear();
    m_selectedCADEntityId = 0;
    for (const auto& e : m_document->GetCADEntities()) {
        if (e && e->IsVisible()) m_selectedCADEntityIds.insert(e->GetId());
    }
    if (!m_selectedCADEntityIds.empty()) {
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
        ComputeGrips();
        statusBar()->showMessage(QString("%1 entity seçildi (Tümü)").arg(m_selectedCADEntityIds.size()), 2000);
    }
}

// ============================================================
//  SCALE (OLCEKLE) -- FineSANI uyumlu
// ============================================================
void MainWindow::OnScale() {
    if (!m_document) return;
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
    if (ids.empty()) {
        statusBar()->showMessage("Önce entity seçin, sonra SCALE komutunu çalıştırın", 2000);
        return;
    }

    bool ok;
    double factor = QInputDialog::getDouble(this, "Ölçekle (Scale)",
        "Ölçek faktörü:\n"
        "  0.001 = mm→m  |  1000 = m→mm\n"
        "  0.01 = cm→m   |  100  = m→cm\n"
        "  2.0  = 2 katı |  0.5  = yarıya küçült",
        1.0, 0.0001, 1000000.0, 6, &ok);
    if (!ok || std::abs(factor - 1.0) < 1e-12) return;

    // Olcekleme merkezi: secimin BBox merkezi
    double cx = 0, cy = 0;
    int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        auto c = it->second->GetBounds().GetCenter();
        cx += c.x; cy += c.y; ++cnt;
    }
    if (cnt > 0) { cx /= cnt; cy /= cnt; }

    // Undo snapshot
    auto& docEnts = m_document->GetCADEntitiesMutable();
    std::vector<cad::EntityId> snapIds;
    std::vector<std::unique_ptr<cad::Entity>> snapshots;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        snapIds.push_back(eid);
        snapshots.push_back(it->second->Clone());
    }

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;
        e->Move({-cx, -cy, 0});
        e->Scale({factor, factor, 1.0});
        e->Move({cx, cy, 0});
    }

    auto cmd = std::make_unique<core::TransformCADEntitiesCommand>(
        docEnts, std::move(snapIds), std::move(snapshots));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("%1 entity ×%2 ölçeklendi (Ctrl+Z geri al)").arg(ids.size()).arg(factor), 2000);
}

// ============================================================
//  ROTATE (DONDUR) -- serbest aci
// ============================================================
void MainWindow::OnRotate() {
    if (!m_document) return;
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
    if (ids.empty()) {
        statusBar()->showMessage("Önce entity seçin, sonra ROTATE komutunu çalıştırın", 2000);
        return;
    }

    bool ok;
    double angle = QInputDialog::getDouble(this, "Döndür (Rotate)",
        "Dönüş açısı (derece, saat yönünün tersi pozitif):",
        90.0, -360.0, 360.0, 2, &ok);
    if (!ok || std::abs(angle) < 1e-9) return;

    // Dondurme merkezi: secimin BBox merkezi
    double cx = 0, cy = 0;
    int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        auto c = it->second->GetBounds().GetCenter();
        cx += c.x; cy += c.y; ++cnt;
    }
    if (cnt > 0) { cx /= cnt; cy /= cnt; }

    // Undo snapshot
    auto& docEnts = m_document->GetCADEntitiesMutable();
    std::vector<cad::EntityId> snapIds;
    std::vector<std::unique_ptr<cad::Entity>> snapshots;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        snapIds.push_back(eid);
        snapshots.push_back(it->second->Clone());
    }

    double rad = angle * 3.14159265358979323846 / 180.0;
    double cosA = std::cos(rad), sinA = std::sin(rad);

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;

        if (e->GetType() == cad::EntityType::Line) {
            auto* ln = static_cast<cad::Line*>(e);
            auto rotPt = [&](geom::Vec3 p) -> geom::Vec3 {
                double rx = (p.x - cx) * cosA - (p.y - cy) * sinA + cx;
                double ry = (p.x - cx) * sinA + (p.y - cy) * cosA + cy;
                return {rx, ry, p.z};
            };
            ln->SetStart(rotPt(ln->GetStart()));
            ln->SetEnd(rotPt(ln->GetEnd()));
        } else {
            e->Move({-cx, -cy, 0});
            e->Rotate(angle);
            e->Move({cx, cy, 0});
        }
    }

    auto cmd = std::make_unique<core::TransformCADEntitiesCommand>(
        docEnts, std::move(snapIds), std::move(snapshots));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("%1 entity %2° döndürüldü (Ctrl+Z geri al)").arg(ids.size()).arg(angle), 2000);
}

// ============================================================
//  STRETCH (ESNET) -- vertex bazli deformasyon
// ============================================================
void MainWindow::OnStretch() {
    if (!m_document) return;
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
    if (ids.empty()) {
        statusBar()->showMessage("Önce entity seçin, sonra STRETCH komutunu çalıştırın", 2000);
        return;
    }

    bool ok;
    double dx = QInputDialog::getDouble(this, "Esnet (Stretch) — X",
        "X yönünde uzatma (mm, - sola, + sağa):", 0.0, -1e6, 1e6, 1, &ok);
    if (!ok) return;
    double dy = QInputDialog::getDouble(this, "Esnet (Stretch) — Y",
        "Y yönünde uzatma (mm, - aşağı, + yukarı):", 0.0, -1e6, 1e6, 1, &ok);
    if (!ok) return;
    if (std::abs(dx) < 1e-9 && std::abs(dy) < 1e-9) return;

    // Secimin BBox'i -- sag yaridaki vertex'leri tasi (AutoCAD crossing stretch)
    double minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        auto bb = it->second->GetBounds();
        if (bb.min.x < minX) minX = bb.min.x;
        if (bb.max.x > maxX) maxX = bb.max.x;
        if (bb.min.y < minY) minY = bb.min.y;
        if (bb.max.y > maxY) maxY = bb.max.y;
    }
    double midX = (minX + maxX) / 2.0;
    double midY = (minY + maxY) / 2.0;

    // Undo snapshot
    auto& docEnts = m_document->GetCADEntitiesMutable();
    std::vector<cad::EntityId> snapIds;
    std::vector<std::unique_ptr<cad::Entity>> snapshots;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        snapIds.push_back(eid);
        snapshots.push_back(it->second->Clone());
    }

    int stretched = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;

        if (e->GetType() == cad::EntityType::Line) {
            auto* ln = static_cast<cad::Line*>(e);
            geom::Vec3 s = ln->GetStart(), en = ln->GetEnd();
            bool movedS = false, movedE = false;
            if (s.x > midX || (std::abs(dx) < 1e-9 && s.y > midY)) {
                s.x += dx; s.y += dy; movedS = true;
            }
            if (en.x > midX || (std::abs(dx) < 1e-9 && en.y > midY)) {
                en.x += dx; en.y += dy; movedE = true;
            }
            if (movedS) ln->SetStart(s);
            if (movedE) ln->SetEnd(en);
            if (movedS || movedE) ++stretched;
        } else if (e->GetType() == cad::EntityType::Polyline) {
            auto* pl = static_cast<cad::Polyline*>(e);
            bool any = false;
            for (size_t vi = 0; vi < pl->GetVertexCount(); ++vi) {
                auto v = pl->GetVertex(vi);
                if (v.pos.x > midX || (std::abs(dx) < 1e-9 && v.pos.y > midY)) {
                    v.pos.x += dx; v.pos.y += dy;
                    pl->SetVertex(vi, v);
                    any = true;
                }
            }
            if (any) ++stretched;
        } else {
            // Diger entity tipleri icin basit Move
            auto c = e->GetBounds().GetCenter();
            if (c.x > midX || (std::abs(dx) < 1e-9 && c.y > midY)) {
                e->Move({dx, dy, 0}); ++stretched;
            }
        }
    }

    auto cmd = std::make_unique<core::TransformCADEntitiesCommand>(
        docEnts, std::move(snapIds), std::move(snapshots));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(
        QString("STRETCH: %1/%2 entity deform edildi (Ctrl+Z geri al)")
            .arg(stretched).arg(ids.size()), 3000);
}

// ============================================================
//  WBLOCK -- Nesne sec + tutma noktasi + DWG/DXF kaydet
// ============================================================
void MainWindow::OnWBlock() {
    if (!m_document) return;

    // 1. Kaynak secimi: "Nesneler" (secili) veya "Tum cizim"
    QStringList kaynaklar = {"Seçili nesneler", "Tüm çizim"};
    bool ok;
    QString kaynak = QInputDialog::getItem(this, "W-Block — Blok Oluştur",
        "Kaynak:", kaynaklar, 0, false, &ok);
    if (!ok) return;

    std::vector<cad::Entity*> selectedEnts;
    if (kaynak == "Seçili nesneler") {
        std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
        if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
        if (ids.empty()) {
            QMessageBox::information(this, "W-Block",
                "Önce nesneleri seçin, sonra WBLOCK komutunu çalıştırın.\n"
                "Seçim: Ctrl+A (Tümü) veya tıklayarak tek tek seçim.");
            return;
        }
        for (auto eid : ids) {
            auto it = m_cadEntityCache.find(eid);
            if (it != m_cadEntityCache.end() && it->second)
                selectedEnts.push_back(it->second);
        }
    } else {
        for (const auto& e : m_document->GetCADEntities()) {
            if (e) selectedEnts.push_back(e.get());
        }
    }

    if (selectedEnts.empty()) {
        QMessageBox::warning(this, "W-Block", "Herhangi bir nesne seçilmedi.");
        return;
    }

    // 2. Tutma noktasi (base point) -- FineSANI'deki kolon kosesi referansi
    double refX = 0, refY = 0;
    QStringList refOptions = {"Seçimin sol-alt köşesi (otomatik)", "Koordinat gir"};
    QString refChoice = QInputDialog::getItem(this, "W-Block — Tutma Noktası",
        "Referans (tutma) noktası:", refOptions, 0, false, &ok);
    if (!ok) return;

    if (refChoice == refOptions[0]) {
        // Otomatik: BBox sol-alt
        double minX = 1e30, minY = 1e30;
        for (auto* e : selectedEnts) {
            auto bb = e->GetBounds();
            if (bb.min.x < minX) minX = bb.min.x;
            if (bb.min.y < minY) minY = bb.min.y;
        }
        refX = minX; refY = minY;
    } else {
        refX = QInputDialog::getDouble(this, "Tutma Noktası X", "X:", 0.0, -1e9, 1e9, 2, &ok);
        if (!ok) return;
        refY = QInputDialog::getDouble(this, "Tutma Noktası Y", "Y:", 0.0, -1e9, 1e9, 2, &ok);
        if (!ok) return;
    }

    // 3. Dosya kaydetme yolu
    auto& pm = core::ProjectManager::Instance();
    QString defDir = pm.HasActiveProject()
        ? QString::fromStdString(pm.GetMimariFolder())
        : QDir::homePath();

    QString path = QFileDialog::getSaveFileName(this, "W-Block — Dosya Kaydet",
        defDir, "DXF Dosyası (*.dxf);;DWG Dosyası (*.dwg)");
    if (path.isEmpty()) return;

    // 4. Entity'leri klonla ve referans noktasina gore oteleme uygula
    std::vector<std::unique_ptr<cad::Entity>> clones;
    for (auto* e : selectedEnts) {
        auto clone = e->Clone();
        if (clone) {
            clone->Move({-refX, -refY, 0});
            clones.push_back(std::move(clone));
        }
    }

    // 5. DXF olarak yaz
    cad::DXFWriter writer;
    mep::NetworkGraph emptyNet;
    if (writer.Write(path.toStdString(), clones, emptyNet, "W-Block")) {
        statusBar()->showMessage(
            QString("W-Block kaydedildi: %1 (%2 nesne, ref=%3,%4)")
                .arg(QFileInfo(path).fileName()).arg(clones.size()).arg(refX, 0, 'f', 1).arg(refY, 0, 'f', 1), 5000);
        if (m_logList) m_logList->addItem(QString("[WBLOCK] %1 → %2 nesne")
            .arg(QFileInfo(path).fileName()).arg(clones.size()));
    } else {
        QMessageBox::warning(this, "W-Block", "Dosya yazılamadı: " + path);
    }
}

// ============================================================
//  MIRROR (AYNA)  -- #7
// ============================================================
void MainWindow::OnMirror() {
    if (!m_document) return;
    if (m_selectedCADEntityIds.empty() && m_selectedCADEntityId == 0) {
        statusBar()->showMessage("Önce entity seçin, sonra MIRROR komutunu çalıştırın", 2000);
        return;
    }

    // Yansima ekseni: yatay mi, dikey mi?
    QStringList eksenler = {"Yatay eksen (X ekseni boyunca)", "Dikey eksen (Y ekseni boyunca)",
                            "Seçimin merkezi etrafında (180° döndür)"};
    bool ok;
    QString secim = QInputDialog::getItem(this, "Mirror — Ayna", "Yansıma ekseni:", eksenler, 0, false, &ok);
    if (!ok) return;

    // Secili entity'leri topla
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);

    // Secimin BBox merkezi
    double cx = 0, cy = 0;
    int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        auto bb = it->second->GetBounds();
        auto c  = bb.GetCenter();
        cx += c.x; cy += c.y; ++cnt;
    }
    if (cnt > 0) { cx /= cnt; cy /= cnt; }

    // Undo snapshot
    auto& docEnts = m_document->GetCADEntitiesMutable();
    std::vector<cad::EntityId> snapIds;
    std::vector<std::unique_ptr<cad::Entity>> snapshots;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it != m_cadEntityCache.end() && it->second) {
            snapIds.push_back(eid);
            snapshots.push_back(it->second->Clone());
        }
    }

    int eksNo = eksenler.indexOf(secim);
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;
        if (eksNo == 0)
            e->Mirror({cx - 1, cy, 0}, {cx + 1, cy, 0});
        else if (eksNo == 1)
            e->Mirror({cx, cy - 1, 0}, {cx, cy + 1, 0});
        else
            e->Rotate(180.0);
    }

    auto cmd = std::make_unique<core::TransformCADEntitiesCommand>(
        docEnts, std::move(snapIds), std::move(snapshots));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    InvalidateRenderer();
    ComputeGrips();
    statusBar()->showMessage(QString("%1 entity yansıtıldı (Ctrl+Z geri al)").arg(ids.size()), 2000);
}

// ============================================================
//  OFFSET -- paralel kopya  (#8)
// ============================================================
void MainWindow::OnOffset() {
    if (!m_document) return;
    if (m_vulkanWindow) m_vulkanWindow->WaitForCADBuild();

    // Secili entity kontrolu
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);

    if (ids.empty()) {
        statusBar()->showMessage("Önce entity seçin, sonra OFFSET komutunu çalıştırın", 2000);
        return;
    }

    bool ok;
    double dist = QInputDialog::getDouble(this, "Offset — Paralel Kopya",
        "Offset mesafesi (mm):", 100.0, 0.1, 100000.0, 1, &ok);
    if (!ok) return;

    QStringList yonler = {"Sağa / Yukarıya", "Sola / Aşağıya", "Her iki yöne"};
    QString yon = QInputDialog::getItem(this, "Offset Yönü", "Yön:", yonler, 0, false, &ok);
    if (!ok) return;
    int yonIdx = yonler.indexOf(yon);

    auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
        m_document->GetCADEntities());

    std::vector<std::unique_ptr<cad::Entity>> newEnts;

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it == m_cadEntityCache.end() || !it->second) continue;
        cad::Entity* e = it->second;

        auto makeOffset = [&](double d) {
            auto clone = e->Clone();
            if (!clone) return;
            if (e->GetType() == cad::EntityType::Line) {
                const auto* ln = static_cast<const cad::Line*>(e);
                auto* c = static_cast<cad::Line*>(clone.get());
                geom::Vec3 s = ln->GetStart(), en = ln->GetEnd();
                double dx = en.x - s.x, dy = en.y - s.y;
                double len = std::sqrt(dx*dx + dy*dy);
                if (len < 1e-9) return;
                double nx = -dy/len * d, ny = dx/len * d;
                c->SetStart({s.x + nx,  s.y + ny,  s.z});
                c->SetEnd  ({en.x + nx, en.y + ny, en.z});
            } else {
                clone->Move(geom::Vec3(d, 0, 0)); // diger entity'ler icin basit oteleme
            }
            newEnts.push_back(std::move(clone));
        };

        if (yonIdx == 0 || yonIdx == 2) makeOffset( dist);
        if (yonIdx == 1 || yonIdx == 2) makeOffset(-dist);
    }

    std::vector<cad::EntityId> addedIds;
    for (auto& ne : newEnts) {
        addedIds.push_back(ne->GetId());
        entities.push_back(std::move(ne));
    }

    auto cmd = std::make_unique<core::AddCADEntitiesCommand>(entities, std::move(addedIds));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    RebuildCADEntityCache();
    InvalidateRenderer();
    statusBar()->showMessage(
        QString("%1 entity offset edildi (Ctrl+Z geri al)").arg(ids.size() * (yonIdx == 2 ? 2 : 1)), 2000);
}

// ============================================================
//  COPY/PASTE  (#9)
// ============================================================
void MainWindow::OnCopy() {
    if (!m_document) return;
    m_clipboard.clear();
    std::vector<cad::EntityId> ids(m_selectedCADEntityIds.begin(), m_selectedCADEntityIds.end());
    if (m_selectedCADEntityId != 0 && ids.empty()) ids.push_back(m_selectedCADEntityId);
    if (ids.empty()) { statusBar()->showMessage("Kopyalanacak entity seçin", 2000); return; }

    // BBox merkezi -- paste sirasinda ortalama
    double cx = 0, cy = 0; int cnt = 0;
    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it != m_cadEntityCache.end() && it->second) {
            auto c = it->second->GetBounds().GetCenter();
            cx += c.x; cy += c.y; ++cnt;
        }
    }
    if (cnt) { cx /= cnt; cy /= cnt; }
    m_clipboardCenter = {cx, cy, 0};

    for (auto eid : ids) {
        auto it = m_cadEntityCache.find(eid);
        if (it != m_cadEntityCache.end() && it->second) {
            auto clone = it->second->Clone();
            if (clone) m_clipboard.push_back(std::move(clone));
        }
    }
    statusBar()->showMessage(QString("%1 entity kopyalandı — Ctrl+V ile yapıştır").arg(m_clipboard.size()), 2000);
}

void MainWindow::OnPaste() {
    if (m_vulkanWindow) m_vulkanWindow->WaitForCADBuild();
    if (!m_document || m_clipboard.empty()) {
        statusBar()->showMessage("Panoda entity yok — önce Ctrl+C ile kopyalayın", 2000); return;
    }

    bool ok;
    double ox = QInputDialog::getDouble(this, "Yapıştır — Offset X",
        "X öteleme (mm, 0=aynı konum):", 50.0, -1e6, 1e6, 0, &ok);
    if (!ok) return;
    double oy = QInputDialog::getDouble(this, "Yapıştır — Offset Y",
        "Y öteleme (mm, 0=aynı konum):", 50.0, -1e6, 1e6, 0, &ok);
    if (!ok) return;

    auto& entities = m_document->GetCADEntitiesMutable();

    m_selectedCADEntityIds.clear();
    std::vector<cad::EntityId> addedIds;
    for (const auto& src : m_clipboard) {
        auto clone = src->Clone();
        if (!clone) continue;
        clone->Move(geom::Vec3(ox, oy, 0));
        addedIds.push_back(clone->GetId());
        m_selectedCADEntityIds.insert(clone->GetId());
        entities.push_back(std::move(clone));
    }

    auto cmd = std::make_unique<core::AddCADEntitiesCommand>(entities, std::move(addedIds));
    m_document->ExecuteCommand(std::move(cmd));
    m_document->SetModified(true);
    RebuildCADEntityCache();
    if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
        m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds(m_selectedCADEntityIds);
    InvalidateRenderer();
    statusBar()->showMessage(
        QString("%1 entity yapıştırıldı (Ctrl+Z geri al)").arg(m_clipboard.size()), 2000);
}

void MainWindow::OnDelete() {
    if (!m_document) return;

    auto& network = m_document->GetNetwork();

    // Coklu CAD entity secimi varsa tumunu sil
    if (!m_selectedCADEntityIds.empty()) {
        auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
            m_document->GetCADEntities());
        size_t before = entities.size();
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [this](const std::unique_ptr<cad::Entity>& e) {
                return e && m_selectedCADEntityIds.count(e->GetId());
            }), entities.end());
        int deleted = static_cast<int>(before - entities.size());
        m_selectedCADEntityIds.clear();
        m_selectedCADEntityId = 0;
        ClearGrips();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityIds({});
        RebuildCADEntityCache();
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("%1 entity silindi").arg(deleted), 2000);
        return;
    }

    if (m_selectedNodeId != 0) {
        auto cmd = std::make_unique<core::DeleteNodeCommand>(network, m_selectedNodeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Node #%1 silindi").arg(m_selectedNodeId), 2000);
        m_selectedNodeId = 0;
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer()) m_vulkanWindow->GetRenderer()->SetGizmoVisible(false);
        if (m_fixturePanel) m_fixturePanel->Clear();
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else if (m_selectedEdgeId != 0) {
        auto cmd = std::make_unique<core::DeleteEdgeCommand>(network, m_selectedEdgeId);
        m_document->ExecuteCommand(std::move(cmd));
        statusBar()->showMessage(QString("Kenar #%1 silindi").arg(m_selectedEdgeId), 2000);
        m_selectedEdgeId = 0;
        m_document->SetModified(true);
        UpdateUI();
        ScheduleAutoHydro();
    } else if (m_selectedCADEntityId != 0) {
        // Tek secili CAD entity
        auto& entities = const_cast<std::vector<std::unique_ptr<cad::Entity>>&>(
            m_document->GetCADEntities());
        cad::EntityId eid = m_selectedCADEntityId;
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [eid](const std::unique_ptr<cad::Entity>& e){ return e && e->GetId() == eid; }),
            entities.end());
        m_selectedCADEntityId = 0;
        ClearGrips();
        if (m_vulkanWindow && m_vulkanWindow->GetRenderer())
            m_vulkanWindow->GetRenderer()->SetHighlightCADEntityId(0);
        RebuildCADEntityCache();
        m_document->SetModified(true);
        UpdateUI();
        statusBar()->showMessage(QString("Entity #%1 silindi").arg(eid), 2000);
    } else {
        statusBar()->showMessage("Silme: önce bir öğe seçin (Select modu)");
    }
}

} // namespace ui
} // namespace vkt
