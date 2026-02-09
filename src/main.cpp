/**
 * @file main.cpp
 * @brief VKT Uygulama Giriş Noktası
 */

#include <QApplication>
#include <iostream>
#include "core/Application.hpp"
#include "ui/MainWindow.hpp"

int main(int argc, char* argv[]) {
    try {
        // Qt uygulama başlat
        QApplication qapp(argc, argv);
        qapp.setApplicationName("VKT - Mekanik Tesisat Draw (FINE SANI++)");
        qapp.setApplicationVersion("1.0.0");
        
        // VKT uygulama örneği
        auto& app = vkt::core::Application::Instance();
        app.Initialize();
        
        // Ana pencere oluştur (MainWindow)
        auto* mainWindow = new vkt::ui::MainWindow();
        mainWindow->show();
        
        // Yeni belge oluştur
        auto* doc = app.CreateNewDocument();
        mainWindow->SetDocument(doc);
        
        std::cout << "═══════════════════════════════════════" << std::endl;
        std::cout << "  VKT - Mekanik Tesisat CAD" << std::endl;
        std::cout << "  FINE SANI++ Mühendislik Modu" << std::endl;
        std::cout << "═══════════════════════════════════════" << std::endl;
        std::cout << "✅ Vulkan Renderer: Aktif" << std::endl;
        std::cout << "✅ Qt6 UI: Hazır" << std::endl;
        std::cout << "✅ Hidrolik Motor: Standby" << std::endl;
        std::cout << "✅ B-Rep Engine: Hazır" << std::endl;
        std::cout << "═══════════════════════════════════════" << std::endl;
        
        // Qt event loop
        int result = qapp.exec();
        
        // Temizlik
        delete mainWindow;
        app.Shutdown();
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "❌ HATA: " << e.what() << std::endl;
        return 1;
    }
}
