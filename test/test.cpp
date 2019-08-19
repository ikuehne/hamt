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

    for (int i = 0; i < 2000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }
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
    for (auto str: stringsToAddCopy) {
        hamt.insert(std::move(str));
    }

    std::random_shuffle(stringsToAdd.begin(), stringsToAdd.end());

    for (const auto &str: stringsToAdd) {
        if (!hamt.lookup(str)) die();
    }

    for (const auto &str: stringsNotToAdd) {
        if (hamt.lookup(str)) die();
        if (hamt.remove(str)) die();
    }

    std::random_shuffle(stringsToAdd.begin(), stringsToAdd.end());

    for (auto str = stringsToAdd.begin();
         str < stringsToAdd.begin() + stringsToAdd.size() / 2;
         str++) {
        if (!hamt.remove(*str)) die();
    }

    for (auto str = stringsToAdd.begin();
         str < stringsToAdd.begin() + stringsToAdd.size() / 2;
         str++) {
        if (hamt.lookup(*str)) die();
    }

    for (auto str = stringsToAdd.begin() + stringsToAdd.size() / 2;
         str < stringsToAdd.end();
         str++) {
        if (!hamt.lookup(*str)) die();
    }

    return 0;
}
