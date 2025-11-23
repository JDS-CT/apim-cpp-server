#pragma once

#include <optional>
#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

namespace core {

enum class ChecklistStatus { kUnknown = 0, kPass, kFail, kNA, kOther };

struct RelationshipEdge {
  std::string predicate;
  std::string target;
};

struct RelationshipGraph {
  std::vector<RelationshipEdge> outgoing;
  std::vector<RelationshipEdge> incoming;
};

struct ChecklistSlug {
  std::string address_id;
  std::string checklist;
  std::string section;
  std::string procedure;
  std::string action;
  std::string spec;

  std::string result;
  ChecklistStatus status = ChecklistStatus::kUnknown;
  std::string comment;
  std::string timestamp;

  std::string instructions;
  std::vector<RelationshipEdge> relationships;
};

struct SlugUpdate {
  std::string address_id;
  std::optional<std::string> result;
  std::optional<ChecklistStatus> status;
  std::optional<std::string> comment;
  std::optional<std::string> timestamp;
};

class ChecklistStore {
 public:
  explicit ChecklistStore(std::string db_path);
  ~ChecklistStore();

  ChecklistStore(const ChecklistStore&) = delete;
  ChecklistStore& operator=(const ChecklistStore&) = delete;

  void Initialize(bool seed_demo_data);
  ChecklistSlug GetSlugOrThrow(const std::string& address_id) const;
  std::vector<ChecklistSlug> GetSlugsForChecklist(const std::string& checklist) const;
  RelationshipGraph GetRelationships(const std::string& address_id) const;
  void ApplyUpdate(const SlugUpdate& update);
  void ApplyBulkUpdates(const std::vector<SlugUpdate>& updates);
  void ReplaceChecklist(const std::string& checklist, const std::vector<ChecklistSlug>& slugs);
  std::vector<ChecklistSlug> ExportAllSlugs() const;
  std::vector<std::string> ListChecklists() const;

 private:
  void EnsureSchema();
  bool HasAnySlugs() const;
  void SeedDemoData();
  void UpsertSlug(const ChecklistSlug& slug);
  void UpsertSlugUnlocked(const ChecklistSlug& slug);
  void ReplaceRelationships(const std::string& subject_id,
                            const std::vector<RelationshipEdge>& edges);
  void InsertHistorySnapshot(const ChecklistSlug& slug);
  std::vector<RelationshipEdge> LoadOutgoingEdges(const std::string& address_id) const;

  sqlite3* db_ = nullptr;
  std::string db_path_;
  mutable std::mutex mutex_;
};

ChecklistStatus ParseStatus(const std::string& value);
std::string StatusToString(ChecklistStatus status);
std::string ComputeAddressId(const std::string& checklist, const std::string& section,
                             const std::string& procedure, const std::string& action,
                             const std::string& spec);
std::string CurrentTimestampIsoUtc();

}  // namespace core
