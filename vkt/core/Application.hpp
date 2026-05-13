/**
 * @file Application.hpp
 * @brief VKT Ana Uygulama Sınıfı
 * 
 * Uygulama yaşam döngüsünü yöneten merkezi controller.
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Document.hpp"

namespace vkt {
namespace core {

/**
 * @class Application
 * @brief Singleton uygulama yöneticisi
 * 
 * Tüm pencereleri, belgeleri ve global durumu yönetir.
 */
class Application {
public:
    static Application& Instance();

    void Initialize();
    void Run();
    void Shutdown();

    // Document yönetimi
    Document* CreateNewDocument();
    Document* OpenDocument(const std::string& path);
    void CloseDocument(Document* doc);
    
    Document* GetActiveDocument() { return m_activeDocument; }
    void SetActiveDocument(Document* doc) { m_activeDocument = doc; }

private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    std::vector<std::unique_ptr<Document>> m_documents;
    Document* m_activeDocument = nullptr;
    bool m_running = false;
};

} // namespace core
} // namespace vkt
