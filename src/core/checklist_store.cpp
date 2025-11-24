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

int Prepare(sqlite3* db, const std::string& sql, sqlite3_stmt** stmt);
void Finalize(sqlite3_stmt* stmt);
void StepOrThrow(sqlite3_stmt* stmt, const std::string& context);

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

int64_t ColumnInt64(sqlite3_stmt* stmt, int column) {
  return sqlite3_column_int64(stmt, column);
}

void InsertOrIgnore(sqlite3* db, const std::string& sql, const std::vector<std::string>& params) {
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare insert-or-ignore");
  }
  for (std::size_t i = 0; i < params.size(); ++i) {
    sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
  }
  StepOrThrow(stmt, "insert-or-ignore");
  Finalize(stmt);
}

int64_t ResolveChecklistId(sqlite3* db, const std::string& name) {
  InsertOrIgnore(db, "INSERT OR IGNORE INTO checklists (name) VALUES (?);", {name});
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, "SELECT id FROM checklists WHERE name=?;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to resolve checklist id");
  }
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Checklist not found after insert: " + name);
  }
  const int64_t id = ColumnInt64(stmt, 0);
  Finalize(stmt);
  return id;
}

int64_t ResolveSectionId(sqlite3* db, int64_t checklist_id, const std::string& name) {
  sqlite3_stmt* insert = nullptr;
  if (Prepare(db, "INSERT OR IGNORE INTO sections (name, checklist_id) VALUES (?, ?);",
              &insert) != SQLITE_OK) {
    Finalize(insert);
    throw std::runtime_error("Failed to prepare section insert");
  }
  sqlite3_bind_text(insert, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(insert, 2, checklist_id);
  StepOrThrow(insert, "section insert");
  Finalize(insert);

  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, "SELECT id FROM sections WHERE checklist_id=? AND name=?;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to resolve section id");
  }
  sqlite3_bind_int64(stmt, 1, checklist_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Section not found after insert: " + name);
  }
  const int64_t id = ColumnInt64(stmt, 0);
  Finalize(stmt);
  return id;
}

int64_t ResolveProcedureId(sqlite3* db, int64_t section_id, const std::string& name) {
  sqlite3_stmt* insert = nullptr;
  if (Prepare(db, "INSERT OR IGNORE INTO procedures (name, section_id) VALUES (?, ?);",
              &insert) != SQLITE_OK) {
    Finalize(insert);
    throw std::runtime_error("Failed to prepare procedure insert");
  }
  sqlite3_bind_text(insert, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(insert, 2, section_id);
  StepOrThrow(insert, "procedure insert");
  Finalize(insert);

  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, "SELECT id FROM procedures WHERE section_id=? AND name=?;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to resolve procedure id");
  }
  sqlite3_bind_int64(stmt, 1, section_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Procedure not found after insert: " + name);
  }
  const int64_t id = ColumnInt64(stmt, 0);
  Finalize(stmt);
  return id;
}

int64_t ResolveActionId(sqlite3* db, int64_t procedure_id, const std::string& name) {
  sqlite3_stmt* insert = nullptr;
  if (Prepare(db, "INSERT OR IGNORE INTO actions (name, procedure_id) VALUES (?, ?);",
              &insert) != SQLITE_OK) {
    Finalize(insert);
    throw std::runtime_error("Failed to prepare action insert");
  }
  sqlite3_bind_text(insert, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(insert, 2, procedure_id);
  StepOrThrow(insert, "action insert");
  Finalize(insert);

  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, "SELECT id FROM actions WHERE procedure_id=? AND name=?;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to resolve action id");
  }
  sqlite3_bind_int64(stmt, 1, procedure_id);
  sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Action not found after insert: " + name);
  }
  const int64_t id = ColumnInt64(stmt, 0);
  Finalize(stmt);
  return id;
}

int64_t ResolveSpecId(sqlite3* db, int64_t action_id, const std::string& text) {
  sqlite3_stmt* insert = nullptr;
  if (Prepare(db, "INSERT OR IGNORE INTO specs (text, action_id) VALUES (?, ?);", &insert) !=
      SQLITE_OK) {
    Finalize(insert);
    throw std::runtime_error("Failed to prepare spec insert");
  }
  sqlite3_bind_text(insert, 1, text.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(insert, 2, action_id);
  StepOrThrow(insert, "spec insert");
  Finalize(insert);

  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db, "SELECT id FROM specs WHERE action_id=? AND text=?;", &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to resolve spec id");
  }
  sqlite3_bind_int64(stmt, 1, action_id);
  sqlite3_bind_text(stmt, 2, text.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Spec not found after insert: " + text);
  }
  const int64_t id = ColumnInt64(stmt, 0);
  Finalize(stmt);
  return id;
}

std::vector<std::string> TableColumns(sqlite3* db, const std::string& table) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql = "PRAGMA table_info(" + table + ");";
  if (Prepare(db, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to inspect table schema for " + table);
  }
  std::vector<std::string> names;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    names.push_back(ColumnText(stmt, 1));
  }
  Finalize(stmt);
  return names;
}

bool HasColumn(const std::vector<std::string>& columns, const std::string& name) {
  return std::find(columns.begin(), columns.end(), name) != columns.end();
}

ChecklistSlug BuildSlug(sqlite3_stmt* stmt) {
  ChecklistSlug slug;
  slug.address_id = ColumnText(stmt, 0);
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

std::string ComputeAddressId(const std::string& checklist, const std::string& section,
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
  try {
    const auto columns = TableColumns(db_, "slugs");
    const bool has_fk_columns = HasColumn(columns, "checklist_id") &&
                                HasColumn(columns, "section_id") &&
                                HasColumn(columns, "procedure_id") &&
                                HasColumn(columns, "action_id") && HasColumn(columns, "spec_id") &&
                                HasColumn(columns, "address_id");
    const bool has_legacy_text_columns =
        HasColumn(columns, "checklist") || HasColumn(columns, "section") ||
        HasColumn(columns, "procedure") || HasColumn(columns, "action") ||
        HasColumn(columns, "spec");
    if (!columns.empty() && (!has_fk_columns || has_legacy_text_columns)) {
      LogInfo("Dropping legacy slugs schema to apply normalized schema");
      const char* drop_sql =
          "DROP TABLE IF EXISTS relationships;"
          "DROP TABLE IF EXISTS history;"
          "DROP TABLE IF EXISTS slugs;"
          "DROP TABLE IF EXISTS specs;"
          "DROP TABLE IF EXISTS actions;"
          "DROP TABLE IF EXISTS procedures;"
          "DROP TABLE IF EXISTS sections;"
          "DROP TABLE IF EXISTS checklists;";
      char* errmsg = nullptr;
      if (sqlite3_exec(db_, drop_sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string message = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("Failed to drop legacy tables: " + message);
      }
    }
  } catch (...) {
    // If inspection fails (e.g., table absent), proceed to schema creation.
  }

  const char* kSchema = R"sql(
    CREATE TABLE IF NOT EXISTS checklists (
        id    INTEGER PRIMARY KEY AUTOINCREMENT,
        name  TEXT NOT NULL UNIQUE
    );

    CREATE TABLE IF NOT EXISTS sections (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        name          TEXT NOT NULL,
        checklist_id  INTEGER NOT NULL,
        UNIQUE(checklist_id, name),
        FOREIGN KEY(checklist_id) REFERENCES checklists(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS procedures (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        name        TEXT NOT NULL,
        section_id  INTEGER NOT NULL,
        UNIQUE(section_id, name),
        FOREIGN KEY(section_id) REFERENCES sections(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS actions (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        name          TEXT NOT NULL,
        procedure_id  INTEGER NOT NULL,
        UNIQUE(procedure_id, name),
        FOREIGN KEY(procedure_id) REFERENCES procedures(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS specs (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        text       TEXT NOT NULL,
        action_id  INTEGER NOT NULL,
        UNIQUE(action_id, text),
        FOREIGN KEY(action_id) REFERENCES actions(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS slugs (
        address_id    TEXT PRIMARY KEY,
        checklist_id  INTEGER NOT NULL,
        section_id    INTEGER NOT NULL,
        procedure_id  INTEGER NOT NULL,
        action_id     INTEGER NOT NULL,
        spec_id       INTEGER NOT NULL,
        result        TEXT,
        status        TEXT CHECK (status IN ('Pass','Fail','NA','Other','Unknown')),
        comment       TEXT,
        timestamp     TEXT,
        instructions  TEXT,
        FOREIGN KEY(checklist_id) REFERENCES checklists(id) ON DELETE CASCADE,
        FOREIGN KEY(section_id)   REFERENCES sections(id)   ON DELETE CASCADE,
        FOREIGN KEY(procedure_id) REFERENCES procedures(id) ON DELETE CASCADE,
        FOREIGN KEY(action_id)    REFERENCES actions(id)    ON DELETE CASCADE,
        FOREIGN KEY(spec_id)      REFERENCES specs(id)      ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS relationships (
        subject_id  TEXT NOT NULL,
        predicate   TEXT NOT NULL,
        target_id   TEXT NOT NULL,
        FOREIGN KEY(subject_id) REFERENCES slugs(address_id) ON DELETE CASCADE,
        FOREIGN KEY(target_id)  REFERENCES slugs(address_id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS history (
        address_id  TEXT NOT NULL,
        timestamp   TEXT NOT NULL,
        result      TEXT,
        status      TEXT,
        comment     TEXT,
        FOREIGN KEY(address_id) REFERENCES slugs(address_id) ON DELETE CASCADE,
        PRIMARY KEY (address_id, timestamp)
    );

    CREATE INDEX IF NOT EXISTS idx_slugs_checklist_id  ON slugs(checklist_id);
    CREATE INDEX IF NOT EXISTS idx_slugs_section_id    ON slugs(section_id);
    CREATE INDEX IF NOT EXISTS idx_slugs_procedure_id  ON slugs(procedure_id);
    CREATE INDEX IF NOT EXISTS idx_slugs_action_id     ON slugs(action_id);
    CREATE INDEX IF NOT EXISTS idx_slugs_spec_id       ON slugs(spec_id);
    CREATE INDEX IF NOT EXISTS idx_relationships_subject ON relationships(subject_id);
    CREATE INDEX IF NOT EXISTS idx_relationships_target  ON relationships(target_id);
    CREATE INDEX IF NOT EXISTS idx_history_address ON history(address_id);
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
      /*address_id=*/{},
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
      /*address_id=*/{},
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
      /*address_id=*/{},
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

  power_check.address_id = ComputeAddressId(
      power_check.checklist, power_check.section, power_check.procedure, power_check.action,
      power_check.spec);
  uplink_check.address_id = ComputeAddressId(
      uplink_check.checklist, uplink_check.section, uplink_check.procedure, uplink_check.action,
      uplink_check.spec);
  dhcp_check.address_id = ComputeAddressId(
      dhcp_check.checklist, dhcp_check.section, dhcp_check.procedure, dhcp_check.action,
      dhcp_check.spec);

  uplink_check.relationships = {RelationshipEdge{"depends_on", power_check.address_id}};
  dhcp_check.relationships = {RelationshipEdge{"depends_on", uplink_check.address_id}};

  UpsertSlug(power_check);
  UpsertSlug(uplink_check);
  UpsertSlug(dhcp_check);

  ReplaceRelationships(power_check.address_id, power_check.relationships);
  ReplaceRelationships(uplink_check.address_id, uplink_check.relationships);
  ReplaceRelationships(dhcp_check.address_id, dhcp_check.relationships);
}

void ChecklistStore::UpsertSlug(const ChecklistSlug& slug) {
  std::lock_guard<std::mutex> lock(mutex_);
  UpsertSlugUnlocked(slug);
}

void ChecklistStore::UpsertSlugUnlocked(const ChecklistSlug& slug) {
  const int64_t checklist_id = ResolveChecklistId(db_, slug.checklist);
  const int64_t section_id = ResolveSectionId(db_, checklist_id, slug.section);
  const int64_t procedure_id = ResolveProcedureId(db_, section_id, slug.procedure);
  const int64_t action_id = ResolveActionId(db_, procedure_id, slug.action);
  const int64_t spec_id = ResolveSpecId(db_, action_id, slug.spec);

  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "INSERT INTO slugs (address_id, checklist_id, section_id, procedure_id, action_id, spec_id, "
      "result, status, comment, timestamp, instructions) VALUES (?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(address_id) DO UPDATE SET result=excluded.result, status=excluded.status, "
      "comment=excluded.comment, timestamp=excluded.timestamp, instructions=excluded.instructions;";

  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug upsert");
  }

  sqlite3_bind_text(stmt, 1, slug.address_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, checklist_id);
  sqlite3_bind_int64(stmt, 3, section_id);
  sqlite3_bind_int64(stmt, 4, procedure_id);
  sqlite3_bind_int64(stmt, 5, action_id);
  sqlite3_bind_int64(stmt, 6, spec_id);
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

ChecklistSlug ChecklistStore::GetSlugOrThrow(const std::string& address_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT s.address_id, c.name, sec.name, p.name, a.name, sp.text, s.result, s.status, "
      "s.comment, s.timestamp, s.instructions "
      "FROM slugs s "
      "JOIN checklists c ON s.checklist_id = c.id "
      "JOIN sections sec ON s.section_id = sec.id "
      "JOIN procedures p ON s.procedure_id = p.id "
      "JOIN actions a ON s.action_id = a.id "
      "JOIN specs sp ON s.spec_id = sp.id "
      "WHERE s.address_id=?;";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug lookup");
  }
  sqlite3_bind_text(stmt, 1, address_id.c_str(), -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    Finalize(stmt);
    throw std::runtime_error("Address ID not found: " + address_id);
  }

  ChecklistSlug slug = BuildSlug(stmt);
  Finalize(stmt);
  slug.relationships = LoadOutgoingEdges(address_id);
  return slug;
}

std::vector<ChecklistSlug> ChecklistStore::GetSlugsForChecklist(
    const std::string& checklist) const {
  std::vector<ChecklistSlug> slugs;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT s.address_id, c.name, sec.name, p.name, a.name, sp.text, s.result, s.status, "
      "s.comment, s.timestamp, s.instructions "
      "FROM slugs s "
      "JOIN checklists c ON s.checklist_id = c.id "
      "JOIN sections sec ON s.section_id = sec.id "
      "JOIN procedures p ON s.procedure_id = p.id "
      "JOIN actions a ON s.action_id = a.id "
      "JOIN specs sp ON s.spec_id = sp.id "
      "WHERE c.name=? "
      "ORDER BY sec.name, p.name, a.name;";

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
    slug.relationships = LoadOutgoingEdges(slug.address_id);
  }

  return slugs;
}

RelationshipGraph ChecklistStore::GetRelationships(const std::string& address_id) const {
  RelationshipGraph graph;
  std::lock_guard<std::mutex> lock(mutex_);

  sqlite3_stmt* outgoing_stmt = nullptr;
  if (Prepare(db_, "SELECT predicate, target_id FROM relationships WHERE subject_id=?;",
              &outgoing_stmt) != SQLITE_OK) {
    Finalize(outgoing_stmt);
    throw std::runtime_error("Failed to prepare outgoing relationship lookup");
  }
  sqlite3_bind_text(outgoing_stmt, 1, address_id.c_str(), -1, SQLITE_TRANSIENT);
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
  sqlite3_bind_text(incoming_stmt, 1, address_id.c_str(), -1, SQLITE_TRANSIENT);
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
  const ChecklistSlug existing = GetSlugOrThrow(update.address_id);
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
      "UPDATE slugs SET result=?, status=?, comment=?, timestamp=? WHERE address_id=?;";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare slug update");
  }

  sqlite3_bind_text(stmt, 1, mutated.result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, StatusToString(mutated.status).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, mutated.comment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, mutated.timestamp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, mutated.address_id.c_str(), -1, SQLITE_TRANSIENT);

  StepOrThrow(stmt, "slug update");
  Finalize(stmt);
  InsertHistorySnapshot(mutated);
}

void ChecklistStore::ReplaceChecklist(const std::string& checklist,
                                      const std::vector<ChecklistSlug>& slugs) {
  if (checklist.empty()) {
    throw std::invalid_argument("Checklist name must not be empty.");
  }
  for (const auto& slug : slugs) {
    if (slug.checklist != checklist) {
      throw std::invalid_argument("All slugs must belong to checklist '" + checklist + "'.");
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);

  char* errmsg = nullptr;
  sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
  if (errmsg) {
    std::string message = errmsg;
    sqlite3_free(errmsg);
    throw std::runtime_error("Failed to begin transaction for checklist replace: " + message);
  }

  auto rollback = [&]() {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
  };

  sqlite3_stmt* delete_slugs = nullptr;
  sqlite3_stmt* insert_rel = nullptr;

  try {
    const std::string slugs_sql =
        "DELETE FROM slugs WHERE checklist_id IN (SELECT id FROM checklists WHERE name=?);";

    if (Prepare(db_, slugs_sql, &delete_slugs) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare checklist cleanup statements.");
    }

    sqlite3_bind_text(delete_slugs, 1, checklist.c_str(), -1, SQLITE_TRANSIENT);
    StepOrThrow(delete_slugs, "slug delete");

    Finalize(delete_slugs);
    delete_slugs = nullptr;

    // Insert all slugs first to satisfy foreign keys, then batch relationships.
    std::vector<std::pair<std::string, RelationshipEdge>> pending_edges;
    for (const auto& slug : slugs) {
      UpsertSlugUnlocked(slug);
      for (const auto& edge : slug.relationships) {
        pending_edges.emplace_back(slug.address_id, edge);
      }
    }

    const std::string rel_insert_sql =
        "INSERT INTO relationships (subject_id, predicate, target_id) VALUES (?,?,?);";
    if (Prepare(db_, rel_insert_sql, &insert_rel) != SQLITE_OK) {
      throw std::runtime_error("Failed to prepare relationship insert for checklist replace.");
    }

    for (const auto& item : pending_edges) {
      const auto& subject = item.first;
      const auto& edge = item.second;
      sqlite3_reset(insert_rel);
      sqlite3_bind_text(insert_rel, 1, subject.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(insert_rel, 2, edge.predicate.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(insert_rel, 3, edge.target.c_str(), -1, SQLITE_TRANSIENT);
      StepOrThrow(insert_rel, "relationship insert");
    }

    Finalize(insert_rel);
    insert_rel = nullptr;
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
  } catch (...) {
    Finalize(delete_slugs);
    Finalize(insert_rel);
    rollback();
    throw;
  }
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
      "INSERT OR IGNORE INTO history (address_id, timestamp, result, status, comment) "
      "VALUES (?,?,?,?,?);";
  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare history insert");
  }

  sqlite3_bind_text(stmt, 1, slug.address_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, slug.timestamp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, slug.result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, StatusToString(slug.status).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, slug.comment.c_str(), -1, SQLITE_TRANSIENT);

  StepOrThrow(stmt, "history insert");
  Finalize(stmt);
}

std::vector<RelationshipEdge> ChecklistStore::LoadOutgoingEdges(
    const std::string& address_id) const {
  std::vector<RelationshipEdge> edges;
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db_, "SELECT predicate, target_id FROM relationships WHERE subject_id=?;",
              &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare outgoing relationships query");
  }

  sqlite3_bind_text(stmt, 1, address_id.c_str(), -1, SQLITE_TRANSIENT);
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
      "SELECT s.address_id, c.name, sec.name, p.name, a.name, sp.text, s.result, s.status, "
      "s.comment, s.timestamp, s.instructions "
      "FROM slugs s "
      "JOIN checklists c ON s.checklist_id = c.id "
      "JOIN sections sec ON s.section_id = sec.id "
      "JOIN procedures p ON s.procedure_id = p.id "
      "JOIN actions a ON s.action_id = a.id "
      "JOIN specs sp ON s.spec_id = sp.id "
      "ORDER BY c.name, sec.name, p.name, a.name;";

  if (Prepare(db_, sql, &stmt) != SQLITE_OK) {
    Finalize(stmt);
    throw std::runtime_error("Failed to prepare export query");
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    slugs.push_back(BuildSlug(stmt));
  }
  Finalize(stmt);

  for (auto& slug : slugs) {
    slug.relationships = LoadOutgoingEdges(slug.address_id);
  }
  return slugs;
}

std::vector<std::string> ChecklistStore::ListChecklists() const {
  std::vector<std::string> names;
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  if (Prepare(db_, "SELECT name FROM checklists ORDER BY name;", &stmt) !=
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

