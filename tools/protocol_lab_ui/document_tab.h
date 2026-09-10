#pragma once

#include <QWidget>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "compile_worker.h"
#include "document_session.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class QTextBrowser;

namespace pae::protocol_lab_ui {

class ExactValueDelegate;
class FieldTableModel;
class HexView;

struct EncodeTimingSnapshot {
  qint64 exact_input_ns = 0;
  qint64 main_codec_ns = 0;
  qint64 review_decode_ns = 0;
  qint64 result_mapping_ns = 0;
  qint64 hex_replace_ns = 0;
  qint64 first_repaint_total_ns = 0;
};

class DocumentTab final : public QWidget {
 public:
  DocumentTab(DocumentId document_id, CompileWorker& worker, QWidget* parent = nullptr);
  ~DocumentTab() override;

  DocumentId document_id() const noexcept { return session_.id(); }
  DocumentState state() const noexcept { return session_.state(); }
  QString ConfigPath() const;
  QString Title() const;

  void LoadPath(const QString& path);
  void AcceptCompletion(std::unique_ptr<CompileCompletion> completion);
  void CloseDocument();

  bool PopulateCanonicalDraftsForSmoke(QString& error);
  bool EncodeForSmoke(QString& error);
  bool InspectTextForSmoke(const QString& text, QString& error);
  bool VerifyInspectFailureForSmoke(const QString& text, InspectFailureStage stage,
                                    const QString& status, std::optional<std::size_t> input_offset,
                                    const QString& failed_field_id, QString& error);
  QString InspectMatchedMessageForSmoke() const;
  int InspectFieldCountForSmoke() const noexcept;
  QString InspectRawValueForSmoke(int row) const;
  QString InspectLogicalValueForSmoke(int row) const;
  bool SelectFirstMappableFieldForSmoke(QString& error);
  bool VerifyInvalidDraftRetentionForSmoke(QString& error);
  bool VerifyPipelineSwitchClearsInspectForSmoke(QString& error);
  void InvalidatePreviewForSmoke();
  std::size_t PreviewFrameSizeForSmoke() const noexcept;
  const std::vector<std::uint8_t>& PreviewFrameForSmoke() const noexcept;
  std::size_t HighlightedCellCountForSmoke() const noexcept;
  std::uint8_t HighlightMaskForSmoke(std::size_t frame_byte_index) const noexcept;
  const EncodeTimingSnapshot& LastTiming() const noexcept { return timing_; }

 private:
  void BuildUi();
  void BeginLoadFromPath();
  void ResetVisibleDocument();
  void RebuildSelectorsAndModel();
  void RebuildMessageSelector();
  void RefreshState();
  void RefreshPreview();
  void RefreshInspect();
  void RefreshModePresentation();
  void RefreshFieldDetails(int row);
  void SelectPipeline(int combo_index);
  void SelectMessage(int combo_index);
  void EncodeCurrent();
  void InspectCurrent();
  void SelectMode(int combo_index);
  void InvalidateEditedPreview();
  void InvalidateEditedDraft(std::size_t field_index);
  std::vector<PhysicalBitMask> InspectFailureHighlights(const MessageDescriptor* message) const;
  const MessageDescriptor* CurrentMessage() const noexcept;
  const MessageDescriptor* DisplayedMessage() const noexcept;

  CompileWorker& worker_;
  DocumentSession session_;
  bool closed_ = false;
  bool rebuilding_selectors_ = false;

  QLineEdit* path_edit_ = nullptr;
  QPushButton* browse_button_ = nullptr;
  QPushButton* load_button_ = nullptr;
  QLabel* identity_label_ = nullptr;
  QComboBox* pipeline_combo_ = nullptr;
  QComboBox* mode_combo_ = nullptr;
  QComboBox* message_combo_ = nullptr;
  QPushButton* encode_button_ = nullptr;
  QPushButton* inspect_button_ = nullptr;
  QLabel* inspect_input_label_ = nullptr;
  QPlainTextEdit* inspect_input_ = nullptr;
  QLabel* result_kind_label_ = nullptr;
  QTableView* field_table_ = nullptr;
  FieldTableModel* field_model_ = nullptr;
  ExactValueDelegate* value_delegate_ = nullptr;
  HexView* hex_view_ = nullptr;
  QTextBrowser* details_view_ = nullptr;
  QLabel* diagnostic_label_ = nullptr;
  QLabel* timing_label_ = nullptr;
  EncodeTimingSnapshot timing_;
};

}  // namespace pae::protocol_lab_ui
