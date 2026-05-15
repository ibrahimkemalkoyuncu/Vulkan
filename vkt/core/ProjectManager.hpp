/**
 * @file ProjectManager.hpp
 * @brief VKT Proje Çalışma Alanı Yöneticisi
 *
 * FineSANI'nin "CC klasörü" konseptinin VKT karşılığı.
 *
 * Her proje bir alt klasör olarak merkezi bir kök dizine (projects root)
 * kaydedilir. Alt klasör yapısı:
 *
 *   [ProjeKökü]/
 *   └── [ProjeAdı]/
 *       ├── [ProjeAdı].vkt    ← ana proje dosyası
 *       ├── mimari/           ← DXF/DWG altlık kopyaları
 *       ├── cikti/            ← dışa aktarılan DXF/blok dosyaları
 *       └── rapor/            ← hesap raporları (XLS/PDF)
 */

#pragma once

#include <string>
#include <vector>

namespace vkt {
namespace core {

/**
 * @class ProjectManager
 * @brief Proje çalışma alanı yöneticisi (Singleton)
 *
 * Kullanım:
 *   auto& pm = ProjectManager::Instance();
 *   pm.SetProjectsRoot("C:/Projeler");   // kalıcı olarak QSettings'te tutulur
 *   pm.CreateProject("Otel_Renovasyon"); // klasör + alt dizinler oluşturulur
 *   doc->Save(pm.GetMainFilePath());     // ana .vkt dosyasına kaydet
 */
class ProjectManager {
public:
    static ProjectManager& Instance();

    // ── Proje kök dizini (CC klasörü eşdeğeri) ─────────────────
    void SetProjectsRoot(const std::string& path);
    const std::string& GetProjectsRoot() const { return m_projectsRoot; }
    bool HasProjectsRoot() const { return !m_projectsRoot.empty(); }

    // ── Yeni proje oluştur ──────────────────────────────────────
    /**
     * @brief Kök dizin altında yeni proje klasörü oluştur.
     * @param name  Proje adı (klasör adı olarak kullanılır; özel karakterler temizlenir)
     * @param error Hata mesajı (başarısız ise dolu gelir)
     * @return Başarılı ise true; klasör zaten varsa veya kök belirsiz ise false
     */
    bool CreateProject(const std::string& name, std::string& error);

    // ── Var olan projeyi aç ────────────────────────────────────
    /**
     * @brief Var olan bir proje klasörünü aktif proje olarak ayarla.
     * @param projectFolder  Tam klasör yolu
     */
    void SetActiveProject(const std::string& projectFolder);

    // ── Aktif proje bilgileri ──────────────────────────────────
    bool HasActiveProject() const { return !m_projectFolder.empty(); }
    const std::string& GetProjectFolder() const { return m_projectFolder; }
    const std::string& GetProjectName()   const { return m_projectName; }

    std::string GetMainFilePath()  const; ///< [klasör]/[ad].vkt
    std::string GetMimariFolder()  const; ///< [klasör]/mimari/
    std::string GetCiktiFolder()   const; ///< [klasör]/cikti/
    std::string GetRaporFolder()   const; ///< [klasör]/rapor/

    void ClearActiveProject();

    // ── Proje listesi ──────────────────────────────────────────
    /// Kök dizindeki tüm proje klasörlerini adlarıyla döndür
    std::vector<std::string> ListProjects() const;

    // ── Yardımcı ───────────────────────────────────────────────
    /// Klasör adı olarak kullanılabilecek temiz bir isim üret
    static std::string SanitizeName(const std::string& name);

    /// Yolu forward-slash ile normalize et
    static std::string NormalizePath(const std::string& path);

private:
    ProjectManager() = default;
    ProjectManager(const ProjectManager&) = delete;
    ProjectManager& operator=(const ProjectManager&) = delete;

    bool CreateSubFolders();

    std::string m_projectsRoot;
    std::string m_projectFolder;
    std::string m_projectName;
};

} // namespace core
} // namespace vkt
