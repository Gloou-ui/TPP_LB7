#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace text_analyzer {

class AnalyzerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class FileNotFoundError final : public AnalyzerError {
public:
    explicit FileNotFoundError(const std::filesystem::path& path);
};

class FileReadError final : public AnalyzerError {
public:
    FileReadError(const std::filesystem::path& path, std::string detail);
};

class EmptyWordSetError final : public AnalyzerError {
public:
    EmptyWordSetError();
};

class TextAnalyzer final {
public:
    using WordFrequency = std::pair<std::string, std::size_t>;

    TextAnalyzer() noexcept = default;
    TextAnalyzer(const TextAnalyzer&) = default;
    TextAnalyzer& operator=(const TextAnalyzer& other);
    TextAnalyzer(TextAnalyzer&& other) noexcept;
    TextAnalyzer& operator=(TextAnalyzer&& other) noexcept;

    void analyzeText(std::string_view text);

    void loadFile(const std::filesystem::path& path);

    [[nodiscard]] std::size_t wordCount() const noexcept;
    [[nodiscard]] std::size_t uniqueWordCount() const noexcept;
    [[nodiscard]] std::size_t frequencyOf(std::string_view word) const;
    [[nodiscard]] std::vector<WordFrequency> topWords(int limit) const;

private:
    void swap(TextAnalyzer& other) noexcept;

    std::map<std::string, std::size_t> frequencies_;
    std::size_t word_count_ = 0;
};

class ScopeLogger final {
public:
    explicit ScopeLogger(std::string name);
    ~ScopeLogger() noexcept;

    ScopeLogger(const ScopeLogger&) = delete;
    ScopeLogger& operator=(const ScopeLogger&) = delete;

private:
    std::string name_;
};

}
