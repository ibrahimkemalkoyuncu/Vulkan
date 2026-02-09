/**
 * @file Window.cpp
 * @brief Ana Pencere İmplementasyonu
 */

#include "core/Window.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <iostream>

namespace vkt {
namespace core {

Window::Window(QWidget* parent)
    : QMainWindow(parent) {
    
    setWindowTitle("VKT - Mekanik Tesisat Draw");
    resize(1280, 720);
    
    SetupUI();
    CreateActions();
    CreateMenus();
    CreateToolbars();
    
    statusBar()->showMessage("Hazır");
}

Window::~Window() {
    std::cout << "Window kapatılıyor..." << std::endl;
}

void Window::SetDocument(Document* doc) {
    m_document = doc;
    if (m_renderer) {
        // Renderer'ı güncelle
    }
    std::cout << "Document pencereye atandı." << std::endl;
}

void Window::closeEvent(QCloseEvent* event) {
    if (m_document && m_document->IsModified()) {
        auto reply = QMessageBox::question(this, 
            "Kaydetmeden Çık",
            "Değişiklikler kaydedilmedi. Çıkılsın mı?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void Window::SetupUI() {
    // Renderer oluştur
    m_renderer = std::make_unique<render::VulkanRenderer>();
    
    // UI oluştur
    m_ui = std::make_unique<ui::UI>(this);
    m_ui->Initialize();
}

void Window::CreateActions() {
    // İşlemler sonra eklenecek
}

void Window::CreateMenus() {
    auto* fileMenu = menuBar()->addMenu("&Dosya");
    auto* editMenu = menuBar()->addMenu("&Düzen");
    auto* viewMenu = menuBar()->addMenu("&Görünüm");
    auto* analyzeMenu = menuBar()->addMenu("&Analiz");
    auto* helpMenu = menuBar()->addMenu("&Yardım");
}

void Window::CreateToolbars() {
    auto* drawToolbar = addToolBar("Çizim");
    auto* editToolbar = addToolBar("Düzenleme");
    auto* viewToolbar = addToolBar("Görünüm");
}

void Window::OnNew() {
    std::cout << "Yeni proje..." << std::endl;
}

void Window::OnOpen() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Proje Aç", "", "VKT Projeler (*.vkt);;Tüm Dosyalar (*)");
    if (!fileName.isEmpty()) {
        std::cout << "Dosya açılıyor: " << fileName.toStdString() << std::endl;
    }
}

void Window::OnSave() {
    if (m_document) {
        std::cout << "Kaydet..." << std::endl;
    }
}

void Window::OnSaveAs() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Farklı Kaydet", "", "VKT Projeler (*.vkt)");
    if (!fileName.isEmpty()) {
        std::cout << "Farklı kaydet: " << fileName.toStdString() << std::endl;
    }
}

void Window::OnUndo() {
    if (m_document && m_document->CanUndo()) {
        m_document->Undo();
    }
}

void Window::OnRedo() {
    if (m_document && m_document->CanRedo()) {
        m_document->Redo();
    }
}

} // namespace core
} // namespace vkt
