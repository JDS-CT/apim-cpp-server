#include "core/checklist_store.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <cctype>

#include "core/logging.hpp"
#include "sqlite3.h"
#include "xxhash.h"

namespace {

using core::ChecklistSlug;
using core::ChecklistStatus;
using core::RelationshipEdge;
using core::RelationshipGraph;
using core::SlugUpdate;
using core::logging::LogError;
using core::logging::LogInfo;

constexpr char kBase32Alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string EncodeBase32(const std::array<uint8_t, 10>& bytes) {
  std::string output;
  output.reserve(16);

  uint32_t buffer = 0;
  int bits = 0;

  for (const auto value : bytes) {
    buffer = (buffer << 8) | value;
    bits += 8;

    while (bits >= 5) {
      bits -= 5;
      const uint32_t index = (buffer >> bits) & 0x1Fu;
      output.push_back(kBase32Alphabet[index]);
    }
  }

  return output;
}

std::string ColumnText(sqlite3_stmt* stmt, int column) {
  const unsigned char* text = sqlite3_column_text(stmt, column);
  if (!text) {
    return {};
  }
  return reinterpret_cast<const char*>(text);
}

ChecklistSlug BuildSlug(sqlite3_stmt* stmt) {
  ChecklistSlug slug;
  slug.checklist_id = ColumnText(stmt, 0);
  slug.checklist = ColumnText(stmt, 1);
  slug.section = ColumnText(stmt, 2);
  slug.procedure = ColumnText(stmt, 3);
  slug.action = ColumnText(stmt, 4);
  slug.spec = ColumnText(stmt, 5);
  slug.result = ColumnText(stmt, 6);
  slug.status = core::ParseStatus(ColumnText(stmt, 7));
  slug.comment = ColumnText(stmt, 8);
  slug.timestamp = ColumnText(stmt, 9);
  slug.instructions = ColumnText(stmt, 10);
  return slug;
}

void Finalize(sqlite3_stmt* stmt) {
  if (stmt) {
    sqlite3_finalize(stmt);
  }
}

int Prepare(sqlite3* db, const std::string& sql, sqlite3_stmt** stmt) {
  return sqlite3_prepare_v2(db, sql.c_str(), -1, stmt, nullptr);
}

void StepOrThrow(sqlite3_stmt* stmt, const std::string& context) {
  const int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    throw std::runtime_error(context + " failed: " + std::string(sqlite3_errstr(rc)));
  }
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

}  // namespace

namespace core {

ChecklistStatus ParseStatus(const std::string& value) {
  const std::string lowered = ToLower(value);
  if (lowered == "pass") {
    return ChecklistStatus::kPass;
  }
  if (lowered == "fail") {
    return ChecklistStatus::kFail;
  }
  if (lowered == "na" || lowered == "n/a") {
    return ChecklistStatus::kNA;
  }
  if (lowered == "other") {
    return ChecklistStatus::kOther;
  }
  return ChecklistStatus::kUnknown;
}

std::string StatusToString(ChecklistStatus status) {
  switch (status) {
    case ChecklistStatus::kPass:
      return "Pass";
    case ChecklistStatus::kFail:
      return "Fail";
    case ChecklistStatus::kNA:
      return "NA";
    case ChecklistStatus::kOther:
      return "Other";
    case ChecklistStatus::kUnknown:
    default:
      return "Unknown";
  }
}

std::string CurrentTimestampIsoUtc() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm tm_snapshot{};
#if defined(_WIN32)
  gmtime_s(&tm_snapshot, &time);
#else
  gmtime_r(&time, &tm_snapshot);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_snapshot, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

std::string ComputeChecklistId(const std::string& checklist, const std::string& section,
                               const std::string& procedure, const std::string& action,
                               const std::string& spec) {
  const std::string canonical =
      checklist + "||" + section + "||" + procedure + "||" + action + "||" + spec;
  const XXH128_hash_t hash = XXH3_128bits(canonical.data(), canonical.size());
  XXH128_canonical_t hash_bytes;
  XXH128_canonicalFromHash(&hash_bytes, hash);

  std::array<uint8_t, 10> truncated{};
  std::copy(hash_bytes.digest + 6, hash_bytes.digest + 16, truncated.begin());
  return EncodeBase32(truncated);
}

ChecklistStore::ChecklistStore(std::string db_path) : db_path_(std::move(db_path)) {}

ChecklistStore::~ChecklistStore() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void ChecklistStore::Initialize(bool seed_demo_data) {
  namespace fs = std::filesystem;
  const fs::path db_location(db_path_);
  if (db_location.has_parent_path()) {
    fs::create_directories(db_location.parent_path());
  }

  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
  const int rc = sqlite3_open_v2(db_path_.c_str(), &db_, flags, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error("Failed to open SQLite database at " + db_path_ + ": " +
                             sqlite3_errstr(rc));
  }

  {
    char* errmsg = nullptr;
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &errmsg);
    if (errmsg) {
      std::string message = errmsg;
      sqlite3_free(errmsg);
      throw std::runtime_error("Failed to enable foreign_keys: " + message);
    }
  }

  {
    char* errmsg = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errmsg);
    if (errmsg) {
      LogError(std::string{"Could not set WAL journal mode: "} + errmsg);
      sqlite3_free(errmsg);
    }
  }

  EnsureSchema();

  if (seed_demo_data && !HasAnySlugs()) {
    SeedDemoData();
  }
}

void ChecklistStore::EnsureSchema() {
  const char* kSchema = R"sql(
    CREATE TABLE IF NOT EXISTS slugs (
        checklist_id    TEXT PRIMARY KEY,
        checklist       TEXT NOT NULL,
        section         TEXT NOT NULL,
        procedure       TEXT NOT NULL,
        action          TEXT NOT NULL,
        spec            TEXT NOT NULL,
        result          TEXT,
        status          TEXT CHECK (status IN ('Pass','Fail','NA','Other','Unknown')),
        comment         TEXT,
        timestamp       TEXT,
        instructions    TEXT
    );
    CREATE TABLE IF NOT EXISTS relationships (
        subject_id  TEXT NOT NULL,
        predicate   TEXT NOT NULL,
        target_id   TEXT NOT NULL,
        FOREIGN KEY(subject_id) REFERENCES slugs(checklist_id),
        FOREIGN KEY(target_id)  REFERENCES slugs(checklist_id)
    );
    CREATE TABLE IF NOT EXISTS history (
        checklist_id  TEXT NOT NULL,
        timestamp     TEXT NOT NULL,
        result        TEXT,
        status        TEXT,
        comment       TEXT,
        FOREIGN KEY(checklist_id) REFERENCES slugs(checklist_id),
        PRIMARY KEY (checklist_id, timestamp)
    );
    CREATE INDEX IF NOT EXISTS idx_slugs_checklist ON slugs(checklist);
    CREATE INDEX IF NOT EXISTS idx_relationships_subject ON relationships(subject_id);
    CREATE INDEX IF NOT EXISTS idx_relationships_target ON relationships(target_id);
    CREATE INDEX IF NOT EXISTS idx_history_checklist ON history(checklist_id);
  )sql";

  char* errmsg = nullptr;
  const int rc = sqlite3_exec(db_, kSchema, nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK) {
    std::string message = errmsg ? errmsg : "";
    sqlite3_free(errmsg);
    throw std::runtime_error("Failed to initialize schema: " + message);
  }
}

bool ChecklistStore::HasAnySlugs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db_, "SELECT 1 FROM slugs LIMIT 1;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug count query");
  }

  const int rc = sqlite3_step(stmt);
  Finalize(stmt);
  return rc == SQLITE_ROW;
}

void ChecklistStore::SeedDemoData() {
  LogInfo("Seeding demo checklist data");
  ChecklistSlug power_check{
      /*checklist_id=*/{},
      /*checklist=*/"apim-demo",
      /*section=*/"Site Readiness",
      /*procedure=*/"Power Bring-up",
      /*action=*/"Verify rack power",
      /*spec=*/"24V DC stable",
      /*result=*/"24.1V",
      /*status=*/ChecklistStatus::kPass,
      /*comment=*/"Measured at supply taps.",
      /*timestamp=*/CurrentTimestampIsoUtc(),
      /*instructions=*/"Use calibrated multimeter on the main supply rails.",
      /*relationships=*/{}};

  ChecklistSlug uplink_check{
      /*checklist_id=*/{},
      /*checklist=*/"apim-demo",
      /*section=*/"Networking",
      /*procedure=*/"Switch bring-up",
      /*action=*/"Verify uplink",
      /*spec=*/"1GbE link up",
      /*result=*/"",
      /*status=*/ChecklistStatus::kFail,
      /*comment=*/"No link LED on port 48.",
      /*timestamp=*/CurrentTimestampIsoUtc(),
      /*instructions=*/"Patch into the core switch, check SFP seating, and confirm VLAN tagging.",
      /*relationships=*/{}};

  ChecklistSlug dhcp_check{
      /*checklist_id=*/{},
      /*checklist=*/"apim-demo",
      /*section=*/"Networking",
      /*procedure=*/"DHCP scope",
      /*action=*/"Request lease",
      /*spec=*/"Lease within 1s, correct gateway",
      /*result=*/"pending",
      /*status=*/ChecklistStatus::kNA,
      /*comment=*/"Waiting on uplink resolution.",
      /*timestamp=*/CurrentTimestampIsoUtc(),
      /*instructions=*/"Use iperf client once uplink is established to verify end-to-end path.",
      /*relationships=*/{}};

  power_check.checklist_id = ComputeChecklistId(
      power_check.checklist, power_check.section, power_check.procedure, power_check.action,
      power_check.spec);
  uplink_check.checklist_id = ComputeChecklistId(
      uplink_check.checklist, uplink_check.section, uplink_check.procedure, uplink_check.action,
      uplink_check.spec);
  dhcp_check.checklist_id = ComputeChecklistId(
      dhcp_check.checklist, dhcp_check.section, dhcp_check.procedure, dhcp_check.action,
      dhcp_check.spec);

  uplink_check.relationships = {RelationshipEdge{"depends_on", power_check.checklist_id}};
  dhcp_check.relationships = {RelationshipEdge{"depends_on", uplink_check.checklist_id}};

  UpsertSlug(power_check);
  UpsertSlug(uplink_check);
  UpsertSlug(dhcp_check);

  ReplaceRelationships(power_check.checklist_id, power_check.relationships);
  ReplaceRelationships(uplink_check.checklist_id, uplink_check.relationships);
  ReplaceRelationships(dhcp_check.checklist_id, dhcp_check.relationships);
}

void ChecklistStore::UpsertSlug(const ChecklistSlug& slug) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "INSERT INTO slugs (checklist_id, checklist, section, procedure, action, spec, result, "
      "status, comment, timestamp, instructions) VALUES (?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(checklist_id) DO UPDATE SET result=excluded.result, status=excluded.status, "
      "comment=excluded.comment, timestamp=excluded.timestamp, instructions=excluded.instructions;";

  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug upsert");
  }

  sqlite3_bind_text(stmt, 1, slug.checklist_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, slug.checklist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, slug.section.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, slug.procedure.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, slug.action.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, slug.spec.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, slug.result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, StatusToString(slug.status).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, slug.comment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, slug.timestamp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, slug.instructions.c_str(), -1, SQLITE_TRANSIENT);

  StepOrThrow(stmt, "slug upsert");
  Finalize(stmt);
}

void ChecklistStore::ReplaceRelationships(const std::string& subject_id,
                                          const std::vector<RelationshipEdge>& edges) {
  std::lock_guard<std::mutex> lock(mutex_);

  char* errmsg = nullptr;
  sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
  if (errmsg) {
    std::string message = errmsg;
    sqlite3_free(errmsg);
    throw std::runtime_error("Failed to begin transaction for relationships: " + message);
  }

  sqlite3_stmt* delete_stmt = nullptr;
  if (Prepare(db_, "DELETE FROM relationships WHERE subject_id=?;", &delete_stmt) != SQLITE_OK) {
    Finalize(delete_stmt);
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw std::runtime_error("Failed to prepare relationship delete");
  }
  sqlite3_bind_text(delete_stmt, 1, subject_id.c_str(), -1, SQLITE_TRANSIENT);
  StepOrThrow(delete_stmt, "relationship delete");
  Finalize(delete_stmt);

  sqlite3_stmt* insert_stmt = nullptr;
  if (Prepare(db_, "INSERT INTO relationships (subject_id, predicate, target_id) VALUES (?,?,?);",
              &insert_stmt) != SQLITE_OK) {
    Finalize(insert_stmt);
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw std::runtime_error("Failed to prepare relationship insert");
  }

  for (const auto& edge : edges) {
    sqlite3_reset(insert_stmt);
    sqlite3_bind_text(insert_stmt, 1, subject_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 2, edge.predicate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 3, edge.target.c_str(), -1, SQLITE_TRANSIENT);
    StepOrThrow(insert_stmt, "relationship insert");
  }

  Finalize(insert_stmt);
  sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

ChecklistSlug ChecklistStore::GetSlugOrThrow(const std::string& checklist_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT checklist_id, checklist, section, procedure, action, spec, result, status, "
      "comment, timestamp, instructions FROM slugs WHERE checklist_id=?;";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug lookup");
  }
  sqlite3_bind_text(stmt, 1, checklist_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Checklist ID not found: " + checklist_id);
  }

  ChecklistSlug slug = BuildSlug(stmt);
  Finalize(stmt);
  slug.relationships = LoadOutgoingEdges(checklist_id);
  return slug;
}

std::vector<ChecklistSlug> ChecklistStore::GetSlugsForChecklist(
    const std::string& checklist) const {
  std::vector<ChecklistSlug> slugs;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT checklist_id, checklist, section, procedure, action, spec, result, status, "
      "comment, timestamp, instructions FROM slugs WHERE checklist=? "
      "ORDER BY section, procedure, action;";

  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare checklist lookup");
  }
  sqlite3_bind_text(stmt, 1, checklist.c_str(), -1, SQLITE_TRANSIENT);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    slugs.push_back(BuildSlug(stmt));
  }
  Finalize(stmt);

  for (auto& slug : slugs) {
    slug.relationships = LoadOutgoingEdges(slug.checklist_id);
  }

  return slugs;
}

RelationshipGraph ChecklistStore::GetRelationships(const std::string& checklist_id) const {
  RelationshipGraph graph;
  std::lock_guard<std::mutex> lock(mutex_);

  sqlite3_stmt* outgoing_stmt = nullptr;
  if (Prepare(db_, "SELECT predicate, target_id FROM relationships WHERE subject_id=?;",
              &outgoing_stmt) != SQLITE_OK) {
    Finalize(outgoing_stmt);
    throw std::runtime_error("Failed to prepare outgoing relationship lookup");
  }
  sqlite3_bind_text(outgoing_stmt, 1, checklist_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(outgoing_stmt) == SQLITE_ROW) {
    RelationshipEdge edge;
    edge.predicate = ColumnText(outgoing_stmt, 0);
    edge.target = ColumnText(outgoing_stmt, 1);
    graph.outgoing.push_back(edge);
  }
  Finalize(outgoing_stmt);

  sqlite3_stmt* incoming_stmt = nullptr;
  if (Prepare(db_, "SELECT subject_id, predicate FROM relationships WHERE target_id=?;",
              &incoming_stmt) != SQLITE_OK) {
    Finalize(incoming_stmt);
    throw std::runtime_error("Failed to prepare incoming relationship lookup");
  }
  sqlite3_bind_text(incoming_stmt, 1, checklist_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(incoming_stmt) == SQLITE_ROW) {
    RelationshipEdge edge;
    edge.target = ColumnText(incoming_stmt, 0);
    edge.predicate = ColumnText(incoming_stmt, 1);
    graph.incoming.push_back(edge);
  }
  Finalize(incoming_stmt);

  return graph;
}

void ChecklistStore::ApplyUpdate(const SlugUpdate& update) {
  const ChecklistSlug existing = GetSlugOrThrow(update.checklist_id);
  ChecklistSlug mutated = existing;

  if (update.result) {
    mutated.result = *update.result;
  }
  if (update.status) {
    mutated.status = *update.status;
  }
  if (update.comment) {
    mutated.comment = *update.comment;
  }
  mutated.timestamp = update.timestamp.value_or(CurrentTimestampIsoUtc());

  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "UPDATE slugs SET result=?, status=?, comment=?, timestamp=? WHERE checklist_id=?;";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug update");
  }

  sqlite3_bind_text(stmt, 1, mutated.result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, StatusToString(mutated.status).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, mutated.comment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, mutated.timestamp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, mutated.checklist_id.c_str(), -1, SQLITE_TRANSIENT);

  StepOrThrow(stmt, "slug update");
  Finalize(stmt);
  InsertHistorySnapshot(mutated);
}

void ChecklistStore::ApplyBulkUpdates(const std::vector<SlugUpdate>& updates) {
  if (updates.empty()) {
    return;
  }

  char* errmsg = nullptr;
  sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
  if (errmsg) {
    std::string message = errmsg;
    sqlite3_free(errmsg);
    throw std::runtime_error("Failed to begin transaction for bulk update: " + message);
  }

  try {
    for (const auto& update : updates) {
      ApplyUpdate(update);
    }
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

void ChecklistStore::InsertHistorySnapshot(const ChecklistSlug& slug) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "INSERT OR IGNORE INTO history (checklist_id, timestamp, result, status, comment) "
      "VALUES (?,?,?,?,?);";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare history insert");
  }

  sqlite3_bind_text(stmt, 1, slug.checklist_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, slug.timestamp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, slug.result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, StatusToString(slug.status).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, slug.comment.c_str(), -1, SQLITE_TRANSIENT);

  StepOrThrow(stmt, "history insert");
  Finalize(stmt);
}

std::vector<RelationshipEdge> ChecklistStore::LoadOutgoingEdges(
    const std::string& checklist_id) const {
  std::vector<RelationshipEdge> edges;
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db_, "SELECT predicate, target_id FROM relationships WHERE subject_id=?;",
              &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare outgoing relationships query");
  }

  sqlite3_bind_text(stmt, 1, checklist_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    RelationshipEdge edge;
    edge.predicate = ColumnText(stmt, 0);
    edge.target = ColumnText(stmt, 1);
    edges.push_back(edge);
  }
  Finalize(stmt);
  return edges;
}

std::vector<ChecklistSlug> ChecklistStore::ExportAllSlugs() const {
  std::vector<ChecklistSlug> slugs;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT checklist_id, checklist, section, procedure, action, spec, result, status, "
      "comment, timestamp, instructions FROM slugs ORDER BY checklist, section, procedure;";

  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare export query");
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    slugs.push_back(BuildSlug(stmt));
  }
  Finalize(stmt);

  for (auto& slug : slugs) {
    slug.relationships = LoadOutgoingEdges(slug.checklist_id);
  }
  return slugs;
}

std::vector<std::string> ChecklistStore::ListChecklists() const {
  std::vector<std::string> names;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db_, "SELECT DISTINCT checklist FROM slugs ORDER BY checklist;", &stmt) !=
      SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare checklist listing query");
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    names.push_back(ColumnText(stmt, 0));
  }
  Finalize(stmt);
  return names;
}

}  // namespace core
