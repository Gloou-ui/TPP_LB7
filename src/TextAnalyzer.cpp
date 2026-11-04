#include "TextAnalyzer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <system_error>

namespace text_analyzer {
namespace {

struct Statistics {
    std::map<std::string, std::size_t> frequencies;
    std::size_t word_count = 0;
};

bool isContinuation(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

bool decodeUtf8(std::string_view text, std::size_t offset,
                std::uint32_t& code_point, std::size_t& length) noexcept {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80U) {
        code_point = first;
        length = 1;
        return true;
    }

    if (first >= 0xC2U && first <= 0xDFU) {
        length = 2;
        if (offset + length > text.size()) {
            return false;
        }
        const auto second = static_cast<unsigned char>(text[offset + 1]);
        if (!isContinuation(second)) {
            return false;
        }
        code_point = ((first & 0x1FU) << 6U) | (second & 0x3FU);
        return true;
    }

    if (first >= 0xE0U && first <= 0xEFU) {
        length = 3;
        if (offset + length > text.size()) {
            return false;
        }
        const auto second = static_cast<unsigned char>(text[offset + 1]);
        const auto third = static_cast<unsigned char>(text[offset + 2]);
        if (!isContinuation(second) || !isContinuation(third) ||
            (first == 0xE0U && second < 0xA0U) ||
            (first == 0xEDU && second >= 0xA0U)) {
            return false;
        }
        code_point = ((first & 0x0FU) << 12U) |
                     ((second & 0x3FU) << 6U) | (third & 0x3FU);
        return true;
    }

    if (first >= 0xF0U && first <= 0xF4U) {
        length = 4;
        if (offset + length > text.size()) {
            return false;
        }
        const auto second = static_cast<unsigned char>(text[offset + 1]);
        const auto third = static_cast<unsigned char>(text[offset + 2]);
        const auto fourth = static_cast<unsigned char>(text[offset + 3]);
        if (!isContinuation(second) || !isContinuation(third) ||
            !isContinuation(fourth) ||
            (first == 0xF0U && second < 0x90U) ||
            (first == 0xF4U && second >= 0x90U)) {
            return false;
        }
        code_point = ((first & 0x07U) << 18U) |
                     ((second & 0x3FU) << 12U) |
                     ((third & 0x3FU) << 6U) | (fourth & 0x3FU);
        return true;
    }

    return false;
}

void appendUtf8(std::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7FU) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
        output.push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
        output.push_back(
            static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
        output.push_back(
            static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
}

std::uint32_t toLower(std::uint32_t code_point) noexcept {
    if (code_point >= 'A' && code_point <= 'Z') {
        return code_point + ('a' - 'A');
    }
    if (code_point >= 0x0400U && code_point <= 0x040FU) {
        return code_point + 0x50U;
    }
    if (code_point >= 0x0410U && code_point <= 0x042FU) {
        return code_point + 0x20U;
    }
    if (code_point == 0x0490U) {
        return 0x0491U;
    }
    if ((code_point >= 0x00C0U && code_point <= 0x00D6U) ||
        (code_point >= 0x00D8U && code_point <= 0x00DEU)) {
        return code_point + 0x20U;
    }
    return code_point;
}

bool isWordCodePoint(std::uint32_t code_point) noexcept {
    return (code_point >= 'a' && code_point <= 'z') ||
           (code_point >= 'A' && code_point <= 'Z') ||
           (code_point >= '0' && code_point <= '9') ||
           (code_point >= 0x00C0U && code_point <= 0x00D6U) ||
           (code_point >= 0x00D8U && code_point <= 0x00F6U) ||
           (code_point >= 0x00F8U && code_point <= 0x00FFU) ||
           (code_point >= 0x0300U && code_point <= 0x036FU) ||
           (code_point >= 0x0400U && code_point <= 0x052FU);
}

Statistics buildStatistics(std::string_view text) {
    Statistics result;
    std::string current_word;

    const auto saveWord = [&]() {
        if (current_word.empty()) {
            return;
        }
        if (result.word_count == std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error("The total word count overflowed.");
        }

        auto [entry, inserted] = result.frequencies.try_emplace(current_word, 0);
        (void)inserted;
        if (entry->second == std::numeric_limits<std::size_t>::max()) {
            throw std::overflow_error("A word frequency overflowed.");
        }
        ++entry->second;
        ++result.word_count;
        current_word.clear();
    };

    for (std::size_t offset = 0; offset < text.size();) {
        std::uint32_t code_point = 0;
        std::size_t length = 0;
        if (!decodeUtf8(text, offset, code_point, length)) {
            throw std::invalid_argument("Text must contain valid UTF-8.");
        }

        if (isWordCodePoint(code_point) &&
            !(code_point >= 0x0300U && code_point <= 0x036FU &&
              current_word.empty())) {
            appendUtf8(current_word, toLower(code_point));
        } else {
            saveWord();
        }
        offset += length;
    }
    saveWord();
    return result;
}

std::string normalizeSearchWord(std::string_view word) {
    if (word.empty()) {
        throw std::invalid_argument("Search word must not be empty.");
    }

    std::string normalized;
    for (std::size_t offset = 0; offset < word.size();) {
        std::uint32_t code_point = 0;
        std::size_t length = 0;
        if (!decodeUtf8(word, offset, code_point, length) ||
            !isWordCodePoint(code_point) ||
            (code_point >= 0x0300U && code_point <= 0x036FU &&
             normalized.empty())) {
            throw std::invalid_argument(
                "Search parameter must be exactly one UTF-8 word.");
        }
        appendUtf8(normalized, toLower(code_point));
        offset += length;
    }
    return normalized;
}

std::string pathForMessage(const std::filesystem::path& path) {
    return path.string();
}

}

FileNotFoundError::FileNotFoundError(const std::filesystem::path& path)
    : AnalyzerError("Text file does not exist: " + pathForMessage(path)) {}

FileReadError::FileReadError(const std::filesystem::path& path,
                             std::string detail)
    : AnalyzerError("Cannot read text file '" + pathForMessage(path) +
                    "': " + std::move(detail)) {}

EmptyWordSetError::EmptyWordSetError()
    : AnalyzerError("Cannot select top words: no words have been analyzed.") {}

TextAnalyzer& TextAnalyzer::operator=(const TextAnalyzer& other) {
    if (this != &other) {
        TextAnalyzer replacement(other);
        swap(replacement);
    }
    return *this;
}

TextAnalyzer::TextAnalyzer(TextAnalyzer&& other) noexcept {
    swap(other);
}

TextAnalyzer& TextAnalyzer::operator=(TextAnalyzer&& other) noexcept {
    if (this != &other) {
        TextAnalyzer replacement(std::move(other));
        swap(replacement);
    }
    return *this;
}

void TextAnalyzer::analyzeText(std::string_view text) {
    Statistics replacement = buildStatistics(text);
    frequencies_.swap(replacement.frequencies);
    std::swap(word_count_, replacement.word_count);
}

void TextAnalyzer::loadFile(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw FileReadError(path, error.message());
    }
    if (!exists) {
        throw FileNotFoundError(path);
    }

    const bool is_directory = std::filesystem::is_directory(path, error);
    if (error) {
        throw FileReadError(path, error.message());
    }
    if (is_directory) {
        throw FileReadError(path, "path refers to a directory");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw FileReadError(path, "file could not be opened");
    }

    const std::string contents{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) {
        throw FileReadError(path, "I/O error while reading");
    }

    analyzeText(contents);
}

std::size_t TextAnalyzer::wordCount() const noexcept {
    return word_count_;
}

std::size_t TextAnalyzer::uniqueWordCount() const noexcept {
    return frequencies_.size();
}

std::size_t TextAnalyzer::frequencyOf(std::string_view word) const {
    const std::string normalized = normalizeSearchWord(word);
    const auto entry = frequencies_.find(normalized);
    return entry == frequencies_.end() ? 0 : entry->second;
}

std::vector<TextAnalyzer::WordFrequency> TextAnalyzer::topWords(int limit) const {
    if (limit <= 0) {
        throw std::invalid_argument("topWords limit must be greater than zero.");
    }
    if (frequencies_.empty()) {
        throw EmptyWordSetError();
    }

    std::vector<WordFrequency> result(frequencies_.begin(), frequencies_.end());
    std::sort(result.begin(), result.end(),
              [](const WordFrequency& left, const WordFrequency& right) {
                  if (left.second != right.second) {
                      return left.second > right.second;
                  }
                  return left.first < right.first;
              });
    if (result.size() > static_cast<std::size_t>(limit)) {
        result.resize(static_cast<std::size_t>(limit));
    }
    return result;
}

void TextAnalyzer::swap(TextAnalyzer& other) noexcept {
    frequencies_.swap(other.frequencies_);
    std::swap(word_count_, other.word_count_);
}

ScopeLogger::ScopeLogger(std::string name) : name_(std::move(name)) {
    std::cout << "ScopeLogger: створено " << name_ << std::endl;
}

ScopeLogger::~ScopeLogger() noexcept {
    try {
        std::cout << "ScopeLogger: знищено " << name_ << std::endl;
    } catch (...) {
    }
}

}
