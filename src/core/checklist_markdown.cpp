#include "core/checklist_markdown.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "core/logging.hpp"

namespace {

using core::ChecklistSlug;
using core::ChecklistStatus;
using core::RelationshipEdge;
using core::markdown::ParsedChecklist;

std::string Trim(const std::string& value) {
  std::size_t start = 0;
  std::size_t end = value.size();
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return value.substr(start, end - start);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.compare(0, prefix.size(), prefix) == 0;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string StripPrefix(const std::string& value, const std::string& prefix) {
  if (StartsWith(value, prefix)) {
    return value.substr(prefix.size());
  }
  return value;
}

void Require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct ProcedureBuilder {
  std::string section;
  std::string procedure;
  std::string action;
  std::string spec;
  std::string result;
  std::string status;
  std::string comment;
  std::string timestamp;
  std::string instructions;
  std::vector<RelationshipEdge> relationships;
  std::string checklist_id_hint;
};

ChecklistSlug FinalizeSlug(const std::string& checklist, ProcedureBuilder builder) {
  Require(!builder.section.empty(), "Section (H1) is required before procedures.");
  Require(!builder.procedure.empty(), "Procedure (H2) is required.");
  Require(!builder.action.empty(), "Action is required.");
  Require(!builder.spec.empty(), "Spec is required.");
  Require(!builder.status.empty(), "Status is required.");

  ChecklistSlug slug;
  slug.checklist = checklist;
  slug.section = builder.section;
  slug.procedure = builder.procedure;
  slug.action = builder.action;
  slug.spec = builder.spec;
  slug.result = builder.result;
  slug.status = core::ParseStatus(builder.status);
  Require(slug.status != ChecklistStatus::kUnknown,
          "Status must be Pass, Fail, NA, or Other.");
  slug.comment = builder.comment;
  slug.timestamp = builder.timestamp;
  slug.instructions = builder.instructions;
  slug.checklist_id =
      core::ComputeChecklistId(slug.checklist, slug.section, slug.procedure, slug.action,
                               slug.spec);

  if (!builder.checklist_id_hint.empty() &&
      builder.checklist_id_hint != slug.checklist_id) {
    throw std::runtime_error("Checklist ID mismatch for procedure '" + slug.procedure +
                             "': expected " + slug.checklist_id + " but found " +
                             builder.checklist_id_hint);
  }

  slug.relationships = std::move(builder.relationships);
  return slug;
}

}  // namespace

namespace core::markdown {

ParsedChecklist ParseChecklistMarkdown(const std::string& checklist_name,
                                       const std::string& content) {
  ParsedChecklist parsed;
  parsed.checklist = checklist_name;

  std::vector<std::string> lines;
  {
    std::string current;
    std::istringstream stream(content);
    while (std::getline(stream, current)) {
      if (!current.empty() && current.back() == '\r') {
        current.pop_back();
      }
      lines.push_back(current);
    }
  }

  std::string current_section;
  ProcedureBuilder builder;
  bool in_instructions = false;
  bool in_relationships = false;

  auto flush = [&]() {
    if (builder.procedure.empty()) {
      return;
    }
    parsed.slugs.push_back(FinalizeSlug(checklist_name, builder));
    builder = ProcedureBuilder{};
    builder.section = current_section;
    in_instructions = false;
    in_relationships = false;
  };

  for (const auto& raw_line : lines) {
    const std::string line = Trim(raw_line);
    if (line.empty()) {
      if (in_instructions && !builder.instructions.empty()) {
        builder.instructions.append("\n");
      }
      continue;
    }

    if (StartsWith(line, "# ")) {
      flush();
      current_section = Trim(line.substr(2));
      builder.section = current_section;
      continue;
    }

    if (StartsWith(line, "## ")) {
      flush();
      builder.procedure = Trim(line.substr(3));
      builder.section = current_section;
      continue;
    }

    if (StartsWith(line, "### ")) {
      const std::string header = ToLower(Trim(line.substr(4)));
      if (header == "instructions") {
        in_instructions = true;
        in_relationships = false;
      } else if (header == "relationships") {
        in_relationships = true;
        in_instructions = false;
      }
      continue;
    }

    if (in_instructions) {
      if (!builder.instructions.empty()) {
        builder.instructions.append("\n");
      }
      builder.instructions.append(line);
      continue;
    }

    if (in_relationships) {
      if (StartsWith(ToLower(line), "**checklist id:**")) {
        const auto value = Trim(StripPrefix(line, "**Checklist ID:**"));
        builder.checklist_id_hint = value;
      } else if (StartsWith(line, "-")) {
        const auto edge_text = Trim(line.substr(1));
        if (ToLower(edge_text) == "(none)") {
          continue;
        }
        const auto space_pos = edge_text.find(' ');
        Require(space_pos != std::string::npos, "Relationship must be 'predicate TARGET_ID'.");
        RelationshipEdge edge;
        edge.predicate = Trim(edge_text.substr(0, space_pos));
        edge.target = Trim(edge_text.substr(space_pos + 1));
        Require(!edge.predicate.empty(), "Relationship predicate cannot be empty.");
        Require(!edge.target.empty(), "Relationship target cannot be empty.");
        builder.relationships.push_back(edge);
      }
      continue;
    }

    if (StartsWith(line, "-")) {
      const auto lowered = ToLower(line);
      const auto colon_pos = line.find(':');
      if (colon_pos == std::string::npos) {
        core::logging::LogWarn("Bullet missing ':' separator in Markdown: " + line);
        continue;
      }
      const auto tail = Trim(line.substr(colon_pos + 1));

      if (StartsWith(lowered, "- **action**")) {
        builder.action = tail;
      } else if (StartsWith(lowered, "- **spec**")) {
        builder.spec = tail;
      } else if (StartsWith(lowered, "- **result**")) {
        builder.result = tail;
      } else if (StartsWith(lowered, "- **status**")) {
        builder.status = tail;
      } else if (StartsWith(lowered, "- **comment**")) {
        builder.comment = tail;
      } else if (StartsWith(lowered, "- **timestamp**")) {
        builder.timestamp = tail;
      } else {
        core::logging::LogWarn("Unrecognized bullet in Markdown: " + line);
      }
      continue;
    }
  }

  flush();
  Require(!parsed.slugs.empty(), "No checklist procedures were parsed from Markdown.");
  return parsed;
}

std::string ExportChecklistMarkdown(const std::string& checklist_name,
                                    const std::vector<ChecklistSlug>& slugs) {
  if (slugs.empty()) {
    throw std::runtime_error("Checklist contains no slugs to export.");
  }

  auto ordered = slugs;
  std::sort(ordered.begin(), ordered.end(), [](const ChecklistSlug& a, const ChecklistSlug& b) {
    if (a.section != b.section) return a.section < b.section;
    if (a.procedure != b.procedure) return a.procedure < b.procedure;
    return a.action < b.action;
  });

  std::ostringstream out;
  std::string current_section;

  for (const auto& slug : ordered) {
    if (slug.section != current_section) {
      if (!current_section.empty()) {
        out << "\n";
      }
      current_section = slug.section;
      out << "# " << current_section << "\n\n";
    }

    out << "## " << slug.procedure << "\n\n";
    out << "- **Action**: " << slug.action << "\n";
    out << "- **Spec**: " << slug.spec << "\n";
    out << "- **Result**: " << slug.result << "\n";
    out << "- **Status**: " << core::StatusToString(slug.status) << "\n";
    out << "- **Comment**: " << slug.comment << "\n";
    if (!slug.timestamp.empty()) {
      out << "- **Timestamp**: " << slug.timestamp << "\n";
    }
    out << "\n";
    out << "### Instructions\n";
    out << (slug.instructions.empty() ? "TBD" : slug.instructions) << "\n\n";
    out << "### Relationships\n";
    out << "**Checklist ID:** " << slug.checklist_id << "\n";
    if (slug.relationships.empty()) {
      out << "- (none)\n";
    } else {
      for (const auto& edge : slug.relationships) {
        out << "- " << edge.predicate << " " << edge.target << "\n";
      }
    }
    out << "\n";
  }

  return out.str();
}

}  // namespace core::markdown
