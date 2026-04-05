#include <iostream>
#include <string>
#include <vector>

std::string get_name(const std::string* ptr) {
    return *ptr;
}

int compute(int x) {
    int result = x * multiplier;
    return result;
}

int find_max(std::vector<int>& v) {
    for (int i = 0; i < (int)v.size(); ++i) {
        if (v[i] > 100) return v[i];
    }
}

void print_message(const std::string& msg) {
    std::cout << msg << "\n";
}

int main() {
    std::vector<int> nums = {1, 2, 3};
    print_message(42);
    return 0;
}
