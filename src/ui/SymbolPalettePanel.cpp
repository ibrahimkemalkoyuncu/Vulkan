/**
 * @file SymbolPalettePanel.cpp
 */

#include "ui/SymbolPalettePanel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QByteArray>

#include "cad/FixtureSymbolLibrary.hpp"

namespace vkt {
namespace ui {

// ── Kategori tanımları ────────────────────────────────────────────────────────
struct Category {
    const char* name;
    std::vector<cad::FixtureSymbolType> types;
};

static const Category kCategories[] = {
    { "Su Tesisat", {
        cad::FixtureSymbolType::Washbasin,
        cad::FixtureSymbolType::WC,
        cad::FixtureSymbolType::Shower,
        cad::FixtureSymbolType::Bathtub,
        cad::FixtureSymbolType::DishwasherConnection,
        cad::FixtureSymbolType::WashingMachineConnection,
        cad::FixtureSymbolType::Drain,
        cad::FixtureSymbolType::Source,
        cad::FixtureSymbolType::WaterMeter,
        cad::FixtureSymbolType::Junction,
    }},
    { "Vana / Armatür", {
        cad::FixtureSymbolType::GateValve,
        cad::FixtureSymbolType::CheckValve,
        cad::FixtureSymbolType::BallValve,
        cad::FixtureSymbolType::Pump,
    }},
    { "Gaz", {
        cad::FixtureSymbolType::GasAppliance,
        cad::FixtureSymbolType::GasValve,
    }},
    { "Isıtma", {
        cad::FixtureSymbolType::Boiler,
        cad::FixtureSymbolType::Radiator,
        cad::FixtureSymbolType::HotSource,
    }},
    { "Yangın", {
        cad::FixtureSymbolType::Sprinkler,
        cad::FixtureSymbolType::FirePump,
    }},
};

// ── Constructor ───────────────────────────────────────────────────────────────
SymbolPalettePanel::SymbolPalettePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(4);

    m_tabs = new QTabWidget(this);
    m_tabs->setIconSize(QSize(kIconSize, kIconSize));
    vl->addWidget(m_tabs, 1);

    BuildCategoryTabs();

    // Yerleştir butonu
    auto* btnRow = new QHBoxLayout;
    m_btnPlace = new QPushButton("Yerleştir", this);
    m_btnPlace->setEnabled(false);
    m_btnPlace->setToolTip("Seçili sembolü çizime yerleştir (çift tık da çalışır)");
    btnRow->addWidget(m_btnPlace);

    m_btnCustom = new QPushButton("JSON Yükle...", this);
    m_btnCustom->setToolTip("Kullanıcı tanımlı sembol JSON dosyası yükle");
    btnRow->addWidget(m_btnCustom);
    vl->addLayout(btnRow);

    m_lblHint = new QLabel("Sembol seçin, sonra 'Yerleştir'e tıklayın", this);
    m_lblHint->setWordWrap(true);
    m_lblHint->setStyleSheet("color: gray; font-size: 10px;");
    vl->addWidget(m_lblHint);

    connect(m_btnPlace,  &QPushButton::clicked, this, &SymbolPalettePanel::OnPlaceClicked);
    connect(m_btnCustom, &QPushButton::clicked, this, &SymbolPalettePanel::OnLoadCustom);
}

void SymbolPalettePanel::BuildCategoryTabs() {
    for (const auto& cat : kCategories) {
        auto* listWidget = new QListWidget;
        listWidget->setViewMode(QListWidget::IconMode);
        listWidget->setIconSize(QSize(kIconSize, kIconSize));
        listWidget->setGridSize(QSize(kIconSize + 16, kIconSize + 24));
        listWidget->setResizeMode(QListWidget::Adjust);
        listWidget->setMovement(QListWidget::Static);
        listWidget->setWordWrap(true);
        listWidget->setSpacing(2);

        AddSymbolsToList(listWidget, cat.types);

        connect(listWidget, &QListWidget::itemDoubleClicked,
                this, &SymbolPalettePanel::OnItemDoubleClicked);
        connect(listWidget, &QListWidget::currentItemChanged,
                this, [this](QListWidgetItem* item, QListWidgetItem*) {
                    if (item) {
                        m_selectedType = item->data(Qt::UserRole).toString();
                        m_btnPlace->setEnabled(true);
                        m_lblHint->setText("Seçili: " + m_selectedType);
                    }
                });

        m_tabs->addTab(listWidget, QString::fromUtf8(cat.name));
    }
}

void SymbolPalettePanel::AddSymbolsToList(
        QListWidget* list,
        const std::vector<cad::FixtureSymbolType>& types)
{
    for (auto t : types) {
        QString name = QString::fromStdString(
            cad::FixtureSymbolLibrary::GetDisplayName(t));
        auto* item = new QListWidgetItem(MakeSymbolIcon(t), name, list);
        item->setData(Qt::UserRole, name);
        item->setToolTip(name);
    }
}

QIcon SymbolPalettePanel::MakeSymbolIcon(cad::FixtureSymbolType type) {
    // Build SVG from FixtureSymbolLibrary and render to QPixmap
    auto& lib = cad::FixtureSymbolLibrary::Instance();
    auto sym   = lib.GetSymbol(type, 1.0, 0.0, 0.0);
    std::string svgStr = cad::FixtureSymbolLibrary::ToSVG(sym, "#003399");

    QPixmap pix(kIconSize, kIconSize);
    pix.fill(Qt::white);

    QByteArray ba(svgStr.c_str(), static_cast<int>(svgStr.size()));
    QSvgRenderer renderer(ba);
    if (renderer.isValid()) {
        QPainter painter(&pix);
        renderer.render(&painter);
        painter.end();
    } else {
        // Fallback: simple text label
        QPainter p(&pix);
        p.setPen(Qt::darkBlue);
        p.drawRect(2, 2, kIconSize-4, kIconSize-4);
        p.drawText(pix.rect(), Qt::AlignCenter,
            QString::fromStdString(cad::FixtureSymbolLibrary::GetDisplayName(type)).left(4));
        p.end();
    }
    return QIcon(pix);
}

void SymbolPalettePanel::OnItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    m_selectedType = item->data(Qt::UserRole).toString();
    OnPlaceClicked();
}

void SymbolPalettePanel::OnPlaceClicked() {
    if (m_selectedType.isEmpty()) return;
    emit FixtureTypeSelected(m_selectedType);
    if (m_placeCallback)
        m_placeCallback(m_selectedType.toStdString());
    m_lblHint->setText("Yerleştirme modu: " + m_selectedType + " — haritaya tıklayın");
}

void SymbolPalettePanel::OnLoadCustom() {
    QString path = QFileDialog::getOpenFileName(
        this, "Özel Sembol JSON Yükle", QString(),
        "Sembol JSON (*.json)");
    if (path.isEmpty()) return;

    // Register as Washbasin by default (user can rename); just show confirmation
    auto& lib = cad::FixtureSymbolLibrary::Instance();
    if (lib.RegisterCustom(path.toStdString(),
                            cad::FixtureSymbolType::Washbasin)) {
        m_lblHint->setText("Özel sembol yüklendi: " + path.section('/', -1));
    } else {
        m_lblHint->setText("Hata: JSON parse edilemedi!");
    }
}

void SymbolPalettePanel::LoadCustomSymbols(const std::string& dir) {
    cad::FixtureSymbolLibrary::Instance().LoadCustomDirectory(dir);
}

} // namespace ui
} // namespace vkt
