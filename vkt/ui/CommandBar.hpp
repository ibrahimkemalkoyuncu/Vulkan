/**
 * @file CommandBar.hpp
 * @brief AutoCAD benzeri komut satırı widget'ı
 *
 * Alt barda görünür. Kullanıcı komut yazar (LINE, PIPE, FIXTURE, ZOOM, ...),
 * Enter'a basınca sinyal yayılır. Geçmiş komutlar yukarı ok ile erişilir.
 * Tab ile otomatik tamamlama desteklenir.
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QStringList>
#include <QStringListModel>
#include <QCompleter>

namespace vkt {
namespace ui {

class CommandBar : public QWidget {
    Q_OBJECT

public:
    explicit CommandBar(QWidget* parent = nullptr);

    /** @brief Komut satırına mesaj yaz (ör. "Nokta belirtin:") */
    void SetPrompt(const std::string& prompt);

    /** @brief Komut geçmişini temizle */
    void ClearHistory();

    /** @brief Kullanılabilir komutları ayarla (otomatik tamamlama için) */
    void SetKnownCommands(const QStringList& commands);

    /** @brief Komut satırına odaklan */
    void FocusInput();

signals:
    /** @brief Kullanıcı bir komut girdi */
    void CommandEntered(const QString& command);

    /** @brief Kullanıcı bir sayı girdi (koordinat / mesafe bekleniyor) */
    void NumberEntered(double value);

    /** @brief Escape tuşu basıldı (komut iptal) */
    void EscapePressed();

private slots:
    void OnReturnPressed();
    void OnTextChanged(const QString& text);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void AppendToHistory(const QString& line, bool isCommand = false);

    QListWidget*  m_historyList = nullptr;
    QLineEdit*    m_input       = nullptr;
    QLabel*       m_promptLabel = nullptr;
    QCompleter*   m_completer   = nullptr;

    QStringList   m_history;
    int           m_historyPos = -1;

    static const QStringList s_builtinCommands;
};

} // namespace ui
} // namespace vkt
