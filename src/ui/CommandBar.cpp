/**
 * @file CommandBar.cpp
 * @brief Komut satırı widget implementasyonu
 */

#include "ui/CommandBar.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QFont>
#include <QPalette>
#include <QScrollBar>
#include <QStringList>

namespace vkt {
namespace ui {

const QStringList CommandBar::s_builtinCommands = {
    // Çizim
    "LINE", "POLYLINE", "CIRCLE", "ARC", "TEXT",
    // MEP
    "PIPE", "FIXTURE", "JUNCTION", "SOURCE", "DRAIN",
    "VALVE", "PUMP", "METER",
    // Düzenleme
    "MOVE", "COPY", "DELETE", "ROTATE", "SCALE", "MIRROR",
    "TRIM", "EXTEND", "OFFSET",
    // Görünüm
    "ZOOM", "ZOOM-EXTENTS", "ZOOM-IN", "ZOOM-OUT",
    "PAN", "VIEW-PLAN", "VIEW-ISO",
    // Analiz
    "HYDRAULICS", "DRAINAGE", "SCHEDULE", "RISER",
    // Dosya
    "NEW", "OPEN", "SAVE", "SAVE-AS",
    "EXPORT-DXF", "EXPORT-PDF", "EXPORT-EXCEL",
    // Diğer
    "UNDO", "REDO", "HELP",
};

// ═══════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════

CommandBar::CommandBar(QWidget* parent) : QWidget(parent) {
    setMaximumHeight(140);
    setMinimumHeight(80);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Geçmiş listesi
    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(90);
    m_historyList->setStyleSheet(
        "QListWidget { background:#1e1e1e; color:#d4d4d4;"
        " border:none; font-family:'Consolas','Courier New',monospace;"
        " font-size:11px; }"
        "QListWidget::item { padding:1px 4px; }");
    m_historyList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_historyList);

    // Giriş satırı
    auto* inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(0);

    m_promptLabel = new QLabel("Komut:", this);
    m_promptLabel->setFixedWidth(72);
    m_promptLabel->setStyleSheet(
        "background:#2d2d2d; color:#569cd6; padding:3px 6px;"
        " font-family:'Consolas','Courier New',monospace; font-size:11px;");
    inputRow->addWidget(m_promptLabel);

    m_input = new QLineEdit(this);
    m_input->setStyleSheet(
        "QLineEdit { background:#252526; color:#dcdcdc; border:none;"
        " padding:3px 6px;"
        " font-family:'Consolas','Courier New',monospace; font-size:11px; }"
        "QLineEdit:focus { border-left:2px solid #569cd6; }");
    m_input->setPlaceholderText("komut yaz veya Enter...");
    inputRow->addWidget(m_input);
    layout->addLayout(inputRow);

    // Otomatik tamamlama
    QStringList allCommands = s_builtinCommands;
    auto* model = new QStringListModel(allCommands, this);
    m_completer = new QCompleter(model, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_input->setCompleter(m_completer);

    // Event filter for history navigation
    m_input->installEventFilter(this);

    connect(m_input, &QLineEdit::returnPressed,
            this,    &CommandBar::OnReturnPressed);
    connect(m_input, &QLineEdit::textChanged,
            this,    &CommandBar::OnTextChanged);

    AppendToHistory("VKT Komut Satırı hazır. Komut için buraya yazın.", false);
    AppendToHistory("Yardım: HELP | İptal: ESC", false);
}

// ═══════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════

void CommandBar::SetPrompt(const std::string& prompt) {
    m_promptLabel->setText(QString::fromStdString(prompt) + ":");
}

void CommandBar::ClearHistory() {
    m_historyList->clear();
}

void CommandBar::SetKnownCommands(const QStringList& commands) {
    QStringList all = s_builtinCommands + commands;
    auto* model = new QStringListModel(all, this);
    m_completer->setModel(model);
}

void CommandBar::FocusInput() {
    m_input->setFocus();
}

// ═══════════════════════════════════════════════════════════
//  SLOTS
// ═══════════════════════════════════════════════════════════

void CommandBar::OnReturnPressed() {
    QString text = m_input->text().trimmed().toUpper();
    if (text.isEmpty()) return;

    // Geçmişe ekle
    m_history.prepend(text);
    m_historyPos = -1;
    AppendToHistory("> " + text, true);

    m_input->clear();

    // Sayı mı komutu mu?
    bool ok;
    double num = text.toDouble(&ok);
    if (ok) {
        emit NumberEntered(num);
    } else {
        emit CommandEntered(text);
    }
}

void CommandBar::OnTextChanged(const QString& /*text*/) {
    // Anlık geri bildirim (ileride koordinat parse'a genişletilebilir)
}

// ═══════════════════════════════════════════════════════════
//  EVENT FILTER — geçmiş navigasyonu + ESC
// ═══════════════════════════════════════════════════════════

bool CommandBar::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_Escape) {
            m_input->clear();
            m_historyPos = -1;
            AppendToHistory("* İptal *", false);
            emit EscapePressed();
            return true;
        }

        if (ke->key() == Qt::Key_Up) {
            if (m_historyPos + 1 < m_history.size()) {
                ++m_historyPos;
                m_input->setText(m_history[m_historyPos]);
            }
            return true;
        }

        if (ke->key() == Qt::Key_Down) {
            if (m_historyPos > 0) {
                --m_historyPos;
                m_input->setText(m_history[m_historyPos]);
            } else if (m_historyPos == 0) {
                m_historyPos = -1;
                m_input->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ═══════════════════════════════════════════════════════════
//  PRIVATE HELPERS
// ═══════════════════════════════════════════════════════════

void CommandBar::AppendToHistory(const QString& line, bool isCommand) {
    auto* item = new QListWidgetItem(line, m_historyList);
    if (isCommand) {
        item->setForeground(QColor("#4ec9b0")); // komutlar teal renk
    } else {
        item->setForeground(QColor("#9cdcfe")); // mesajlar açık mavi
    }
    m_historyList->scrollToBottom();
}

} // namespace ui
} // namespace vkt
