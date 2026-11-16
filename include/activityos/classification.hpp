#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace activityos {

// How a rule's application pattern is compared with an application name.
// Comparison ignores case; `exact` and `words` also ignore a trailing ".exe".
enum class NameMatch {
    contains, // the pattern appears anywhere in the name; an empty pattern matches any name
    exact,    // the whole name equals the pattern
    words,    // the pattern's words appear in the name as whole, consecutive words
};

struct ClassifierRule {
    std::string application_pattern;
    NameMatch application_match{NameMatch::contains};
    std::string title_pattern; // found anywhere in the window title; empty matches any title
    std::string category;
    bool is_distraction{false};
    bool is_browser{false};
};

// An ordered list of classification rules. Built-in defaults and user rules
// are both matched through this class, so they behave the same way.
class ClassificationRuleSet {
public:
    ClassificationRuleSet() = default;
    explicit ClassificationRuleSet(std::vector<ClassifierRule> rules);

    // Parses the format documented in resources/classification/defaults.rules.
    // Throws std::invalid_argument naming the line that could not be read.
    static ClassificationRuleSet parse(std::string_view text);
    // The defaults embedded from resources/classification/defaults.rules.
    static const ClassificationRuleSet& builtIn();

    // The first rule, in order, that matches both the application and the title.
    // The pointer stays valid for as long as this rule set does.
    [[nodiscard]] const ClassifierRule* firstMatch(std::string_view application,
                                                   std::string_view title = {}) const;
    // True when any rule flagged as a browser matches the application.
    [[nodiscard]] bool isBrowser(std::string_view application) const;
    [[nodiscard]] const std::vector<ClassifierRule>& rules() const noexcept;

private:
    struct PreparedPattern {
        std::string application;
        std::vector<std::string> application_words;
        std::string title;
    };

    std::vector<ClassifierRule> rules_;
    std::vector<PreparedPattern> prepared_;
};

} // namespace activityos
