#pragma once

#include <QObject>
#include <QVariant>
#include <QWidget>

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstdio>

namespace pae::protocol_lab_ui::ascii_smoke_diagnostic {

inline std::atomic<bool> enabled{false};
inline std::atomic<bool> editor_lost{false};
inline std::atomic<std::uint64_t> sequence{0U};
inline std::atomic<std::uint64_t> next_editor_id{1U};

inline bool Enabled() noexcept { return enabled.load(std::memory_order_relaxed); }

inline void Trace(const char* stage, const void* object = nullptr,
                  std::uint64_t editor_id = 0U) noexcept {
  if (!Enabled()) return;
  const auto next = sequence.fetch_add(1U, std::memory_order_relaxed) + 1U;
  std::fprintf(stderr, "ASCII_SMOKE_DIAG seq=%" PRIu64 " stage=%s editor=%" PRIu64
                       " object=%p\n", next, stage, editor_id, object);
  std::fflush(stderr);
}

inline void Begin() noexcept {
  sequence.store(0U, std::memory_order_relaxed);
  editor_lost.store(false, std::memory_order_relaxed);
  enabled.store(true, std::memory_order_relaxed);
  Trace("smoke_begin");
}

inline std::uint64_t EditorId(const QObject* editor) noexcept {
  return editor ? editor->property("paeAsciiSmokeEditorId").toULongLong() : 0U;
}

inline void RegisterEditor(QWidget* editor) {
  if (!Enabled() || !editor) return;
  const auto id = next_editor_id.fetch_add(1U, std::memory_order_relaxed);
  editor->setProperty("paeAsciiSmokeEditorId", QVariant::fromValue<qulonglong>(id));
  Trace("editor_create", editor, id);
  QObject::connect(editor, &QObject::destroyed, [id](QObject* destroyed) {
    Trace("editor_destroy", destroyed, id);
  });
}

inline void MarkEditorLost(const char* stage, std::uint64_t id) noexcept {
  editor_lost.store(true, std::memory_order_relaxed);
  Trace(stage, nullptr, id);
}

inline bool EditorLost() noexcept {
  return editor_lost.load(std::memory_order_relaxed);
}

}  // namespace pae::protocol_lab_ui::ascii_smoke_diagnostic
