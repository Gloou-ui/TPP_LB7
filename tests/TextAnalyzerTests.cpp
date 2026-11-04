#include "TextAnalyzer.hpp"
#include "PolynomialSolver.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace {

class TemporaryTextFile final {
public:
    explicit TemporaryTextFile(const std::string& contents) {
        static std::atomic<unsigned long long> sequence{0};
        path_ = std::filesystem::temp_directory_path() /
                ("text_analyzer_test_" +
                 std::to_string(sequence.fetch_add(1)) + ".txt");
        std::ofstream output(path_, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Could not create temporary test file.");
        }
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        if (!output) {
            throw std::runtime_error("Could not write temporary test file.");
        }
    }

    ~TemporaryTextFile() noexcept {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void expectStatistics(const text_analyzer::TextAnalyzer& analyzer,
                      std::size_t words, std::size_t unique_words,
                      std::size_t alpha_frequency, std::size_t beta_frequency) {
    EXPECT_EQ(analyzer.wordCount(), words);
    EXPECT_EQ(analyzer.uniqueWordCount(), unique_words);
    EXPECT_EQ(analyzer.frequencyOf("alpha"), alpha_frequency);
    EXPECT_EQ(analyzer.frequencyOf("beta"), beta_frequency);
}

}  // namespace

TEST(TextAnalyzerTest, CountsWordsAndNormalizesLatinAndUkrainianCase) {
    text_analyzer::TextAnalyzer analyzer;

    analyzer.analyzeText("Кіт кІТ, Ґанок ґанок, HELLO hello!");

    EXPECT_EQ(analyzer.wordCount(), 6U);
    EXPECT_EQ(analyzer.uniqueWordCount(), 3U);
    EXPECT_EQ(analyzer.frequencyOf("КІТ"), 2U);
    EXPECT_EQ(analyzer.frequencyOf("ҐАНОК"), 2U);
    EXPECT_EQ(analyzer.frequencyOf("hello"), 2U);
}

TEST(TextAnalyzerTest, LoadsFileAndReplacesPreviousStatistics) {
    TemporaryTextFile file("alpha beta beta");
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("old words");

    analyzer.loadFile(file.path());

    EXPECT_EQ(analyzer.wordCount(), 3U);
    EXPECT_EQ(analyzer.uniqueWordCount(), 2U);
    EXPECT_EQ(analyzer.frequencyOf("beta"), 2U);
    EXPECT_EQ(analyzer.frequencyOf("old"), 0U);
}

TEST(TextAnalyzerTest, MissingFileThrowsAndPreservesPreviousStatistics) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta beta");
    const auto missing = std::filesystem::temp_directory_path() /
                         "text_analyzer_file_that_does_not_exist_14.txt";
    std::error_code error;
    std::filesystem::remove(missing, error);

    EXPECT_THROW(analyzer.loadFile(missing),
                 text_analyzer::FileNotFoundError);

    expectStatistics(analyzer, 3U, 2U, 1U, 2U);
}

TEST(TextAnalyzerTest, DirectoryReadFailurePreservesPreviousStatistics) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta");

    EXPECT_THROW(
        analyzer.loadFile(std::filesystem::temp_directory_path()),
        text_analyzer::FileReadError);

    expectStatistics(analyzer, 2U, 2U, 1U, 1U);
}

TEST(TextAnalyzerTest, InvalidUtf8PreservesPreviousStatistics) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta");
    const std::string invalid_utf8("\xC3", 1);

    EXPECT_THROW(analyzer.analyzeText(invalid_utf8), std::invalid_argument);

    expectStatistics(analyzer, 2U, 2U, 1U, 1U);
}

TEST(TextAnalyzerTest, EmptyTextIsValidButTopWordsReportsEmptySet) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("");

    EXPECT_EQ(analyzer.wordCount(), 0U);
    EXPECT_EQ(analyzer.uniqueWordCount(), 0U);
    EXPECT_THROW((void)analyzer.topWords(1), text_analyzer::EmptyWordSetError);
}

TEST(TextAnalyzerTest, TopWordsRejectsNonPositiveLimits) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta");

    EXPECT_THROW((void)analyzer.topWords(0), std::invalid_argument);
    EXPECT_THROW((void)analyzer.topWords(-1), std::invalid_argument);
}

TEST(TextAnalyzerTest, TopWordsAreFrequencyOrderedAndAlphabeticalOnTies) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("zeta beta beta alpha alpha");

    const auto top = analyzer.topWords(2);

    ASSERT_EQ(top.size(), 2U);
    EXPECT_EQ(top[0], (text_analyzer::TextAnalyzer::WordFrequency{"alpha", 2U}));
    EXPECT_EQ(top[1], (text_analyzer::TextAnalyzer::WordFrequency{"beta", 2U}));
    EXPECT_EQ(analyzer.topWords(100).size(), 3U);
}

TEST(TextAnalyzerTest, SearchRequiresExactlyOneWord) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta");

    EXPECT_THROW((void)analyzer.frequencyOf(""), std::invalid_argument);
    EXPECT_THROW((void)analyzer.frequencyOf("alpha beta"),
                 std::invalid_argument);
    EXPECT_THROW((void)analyzer.frequencyOf("---"), std::invalid_argument);
}

TEST(TextAnalyzerTest, SearchForValidButUnknownWordReturnsZero) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta");

    EXPECT_EQ(analyzer.frequencyOf("unknown"), 0U);
}

TEST(TextAnalyzerTest, CopyAndMoveOperationsPreserveObjectInvariants) {
    text_analyzer::TextAnalyzer source;
    source.analyzeText("alpha beta beta");

    text_analyzer::TextAnalyzer copy;
    copy = source;
    expectStatistics(copy, 3U, 2U, 1U, 2U);

    text_analyzer::TextAnalyzer moved(std::move(source));
    expectStatistics(moved, 3U, 2U, 1U, 2U);
    EXPECT_EQ(source.wordCount(), 0U);
    EXPECT_EQ(source.uniqueWordCount(), 0U);

    text_analyzer::TextAnalyzer destination;
    destination.analyzeText("old state");
    destination = std::move(moved);
    expectStatistics(destination, 3U, 2U, 1U, 2U);
    EXPECT_EQ(moved.wordCount(), 0U);
    EXPECT_EQ(moved.uniqueWordCount(), 0U);
}

TEST(TextAnalyzerTest, FileReadFailureLeavesAllStatisticsUnchanged) {
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("alpha beta beta");

    EXPECT_THROW(
        analyzer.loadFile(std::filesystem::temp_directory_path()),
        text_analyzer::FileReadError);

    expectStatistics(analyzer, 3U, 2U, 1U, 2U);
}

TEST(PolynomialSolverTest, EvaluatesDerivativeAndNewtonRoot) {
    const double cubic[]{-6.0, 11.0, -6.0, 1.0};
    double value = 0.0;
    ASSERT_EQ(polynomial_evaluate(cubic, 4, 2.0, &value),
              POLYNOMIAL_STATUS_OK);
    EXPECT_NEAR(value, 0.0, 1e-10);

    double derivative[3]{};
    std::size_t derivative_count = 0;
    ASSERT_EQ(polynomial_derivative(cubic, 4, derivative, 3,
                                    &derivative_count),
              POLYNOMIAL_STATUS_OK);
    ASSERT_EQ(derivative_count, 3U);
    ASSERT_EQ(polynomial_evaluate(derivative, derivative_count, 2.0, &value),
              POLYNOMIAL_STATUS_OK);
    EXPECT_NEAR(value, -1.0, 1e-10);

    double root = 0.0;
    ASSERT_EQ(polynomial_newton_root(cubic, 4, 2.8, 1e-10, 100, &root),
              POLYNOMIAL_STATUS_OK);
    EXPECT_NEAR(root, 3.0, 1e-8);
}

TEST(PolynomialSolverTest, FindsRealRootsAndReportsRequiredCapacity) {
    const double cubic[]{-6.0, 11.0, -6.0, 1.0};
    double roots[3]{};
    std::size_t root_count = 0;

    ASSERT_EQ(polynomial_find_roots(cubic, 4, 1e-10, roots, 3, &root_count),
              POLYNOMIAL_STATUS_OK);
    ASSERT_EQ(root_count, 3U);
    EXPECT_NEAR(roots[0], 1.0, 1e-8);
    EXPECT_NEAR(roots[1], 2.0, 1e-8);
    EXPECT_NEAR(roots[2], 3.0, 1e-8);

    double small_buffer[2]{};
    EXPECT_EQ(polynomial_find_roots(cubic, 4, 1e-10, small_buffer, 2,
                                    &root_count),
              POLYNOMIAL_STATUS_BUFFER_TOO_SMALL);
    EXPECT_EQ(root_count, 3U);
}

TEST(PolynomialSolverTest, RejectsInvalidArgumentsWithoutCrossingCAbi) {
    double result = 0.0;
    EXPECT_EQ(polynomial_evaluate(nullptr, 0, 1.0, &result),
              POLYNOMIAL_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(polynomial_evaluate(nullptr, 0, 1.0, nullptr),
              POLYNOMIAL_STATUS_INVALID_ARGUMENT);

    const double polynomial[]{1.0, 1.0};
    EXPECT_EQ(polynomial_newton_root(polynomial, 2, 0.0, 1e-8, 0, &result),
              POLYNOMIAL_STATUS_INVALID_ARGUMENT);
}
