#pragma once

#include <string>
#include <vector>

#include "core/checklist_store.hpp"

namespace core::markdown {

struct ParsedChecklist {
  std::string checklist;
  std::vector<ChecklistSlug> slugs;
};

ParsedChecklist ParseChecklistMarkdown(const std::string& checklist_name,
                                       const std::string& content);

std::string ExportChecklistMarkdown(const std::string& checklist_name,
                                    const std::vector<ChecklistSlug>& slugs);

}  // namespace core::markdown
