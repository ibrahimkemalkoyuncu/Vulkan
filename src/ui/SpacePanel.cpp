/**
 * @file SpacePanel.cpp
 * @brief Mahal Yönetim Paneli implementasyonu
 */

#include "ui/SpacePanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>

namespace vkt {
namespace ui {

SpacePanel::SpacePanel(QWidget* parent)
    : QDockWidget("Mahal Listesi", parent)
{
    SetupUI();
}

void SpacePanel::SetupUI() {
    auto* centralWidget = new QWidget(this);
    setWidget(centralWidget);
    
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    
    // Arama ve filtreleme
    auto* filterGroup = new QGroupBox("Filtre");
    auto* filterLayout = new QVBoxLayout(filterGroup);
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Mahal adı ara...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SpacePanel::OnFilterChanged);
    filterLayout->addWidget(m_searchEdit);
    
    m_typeFilterCombo = new QComboBox();
    m_typeFilterCombo->addItem("Tüm Tipler", -1);
    connect(m_typeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpacePanel::OnTypeFilterChanged);
    filterLayout->addWidget(m_typeFilterCombo);
    
    mainLayout->addWidget(filterGroup);
    
    // Mahal listesi (Tree view)
    m_spaceTree = new QTreeWidget();
    m_spaceTree->setHeaderLabels({"Mahal", "Tip", "Alan (m²)", "Hacim (m³)", "Numara"});
    m_spaceTree->setColumnWidth(0, 150);
    m_spaceTree->setColumnWidth(1, 120);
    m_spaceTree->setColumnWidth(2, 80);
    m_spaceTree->setColumnWidth(3, 80);
    m_spaceTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_spaceTree->setAlternatingRowColors(true);
    
    connect(m_spaceTree, &QTreeWidget::itemSelectionChanged, this, &SpacePanel::OnSpaceSelected);
    connect(m_spaceTree, &QTreeWidget::itemDoubleClicked, this, &SpacePanel::OnSpaceDoubleClicked);
    
    mainLayout->addWidget(m_spaceTree);
    
    // İstatistikler
    auto* statsGroup = new QGroupBox("İstatistikler");
    auto* statsLayout = new QVBoxLayout(statsGroup);
    
    m_totalCountLabel = new QLabel("Toplam: 0 mahal");
    m_totalAreaLabel = new QLabel("Alan: 0.00 m²");
    m_totalVolumeLabel = new QLabel("Hacim: 0.00 m³");
    
    statsLayout->addWidget(m_totalCountLabel);
    statsLayout->addWidget(m_totalAreaLabel);
    statsLayout->addWidget(m_totalVolumeLabel);
    
    mainLayout->addWidget(statsGroup);
    
    // Butonlar
    auto* buttonLayout = new QHBoxLayout();
    
    m_refreshButton = new QPushButton("Yenile");
    connect(m_refreshButton, &QPushButton::clicked, this, &SpacePanel::OnRefresh);
    
    m_deleteButton = new QPushButton("Sil");
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &SpacePanel::OnDeleteSpace);
    
    m_statsButton = new QPushButton("Detaylı İstatistikler");
    connect(m_statsButton, &QPushButton::clicked, this, &SpacePanel::OnShowStatistics);
    
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_statsButton);
    
    mainLayout->addLayout(buttonLayout);
    
    PopulateTypeFilter();
}

void SpacePanel::SetSpaceManager(cad::SpaceManager* manager) {
    m_spaceManager = manager;
    RefreshList();
}

void SpacePanel::RefreshList() {
    m_spaceTree->clear();
    
    if (!m_spaceManager) {
        return;
    }
    
    // Filtre ayarları
    QString searchText = m_searchEdit->text().toLower();
    int typeFilter = m_typeFilterCombo->currentData().toInt();
    
    auto spaces = m_spaceManager->GetAllSpaces();
    
    for (auto* space : spaces) {
        // İsim filtresi
        if (!searchText.isEmpty()) {
            QString spaceName = QString::fromStdString(space->GetName()).toLower();
            if (!spaceName.contains(searchText)) {
                continue;
            }
        }
        
        // Tip filtresi
        if (typeFilter >= 0) {
            if (static_cast<int>(space->GetSpaceType()) != typeFilter) {
                continue;
            }
        }
        
        // Tree item oluştur
        auto* item = new QTreeWidgetItem(m_spaceTree);
        
        item->setText(0, QString::fromStdString(space->GetName()));
        item->setText(1, QString::fromStdString(cad::Space::SpaceTypeToString(space->GetSpaceType())));
        item->setText(2, QString::number(space->GetArea(), 'f', 2));
        item->setText(3, QString::number(space->GetVolume(), 'f', 2));
        item->setText(4, QString::fromStdString(space->GetNumber()));
        
        // ID'yi sakla
        item->setData(0, Qt::UserRole, static_cast<qulonglong>(space->GetId()));
        
        // Geçersiz ise kırmızı yap
        if (!space->IsValid()) {
            item->setForeground(0, QColor(200, 0, 0));
            item->setToolTip(0, "Geçersiz mahal!");
        }
    }
    
    UpdateStatistics();
}

void SpacePanel::UpdateStatistics() {
    if (!m_spaceManager) {
        m_totalCountLabel->setText("Toplam: 0 mahal");
        m_totalAreaLabel->setText("Alan: 0.00 m²");
        m_totalVolumeLabel->setText("Hacim: 0.00 m³");
        return;
    }
    
    auto stats = m_spaceManager->GetStatistics();
    
    m_totalCountLabel->setText(QString("Toplam: %1 mahal").arg(stats.totalSpaces));
    m_totalAreaLabel->setText(QString("Alan: %1 m²").arg(stats.totalArea, 0, 'f', 2));
    m_totalVolumeLabel->setText(QString("Hacim: %1 m³").arg(stats.totalVolume, 0, 'f', 2));
}

void SpacePanel::PopulateTypeFilter() {
    m_typeFilterCombo->clear();
    m_typeFilterCombo->addItem("Tüm Tipler", -1);
    
    // Tüm SpaceType'ları ekle
    m_typeFilterCombo->addItem("Yatak Odası", static_cast<int>(cad::SpaceType::Bedroom));
    m_typeFilterCombo->addItem("Banyo", static_cast<int>(cad::SpaceType::Bathroom));
    m_typeFilterCombo->addItem("WC", static_cast<int>(cad::SpaceType::WC));
    m_typeFilterCombo->addItem("Mutfak", static_cast<int>(cad::SpaceType::Kitchen));
    m_typeFilterCombo->addItem("Salon", static_cast<int>(cad::SpaceType::LivingRoom));
    m_typeFilterCombo->addItem("Koridor", static_cast<int>(cad::SpaceType::Corridor));
    m_typeFilterCombo->addItem("Balkon", static_cast<int>(cad::SpaceType::Balcony));
    m_typeFilterCombo->addItem("Ofis", static_cast<int>(cad::SpaceType::Office));
    m_typeFilterCombo->addItem("Depo", static_cast<int>(cad::SpaceType::Storage));
    m_typeFilterCombo->addItem("Çamaşır Odası", static_cast<int>(cad::SpaceType::LaundryRoom));
    m_typeFilterCombo->addItem("Garaj", static_cast<int>(cad::SpaceType::Garage));
    m_typeFilterCombo->addItem("Mekanik Oda", static_cast<int>(cad::SpaceType::MechanicalRoom));
    m_typeFilterCombo->addItem("Kazan Dairesi", static_cast<int>(cad::SpaceType::BoilerRoom));
}

void SpacePanel::OnSpaceSelected() {
    bool hasSelection = !m_spaceTree->selectedItems().isEmpty();
    m_deleteButton->setEnabled(hasSelection);
    
    if (hasSelection) {
        auto* item = m_spaceTree->selectedItems().first();
        unsigned long long spaceId = item->data(0, Qt::UserRole).toULongLong();
        emit SpaceSelected(spaceId);
    }
}

void SpacePanel::OnSpaceDoubleClicked(QTreeWidgetItem* item, int column) {
    if (item) {
        unsigned long long spaceId = item->data(0, Qt::UserRole).toULongLong();
        emit SpaceDoubleClicked(spaceId);
    }
}

void SpacePanel::OnFilterChanged(const QString& text) {
    RefreshList();
}

void SpacePanel::OnTypeFilterChanged(int index) {
    RefreshList();
}

void SpacePanel::OnDeleteSpace() {
    auto selectedItems = m_spaceTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    auto reply = QMessageBox::question(this, "Mahal Sil", 
        QString("%1 mahal silinecek. Emin misiniz?").arg(selectedItems.size()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        for (auto* item : selectedItems) {
            unsigned long long spaceId = item->data(0, Qt::UserRole).toULongLong();
            emit DeleteSpaceRequested(spaceId);
        }
        RefreshList();
    }
}

void SpacePanel::OnRefresh() {
    RefreshList();
}

void SpacePanel::OnShowStatistics() {
    if (!m_spaceManager) {
        return;
    }
    
    auto stats = m_spaceManager->GetStatistics();
    
    QString statsText = QString::fromStdString(stats.ToString());
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Mahal İstatistikleri");
    msgBox.setText(statsText);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

cad::EntityId SpacePanel::GetSelectedSpaceId() const {
    auto selectedItems = m_spaceTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return 0;
    }
    
    return selectedItems.first()->data(0, Qt::UserRole).toULongLong();
}

} // namespace ui
} // namespace vkt
