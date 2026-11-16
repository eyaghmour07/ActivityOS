#include "activityos/classification.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace activityos {
namespace detail {

// Defined in the source generated from resources/classification/defaults.rules.
std::string_view defaultClassificationRulesText();

} // namespace detail

namespace {

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string_view trim(std::string_view value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::vector<std::string> splitWords(std::string_view value) {
    std::vector<std::string> words;
    std::string current;
    for (const char raw : value) {
        const auto c = static_cast<unsigned char>(raw);
        // Bytes of multi-byte UTF-8 characters stay inside the word.
        if (c >= 0x80 || std::isalnum(c)) {
            current += raw;
        } else if (!current.empty()) {
            words.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(std::move(current));
    return words;
}

// An application name in the forms the match modes compare against.
struct PreparedName {
    std::string lowered;            // lowercase
    std::string normalized;         // lowercase, without a trailing ".exe"
    std::vector<std::string> words; // normalized, split on non-alphanumeric characters
};

PreparedName prepareName(std::string_view application) {
    PreparedName name;
    name.lowered = lower(application);
    name.normalized = name.lowered;
    if (name.normalized.size() > 4 && name.normalized.ends_with(".exe")) {
        name.normalized.resize(name.normalized.size() - 4);
    }
    name.words = splitWords(name.normalized);
    return name;
}

std::vector<std::string_view> splitFields(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto separator = line.find('|');
        fields.push_back(trim(line.substr(0, separator)));
        if (separator == std::string_view::npos) break;
        line.remove_prefix(separator + 1);
    }
    return fields;
}

bool applicationMatches(NameMatch match,
                        const PreparedName& name,
                        const std::string& pattern,
                        const std::vector<std::string>& pattern_words) {
    switch (match) {
    case NameMatch::contains:
        return name.lowered.find(pattern) != std::string::npos;
    case NameMatch::exact:
        return name.normalized == pattern;
    case NameMatch::words:
        return !pattern_words.empty() &&
               std::search(name.words.begin(), name.words.end(), pattern_words.begin(),
                           pattern_words.end()) != name.words.end();
    }
    return false;
}

[[noreturn]] void failLine(std::size_t line_number, const std::string& reason) {
    throw std::invalid_argument("classification rules line " + std::to_string(line_number) +
                                ": " + reason);
}

} // namespace

ClassificationRuleSet::ClassificationRuleSet(std::vector<ClassifierRule> rules)
    : rules_(std::move(rules)) {
    prepared_.reserve(rules_.size());
    for (const auto& rule : rules_) {
        PreparedPattern pattern;
        pattern.application = lower(rule.application_pattern);
        pattern.application_words = splitWords(pattern.application);
        pattern.title = lower(rule.title_pattern);
        prepared_.push_back(std::move(pattern));
    }
}

ClassificationRuleSet ClassificationRuleSet::parse(std::string_view text) {
    std::vector<ClassifierRule> rules;
    std::size_t line_number = 0;
    while (!text.empty()) {
        const auto line_end = text.find('\n');
        const auto line = trim(text.substr(0, line_end));
        text.remove_prefix(line_end == std::string_view::npos ? text.size() : line_end + 1);
        ++line_number;
        if (line.empty() || line.front() == '#') continue;

        const auto fields = splitFields(line);
        if (fields.size() != 4) {
            failLine(line_number, "expected \"category | flags | match | application name\"");
        }
        ClassifierRule rule;
        rule.category = std::string(fields[0]);
        if (rule.category.empty()) failLine(line_number, "the category is empty");

        if (fields[1] == "distraction") {
            rule.is_distraction = true;
        } else if (fields[1] == "browser") {
            rule.is_browser = true;
        } else if (fields[1] != "-") {
            failLine(line_number, "unknown flags \"" + std::string(fields[1]) + "\"");
        }

        if (fields[2] == "exact") {
            rule.application_match = NameMatch::exact;
        } else if (fields[2] == "words") {
            rule.application_match = NameMatch::words;
        } else {
            failLine(line_number, "unknown match \"" + std::string(fields[2]) + "\"");
        }

        rule.application_pattern = std::string(fields[3]);
        if (splitWords(rule.application_pattern).empty()) {
            failLine(line_number, "the application name has no letters or digits");
        }
        rules.push_back(std::move(rule));
    }
    return ClassificationRuleSet(std::move(rules));
}

const ClassificationRuleSet& ClassificationRuleSet::builtIn() {
    static const ClassificationRuleSet rules = parse(detail::defaultClassificationRulesText());
    return rules;
}

const ClassifierRule* ClassificationRuleSet::firstMatch(std::string_view application,
                                                        std::string_view title) const {
    const auto name = prepareName(application);
    const auto loweredTitle = lower(title);
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        const auto& pattern = prepared_[i];
        if (applicationMatches(rules_[i].application_match, name, pattern.application,
                               pattern.application_words) &&
            loweredTitle.find(pattern.title) != std::string::npos) {
            return &rules_[i];
        }
    }
    return nullptr;
}

bool ClassificationRuleSet::isBrowser(std::string_view application) const {
    const auto name = prepareName(application);
    for (std::size_t i = 0; i < rules_.size(); ++i) {
        if (rules_[i].is_browser &&
            applicationMatches(rules_[i].application_match, name, prepared_[i].application,
                               prepared_[i].application_words)) {
            return true;
        }
    }
    return false;
}

const std::vector<ClassifierRule>& ClassificationRuleSet::rules() const noexcept {
    return rules_;
}

} // namespace activityos
