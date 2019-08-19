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

void runTest(int size) {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::string> stringsToAdd;

    for (int i = 0; i < size; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }
        setOfStringsToAdd.insert(str);
        stringsToAdd.push_back(str);
    }

    std::vector<std::string> stringsNotToAdd;

    for (int i = 0; i < size; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(str);
    }

    Hamt hamt;
    int i = 0;

    std::vector<std::string> stringsToAddCopy = stringsToAdd;
    for (auto str: stringsToAddCopy) {
        i++;
        hamt.insert(std::move(str));
    }

    i = 0;
    std::random_shuffle(stringsToAdd.begin(), stringsToAdd.end());

    for (const auto &str: stringsToAdd) {
        i++;
        if (!hamt.lookup(str)) die();
    }

    i = 0;
    for (const auto &str: stringsNotToAdd) {
        i++;
        if (hamt.lookup(str)) die();
        if (hamt.remove(str)) die();
    }

    i = 0;
    std::random_shuffle(stringsToAdd.begin(), stringsToAdd.end());

    for (auto str = stringsToAdd.begin();
         str < stringsToAdd.begin() + stringsToAdd.size() / 2;
         str++) {
        i++;
        if (!hamt.remove(*str)) die();
    }

    i = 0;
    for (auto str = stringsToAdd.begin();
         str < stringsToAdd.begin() + stringsToAdd.size() / 2;
         str++) {
        i++;
        if (hamt.lookup(*str)) die();
    }

    i = 0;
    for (auto str = stringsToAdd.begin() + stringsToAdd.size() / 2;
         str < stringsToAdd.end();
         str++) {
        i++;
        if (!hamt.lookup(*str)) die();
    }
}

int main(void) {
    runTest(2);
    runTest(10);
    runTest(100);
    runTest(1000);
    runTest(10000);
    runTest(100000);
    return 0;
}
