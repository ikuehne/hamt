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

std::string random_string()
{
    int length = generator() % 256;
    auto str = std::string(length,0);

    std::generate_n(str.begin(), length, generator);

    return str;
}

int main(void) {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::string> stringsToAdd;

    for (int i = 0; i < 20000; ++i) {
        auto str = random_string();
        setOfStringsToAdd.insert(str);
        stringsToAdd.push_back(str);
    }

    std::vector<std::string> stringsNotToAdd;

    for (int i = 0; i < 20000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(str);
    }

    Hamt hamt;

    std::vector<std::string> stringsToAddCopy = stringsToAdd;
    int i = 0;
    for (auto str: stringsToAddCopy) {
        i += 1;
        hamt.insert(std::move(str));
    }

    i = 0;
    for (const auto &str: stringsToAdd) {
        i += 1;
        if (!hamt.lookup(str)) die();
    }

    i = 0;
    for (const auto &str: stringsNotToAdd) {
        i += 1;
        if (hamt.lookup(str)) die();
    }

    return 0;
}
