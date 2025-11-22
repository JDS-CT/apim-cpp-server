// Domain types (language-agnostic)
enum class Status {
    Pass,
    Fail,
    NA,
    Other,
    Unknown    // no selection / invalid
};

enum class IndicatorClass {
    Base,      // default / unset
    Success,
    Fail,
    Warning,
    Info,
    Neutral
};

struct Indicator {
    string symbol;        // e.g. "✅"
    IndicatorClass kind;  // maps to CSS / style in the UI layer
    string tooltip;       // hover text
};

function evaluate_indicator(status: Status, comment_text: string) -> Indicator {
    Indicator out;
    bool has_comment = (trim(comment_text) != "");

    switch (status) {
        case Status::Pass:
            out.symbol  = "✅";
            out.kind    = IndicatorClass::Success;
            out.tooltip = "Meets the required spec.";
            return out;

        case Status::Fail:
            if (!has_comment) {
                out.symbol  = "❌";
                out.kind    = IndicatorClass::Fail;
                out.tooltip = "Please update the status or add a comment.";
            } else {
                out.symbol  = "⚠️";
                out.kind    = IndicatorClass::Warning;
                out.tooltip = "Status flagged: double-check that the comment describes findings.";
            }
            return out;

        case Status::Other:
            if (!has_comment) {
                out.symbol  = "⚠️";
                out.kind    = IndicatorClass::Warning;
                out.tooltip = "Status flagged: if 'Other' is selected, please add a comment.";
            } else {
                out.symbol  = "🟡";
                out.kind    = IndicatorClass::Info;
                out.tooltip = "Non-standard state: the deviation is to be described in the comment.";
            }
            return out;

        case Status::NA:
            out.symbol  = "⬛";
            out.kind    = IndicatorClass::Neutral;
            out.tooltip = "Not Applicable: this will be removed from the final report.";
            return out;

        case Status::Unknown:
        default:
            out.symbol  = "";
            out.kind    = IndicatorClass::Base;
            out.tooltip = "No valid status selected.";
            return out;
    }
}

// Example per-row usage in a UI layer:
//
// status = row.status;                  // Status enum (Pass/Fail/NA/Other/Unknown)
// comment = row.comment_text;           // string from comment field
// indicator = evaluate_indicator(status, comment);
//
// row.indicator_symbol  = indicator.symbol;
// row.indicator_style   = mapIndicatorClassToStyle(indicator.kind);
// row.indicator_tooltip = indicator.tooltip;
