#include <filesystem>
#include <iostream>

#include "core/checklist_store.hpp"

namespace {

std::string TempDbPath() {
  auto dir = std::filesystem::temp_directory_path() / "apim-schema-test.db";
  return dir.string();
}

void RemoveIfExists(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace

int main() {
  const auto db_path = TempDbPath();
  RemoveIfExists(db_path);

  try {
    core::ChecklistStore store(db_path);
    store.Initialize(/*seed_demo_data=*/false);

    core::ChecklistSlug slug;
    slug.checklist = "integration-checklist";
    slug.section = "Section A";
    slug.procedure = "Proc 1";
    slug.action = "Do thing";
    slug.spec = "Expected value";
    slug.result = "pending";
    slug.status = core::ChecklistStatus::kNA;
    slug.comment = "";
    slug.timestamp = core::CurrentTimestampIsoUtc();
    slug.instructions = "Steps go here";
    slug.address_id = core::ComputeAddressId(slug.checklist, slug.section, slug.procedure,
                                             slug.action, slug.spec);

    store.ReplaceChecklist(slug.checklist, {slug});

    const auto checklists = store.ListChecklists();
    if (checklists.empty() || checklists.front() != slug.checklist) {
      std::cerr << "Checklist list did not return expected name\n";
      return 1;
    }

    const auto slugs = store.GetSlugsForChecklist(slug.checklist);
    if (slugs.size() != 1) {
      std::cerr << "Expected 1 slug, got " << slugs.size() << "\n";
      return 1;
    }
    const auto& fetched = slugs.front();
    if (fetched.address_id != slug.address_id || fetched.action != slug.action ||
        fetched.spec != slug.spec) {
      std::cerr << "Fetched slug does not match inserted slug\n";
      return 1;
    }

    RemoveIfExists(db_path);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "integration_schema_test failure: " << ex.what() << "\n";
    RemoveIfExists(db_path);
    return 1;
  }
}
