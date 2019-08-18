#include <algorithm>
#include <chrono>
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

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();
        setOfStringsToAdd.insert(*str);
        stringsToAdd.push_back(std::move(str));
    }

    std::vector<std::unique_ptr<std::string>> stringsNotToAdd;

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(*str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(std::move(str));
    }

    Hamt hamt;

    auto clock = std::chrono::steady_clock();
    auto start = clock.now();
    int i = 0;
    for (auto &str: stringsToAdd) {
        i += 1;
        hamt.insert(str.get());
    }
    auto end = clock.now();
    std::chrono::duration<double> diff = end-start;

    std::cout << "Insertion time: " << diff.count() << " s.\n";
    std::cout << "(Per insertion): " << 1'000'000 * diff.count()
                                                  / stringsToAdd.size()
              << " us.\n";

    start = clock.now();
    i = 0;
    for (auto &str: stringsToAdd) {
        i += 1;
        if (!hamt.lookup(str.get())) die();
    }
    end = clock.now();

    diff = end-start;

    std::cout << "Successful lookup time: " << diff.count() << " s.\n";
    std::cout << "(Per lookup): " << 1'000'000 * diff.count()
                                               / stringsToAdd.size()
              << " us.\n";

    start = clock.now();
    i = 0;
    for (auto &str: stringsNotToAdd) {
        i += 1;
        if (hamt.lookup(str.get())) die();
    }
    end = clock.now();
    diff = end-start;

    std::cout << "Unsuccessful lookup time: " << diff.count() << " s.\n";
    std::cout << "(Per lookup): " << 1'000'000 * diff.count()
                                               / stringsToAdd.size()
              << " us.\n";

    return 0;
}
