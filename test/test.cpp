#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_set>

#include "HAMT.hh"

void die() {
    std::cerr << "Test failed!\n";
    exit(1);
}

static auto generator = std::mt19937();

std::string *random_string()
{
    int length = generator() % 256;
    auto str = new std::string(length,0);

    std::generate_n(str->begin(), length, generator);

    return str;
}

int main(void) {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::string *> stringsToAdd;

    for (int i = 0; i < 2; ++i) {
        auto str = random_string();
        stringsToAdd.push_back(str);
        setOfStringsToAdd.insert(*str);
    }

    std::vector<std::string*> stringsNotToAdd;

    for (int i = 0; i < 20000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(*str) != setOfStringsToAdd.end()) {
            continue;
        }
        stringsNotToAdd.push_back(str);
    }

    Hamt hamt;

    int i = 0;
    for (auto str: stringsToAdd) {
        i += 1;
        hamt.insert(str);
    }

    i = 0;
    for (auto str: stringsToAdd) {
        i += 1;
        if (!hamt.lookup(str)) die();
    }

    i = 0;
    for (auto str: stringsNotToAdd) {
        i += 1;
        if (hamt.lookup(str)) die();
    }

    return 0;
}
