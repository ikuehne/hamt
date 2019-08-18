#include <algorithm>
#include <iostream>
#include <random>
#include <memory>
#include <unordered_set>

#include "HAMT.hh"

void die() {
    std::cerr << "Test failed!\n";
    exit(1);
}

static auto generator = std::mt19937();

std::unique_ptr<std::string>random_string()
{
    int length = generator() % 256;
    auto str = std::make_unique<std::string>(length,0);

    std::generate_n(str->begin(), length, generator);

    return str;
}

int main(void) {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::unique_ptr<std::string>> stringsToAdd;

    for (int i = 0; i < 20000; ++i) {
        auto str = random_string();
        setOfStringsToAdd.insert(*str);
        stringsToAdd.push_back(std::move(str));
    }

    std::vector<std::unique_ptr<std::string>> stringsNotToAdd;

    for (int i = 0; i < 20000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(*str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(std::move(str));
    }

    Hamt hamt;

    int i = 0;
    for (auto &str: stringsToAdd) {
        i += 1;
        hamt.insert(str.get());
    }

    i = 0;
    for (auto &str: stringsToAdd) {
        i += 1;
        if (!hamt.lookup(str.get())) die();
    }

    i = 0;
    for (auto &str: stringsNotToAdd) {
        i += 1;
        if (hamt.lookup(str.get())) die();
    }

    return 0;
}
