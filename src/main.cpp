#include "TextAnalyzer.hpp"

#include <exception>
#include <filesystem>
#include <iostream>

namespace {

void readAtLeaf(text_analyzer::TextAnalyzer& analyzer,
                const std::filesystem::path& path) {
    text_analyzer::ScopeLogger logger("readAtLeaf");
    analyzer.loadFile(path);
}

void loadWithDiagnostics(text_analyzer::TextAnalyzer& analyzer,
                         const std::filesystem::path& path) {
    text_analyzer::ScopeLogger logger("loadWithDiagnostics");
    try {
        readAtLeaf(analyzer, path);
    } catch (const text_analyzer::FileNotFoundError& error) {
        std::cout << "Діагностика проміжного рівня: " << error.what() << '\n';
        throw;
    } catch (const text_analyzer::FileReadError& error) {
        std::cout << "Діагностика проміжного рівня: " << error.what() << '\n';
        throw;
    } catch (const text_analyzer::AnalyzerError& error) {
        std::cout << "Діагностика проміжного рівня: " << error.what() << '\n';
        throw;
    } catch (const std::exception& error) {
        std::cout << "Діагностика проміжного рівня: " << error.what() << '\n';
        throw;
    }
}

void loadDocument(text_analyzer::TextAnalyzer& analyzer,
                  const std::filesystem::path& path) {
    text_analyzer::ScopeLogger logger("loadDocument");
    loadWithDiagnostics(analyzer, path);
}

}  // namespace

int main(int argc, char* argv[]) {
    text_analyzer::ScopeLogger logger("main");
    text_analyzer::TextAnalyzer analyzer;
    analyzer.analyzeText("Initial statistics: one two two.");
    std::cout << "Початкова кількість слів: " << analyzer.wordCount() << '\n';

    const std::filesystem::path path =
        argc > 1 ? argv[1] : "__text_analyzer_demo_missing_file__.txt";
    try {
        loadDocument(analyzer, path);
        std::cout << "Файл успішно завантажено: " << path.string() << '\n';
    } catch (const text_analyzer::FileNotFoundError& error) {
        std::cout << "Файл відсутній: " << error.what() << '\n';
    } catch (const text_analyzer::FileReadError& error) {
        std::cout << "Помилка читання: " << error.what() << '\n';
    } catch (const text_analyzer::EmptyWordSetError& error) {
        std::cout << "Немає слів для аналізу: " << error.what() << '\n';
    } catch (const text_analyzer::AnalyzerError& error) {
        std::cout << "Помилка аналізатора: " << error.what() << '\n';
    } catch (const std::invalid_argument& error) {
        std::cout << "Некоректні дані: " << error.what() << '\n';
    } catch (const std::exception& error) {
        std::cout << "Непередбачена помилка: " << error.what() << '\n';
    }

    std::cout << "Після операції кількість слів: " << analyzer.wordCount()
              << '\n';
    return 0;
}
