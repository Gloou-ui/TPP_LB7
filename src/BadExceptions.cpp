#include <exception>
#include <iostream>
#include <string>

template <typename Throw>
void demonstrate(const char* label, Throw throw_value) {
    std::cout << "Перевірка винятку типу " << label << ":\n";
    try {
        throw_value();
    } catch (const std::exception&) {
        std::cout << "Перехоплено як std::exception.\n";
    } catch (...) {
        std::cout << "std::exception не перехопив; виняток перехопив catch(...).\n";
    }
}

int main() {
    demonstrate("int", [] { throw 42; });
    demonstrate("std::string", [] { throw std::string("помилка"); });
}
