/**
 * @file Command.hpp
 * @brief Komut Deseni (Command Pattern) ve Geri Al/İleri Al Mantığı.
 * 
 * Bu dosya, CAD yazılımındaki tüm kullanıcı eylemlerini (boru çizme, silme vb.)
 * nesnelleştirerek geri alınabilir (Undo) ve tekrar uygulanabilir (Redo) hale getirir.
 */
#pragma once
#include <vector>
#include <memory>
#include <stack>

namespace vkt {
namespace core {

/**
 * @class Command
 * @brief Tüm kullanıcı eylemleri için temel soyut sınıf.
 */
class Command {
public:
    virtual ~Command() = default;
    
    /** @brief Komutu yürütür. */
    virtual void Execute() = 0;
    
    /** @brief Yapılan işlemi geri alır. */
    virtual void Undo() = 0;
};

/**
 * @class CommandManager
 * @brief İşlem geçmişini (Undo/Redo stack) yöneten merkezi birim.
 */
class CommandManager {
public:
    /** 
     * @brief Yeni bir komutu çalıştırır ve geçmişe ekler. 
     * @param cmd Çalıştırılacak komut nesnesi.
     */
    void Execute(std::unique_ptr<Command> cmd) {
        if (!cmd) return;
        cmd->Execute();
        m_undoStack.push(std::move(cmd));
        
        // Yeni bir işlem yapıldığında ileri al (redo) geçmişi temizlenir.
        while(!m_redoStack.empty()) m_redoStack.pop();
    }

    /** @brief Son yapılan işlemi geri alır. */
    void Undo() {
        if (m_undoStack.empty()) return;
        
        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();
        cmd->Undo();
        m_redoStack.push(std::move(cmd));
    }

    /** @brief Geri alınan işlemi tekrar uygular. */
    void Redo() {
        if (m_redoStack.empty()) return;
        
        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();
        cmd->Execute();
        m_undoStack.push(std::move(cmd));
    }

    /** @brief İşlem geçmişini tamamen temizler. */
    void Clear() {
        while(!m_undoStack.empty()) m_undoStack.pop();
        while(!m_redoStack.empty()) m_redoStack.pop();
    }

private:
    std::stack<std::unique_ptr<Command>> m_undoStack; ///< Geri alma yığını
    std::stack<std::unique_ptr<Command>> m_redoStack; ///< İleri alma yığını
};

} // namespace core
} // namespace vkt
