/**
 * @file FixturePropertiesPanel.cpp
 * @brief Armatür özellikleri paneli implementasyonu
 */

#include "ui/FixturePropertiesPanel.hpp"
#include "cad/FixtureSymbolLibrary.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QFont>

namespace vkt {
namespace ui {

const QStringList FixturePropertiesPanel::s_fixtureTypes = {
    "Lavabo", "WC", "Duş", "Küvet",
    "Bulaşık Makinesi", "Çamaşır Makinesi",
    "Pompa", "Su Sayacı", "Depo", "Özel"
};

// ═══════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════

FixturePropertiesPanel::FixturePropertiesPanel(QWidget* parent)
    : QWidget(parent) {
    BuildUI();
    Clear();
}

void FixturePropertiesPanel::BuildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    // Başlık
    auto* titleLabel = new QLabel("Armatür Özellikleri", this);
    QFont f = titleLabel->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 1);
    titleLabel->setFont(f);
    root->addWidget(titleLabel);

    // Node ID satırı
    auto* idRow = new QHBoxLayout;
    idRow->addWidget(new QLabel("Node ID:", this));
    m_nodeIdLabel = new QLabel("—", this);
    m_nodeIdLabel->setStyleSheet("color:#666;");
    idRow->addWidget(m_nodeIdLabel);
    idRow->addStretch();
    root->addLayout(idRow);

    // Sembol önizleme
    auto* previewBox = new QGroupBox("Sembol", this);
    auto* previewLayout = new QHBoxLayout(previewBox);
    m_symbolPreview = new QLabel(this);
    m_symbolPreview->setFixedSize(80, 80);
    m_symbolPreview->setAlignment(Qt::AlignCenter);
    m_symbolPreview->setStyleSheet(
        "border:1px solid #ccc; background:#f8f8f8; border-radius:4px;");
    previewLayout->addStretch();
    previewLayout->addWidget(m_symbolPreview);
    previewLayout->addStretch();
    root->addWidget(previewBox);

    // Form alanları
    auto* formBox = new QGroupBox("Özellikler", this);
    auto* form = new QFormLayout(formBox);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems(s_fixtureTypes);
    form->addRow("Tip:", m_typeCombo);

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText("Ör: Lavabo-1");
    form->addRow("Etiket:", m_labelEdit);

    m_loadUnitSpin = new QDoubleSpinBox(this);
    m_loadUnitSpin->setRange(0.0, 9999.0);
    m_loadUnitSpin->setDecimals(1);
    m_loadUnitSpin->setSingleStep(0.5);
    m_loadUnitSpin->setToolTip("LU (besleme) veya DU (drenaj)");
    form->addRow("Yük Birimi (LU/DU):", m_loadUnitSpin);

    m_simultCheck = new QCheckBox("Eşzamanlı kullanım", this);
    form->addRow("", m_simultCheck);

    m_floorCombo = new QComboBox(this);
    m_floorCombo->addItem("— Belirsiz —");
    form->addRow("Kat:", m_floorCombo);

    root->addWidget(formBox);
    root->addStretch();

    // Uygula butonu
    m_applyBtn = new QPushButton("Uygula", this);
    m_applyBtn->setStyleSheet(
        "QPushButton { background:#1a6fbf; color:white; padding:5px 16px;"
        " border-radius:3px; font-weight:bold; }"
        "QPushButton:hover { background:#1558a0; }"
        "QPushButton:disabled { background:#aaa; }");
    root->addWidget(m_applyBtn);

    connect(m_applyBtn,  &QPushButton::clicked,
            this,        &FixturePropertiesPanel::OnApply);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,        &FixturePropertiesPanel::OnTypeChanged);
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════

void FixturePropertiesPanel::LoadNode(uint32_t nodeId,
                                       mep::NetworkGraph* graph,
                                       core::FloorManager* floors) {
    m_nodeId = nodeId;
    m_graph  = graph;

    const mep::Node* node = graph ? graph->GetNode(nodeId) : nullptr;
    if (!node) { Clear(); return; }

    m_nodeIdLabel->setText(QString::number(nodeId));

    // Tip
    int typeIdx = s_fixtureTypes.indexOf(
        QString::fromStdString(node->fixtureType));
    m_typeCombo->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);

    // Etiket
    m_labelEdit->setText(QString::fromStdString(node->label));

    // Yük birimi
    m_loadUnitSpin->setValue(node->loadUnit);

    // Eşzamanlı
    m_simultCheck->setChecked(node->isSimultaneous);

    // Kat listesini doldur
    PopulateFloors(floors);

    // Kat ataması
    if (floors) {
        int fi = floors->GetFloorOfNode(nodeId);
        if (fi != -999) {
            for (int i = 1; i < m_floorCombo->count(); ++i) {
                if (m_floorCombo->itemData(i).toInt() == fi) {
                    m_floorCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    m_applyBtn->setEnabled(true);
    UpdateSymbolPreview();
    setEnabled(true);
}

void FixturePropertiesPanel::Clear() {
    m_nodeId = 0;
    m_graph  = nullptr;
    m_nodeIdLabel->setText("—");
    m_labelEdit->clear();
    m_loadUnitSpin->setValue(0.0);
    m_simultCheck->setChecked(false);
    m_floorCombo->setCurrentIndex(0);
    m_symbolPreview->clear();
    m_symbolPreview->setText("?");
    m_applyBtn->setEnabled(false);
    setEnabled(false);
}

// ═══════════════════════════════════════════════════════════
//  SLOTS
// ═══════════════════════════════════════════════════════════

void FixturePropertiesPanel::OnApply() {
    if (!m_graph || m_nodeId == 0) return;

    mep::Node* node = m_graph->GetNode(m_nodeId);
    if (!node) return;

    node->fixtureType    = m_typeCombo->currentText().toStdString();
    node->label          = m_labelEdit->text().toStdString();
    node->loadUnit       = m_loadUnitSpin->value();
    node->isSimultaneous = m_simultCheck->isChecked();

    emit NodeUpdated(m_nodeId);
}

void FixturePropertiesPanel::OnTypeChanged(int /*index*/) {
    UpdateSymbolPreview();
}

void FixturePropertiesPanel::UpdateSymbolPreview() {
    QString typeName = m_typeCombo->currentText();
    auto symType = cad::FixtureSymbolLibrary::FromLabel(
        typeName.toStdString());

    auto& lib = cad::FixtureSymbolLibrary::Instance();
    auto geom = lib.GetSymbol(symType, 0.6, 50.0, 50.0);
    std::string svg = "<?xml version=\"1.0\"?>"
        "<svg xmlns=\"http://www.w3.org/2000/svg\""
        " width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
        "<rect width=\"100\" height=\"100\" fill=\"#f8f8f8\"/>"
        + cad::FixtureSymbolLibrary::ToSVG(geom, "#1a4f8a")
        + "</svg>";

    QPixmap px = SVGToPixmap(svg, 80);
    m_symbolPreview->setPixmap(px);
}

// ═══════════════════════════════════════════════════════════
//  PRIVATE HELPERS
// ═══════════════════════════════════════════════════════════

void FixturePropertiesPanel::PopulateFloors(core::FloorManager* floors) {
    m_floorCombo->clear();
    m_floorCombo->addItem("— Belirsiz —", -999);
    if (!floors) return;

    for (int i = 0; i < floors->GetFloorCount(); ++i) {
        const core::Floor* fl = floors->GetFloor(i);
        if (!fl) continue;
        QString label = QString::fromStdString(fl->label)
                        + QString(" (%1 m)").arg(fl->elevation_m, 0, 'f', 1);
        m_floorCombo->addItem(label, i);
    }
}

QPixmap FixturePropertiesPanel::SVGToPixmap(const std::string& svg, int size) {
    QByteArray data(svg.c_str(), static_cast<int>(svg.size()));
    QSvgRenderer renderer(data);

    QPixmap px(size, size);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    renderer.render(&painter);
    return px;
}

} // namespace ui
} // namespace vkt
