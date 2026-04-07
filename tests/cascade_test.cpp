/**
 * tests/cascade_test.cpp
 *
 * Tests codemedic's root cause analysis.
 * Missing <vector> include causes 8+ cascade errors.
 * Codemedic should identify 1 root cause, not 8 separate errors.
 *
 * Run: codemedic -y tests/cascade_test.cpp
 */

// Intentionally missing: #include <vector>
#include <iostream>

int main() {
    std::vector<int> numbers = {1, 2, 3, 4, 5};  // cascade errors start here

    for (auto& n : numbers) {
        std::cout << n << "\n";
    }

    std::vector<std::string> words = {"hello", "world"};
    words.push_back("foo");

    return 0;
}
