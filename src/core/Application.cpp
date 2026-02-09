/**
 * @file Application.cpp
 * @brief VKT Ana Uygulama İmplementasyonu
 */

#include "core/Application.hpp"
#include <stdexcept>
#include <iostream>

namespace vkt {
namespace core {

Application& Application::Instance() {
    static Application instance;
    return instance;
}

void Application::Initialize() {
    if (m_running) {
        throw std::runtime_error("Application already initialized");
    }
    
    std::cout << "VKT Application başlatılıyor..." << std::endl;
    m_running = true;
}

void Application::Run() {
    // Qt event loop tarafından yönetilir
}

void Application::Shutdown() {
    std::cout << "VKT Application kapatılıyor..." << std::endl;
    
    // Tüm pencereleri kapat
    for (auto* window : m_windows) {
        delete window;
    }
    m_windows.clear();
    
    // Belgeleri kapat
    m_documents.clear();
    m_activeDocument = nullptr;
    
    m_running = false;
}

Document* Application::CreateNewDocument() {
    auto doc = std::make_unique<Document>();
    Document* ptr = doc.get();
    m_documents.push_back(std::move(doc));
    m_activeDocument = ptr;
    
    std::cout << "Yeni belge oluşturuldu." << std::endl;
    return ptr;
}

Document* Application::OpenDocument(const std::string& path) {
    auto doc = std::make_unique<Document>();
    if (!doc->Load(path)) {
        std::cerr << "Belge yüklenemedi: " << path << std::endl;
        return nullptr;
    }
    
    Document* ptr = doc.get();
    m_documents.push_back(std::move(doc));
    m_activeDocument = ptr;
    
    std::cout << "Belge yüklendi: " << path << std::endl;
    return ptr;
}

void Application::CloseDocument(Document* doc) {
    auto it = std::find_if(m_documents.begin(), m_documents.end(),
        [doc](const auto& d) { return d.get() == doc; });
    
    if (it != m_documents.end()) {
        if (m_activeDocument == doc) {
            m_activeDocument = nullptr;
        }
        m_documents.erase(it);
        std::cout << "Belge kapatıldı." << std::endl;
    }
}

Window* Application::CreateWindow() {
    Window* window = new Window();
    m_windows.push_back(window);
    return window;
}

} // namespace core
} // namespace vkt
