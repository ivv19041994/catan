#pragma once

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace test {

class Failure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void Check(bool condition, const std::string& message) {
    if (!condition) throw Failure(message);
}

template<class A, class B>
void Equal(const A& actual, const B& expected, const std::string& message) {
    if (!(actual == expected)) throw Failure(message);
}

template<class F>
void Throws(F&& f, const std::string& message) {
    try { std::forward<F>(f)(); }
    catch (const std::exception&) { return; }
    throw Failure(message);
}

using Case = std::pair<const char*, std::function<void()>>;

inline int Run(const std::vector<Case>& cases) {
    int failures = 0;
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    for (const auto& [name, body] : cases) {
        try {
            body();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& e) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << e.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": non-standard exception\n";
        }
    }
    return failures == 0 ? 0 : 1;
}

} // namespace test
