/**
 * @file ProjectManager.cpp
 * @brief Proje Çalışma Alanı Yöneticisi — Implementasyon
 */

#include "core/ProjectManager.hpp"

#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

namespace vkt {
namespace core {

ProjectManager& ProjectManager::Instance() {
    static ProjectManager instance;
    return instance;
}

void ProjectManager::SetProjectsRoot(const std::string& path) {
    m_projectsRoot = NormalizePath(path);
}

bool ProjectManager::CreateProject(const std::string& name, std::string& error) {
    if (m_projectsRoot.empty()) {
        error = "Proje kök dizini belirlenmedi. Lütfen önce 'Proje Kök Klasörü Ayarla' seçeneğini kullanın.";
        return false;
    }

    std::string safeName = SanitizeName(name);
    if (safeName.empty()) {
        error = "Geçersiz proje adı.";
        return false;
    }

    fs::path projectPath = fs::path(m_projectsRoot) / safeName;

    if (fs::exists(projectPath)) {
        error = "Bu isimde bir proje zaten mevcut: " + safeName;
        return false;
    }

    try {
        fs::create_directories(projectPath);
        m_projectFolder = NormalizePath(projectPath.string());
        m_projectName   = safeName;
        if (!CreateSubFolders()) {
            error = "Alt klasörler oluşturulamadı.";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        error = std::string("Klasör oluşturma hatası: ") + e.what();
        return false;
    }
}

void ProjectManager::SetActiveProject(const std::string& projectFolder) {
    m_projectFolder = NormalizePath(projectFolder);
    // Proje adını klasör adından çıkar
    fs::path p(projectFolder);
    m_projectName = p.filename().string();
}

void ProjectManager::ClearActiveProject() {
    m_projectFolder.clear();
    m_projectName.clear();
}

std::string ProjectManager::GetMainFilePath() const {
    if (m_projectFolder.empty() || m_projectName.empty()) return {};
    fs::path p = fs::path(m_projectFolder) / (m_projectName + ".vkt");
    return NormalizePath(p.string());
}

std::string ProjectManager::GetMimariFolder() const {
    if (m_projectFolder.empty()) return {};
    return NormalizePath((fs::path(m_projectFolder) / "mimari").string());
}

std::string ProjectManager::GetCiktiFolder() const {
    if (m_projectFolder.empty()) return {};
    return NormalizePath((fs::path(m_projectFolder) / "cikti").string());
}

std::string ProjectManager::GetRaporFolder() const {
    if (m_projectFolder.empty()) return {};
    return NormalizePath((fs::path(m_projectFolder) / "rapor").string());
}

std::vector<std::string> ProjectManager::ListProjects() const {
    std::vector<std::string> names;
    if (m_projectsRoot.empty()) return names;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_projectsRoot, ec)) {
        if (entry.is_directory(ec))
            names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool ProjectManager::CreateSubFolders() {
    if (m_projectFolder.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(m_projectFolder) / "mimari", ec);
    fs::create_directories(fs::path(m_projectFolder) / "cikti",  ec);
    fs::create_directories(fs::path(m_projectFolder) / "rapor",  ec);
    return !ec;
}

std::string ProjectManager::SanitizeName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (unsigned char c : name) {
        if (c == ' ' || c == '_' || c == '-' || std::isalnum(c) ||
            // Türkçe karakterlere izin ver (UTF-8 çok baytlı)
            c > 127) {
            result += static_cast<char>(c);
        }
    }
    // Baş/son boşlukları temizle
    while (!result.empty() && result.front() == ' ') result.erase(result.begin());
    while (!result.empty() && result.back()  == ' ') result.pop_back();
    return result;
}

std::string ProjectManager::NormalizePath(const std::string& path) {
    std::string result = path;
    for (char& c : result) {
        if (c == '\\') c = '/';
    }
    return result;
}

} // namespace core
} // namespace vkt
