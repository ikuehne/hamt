#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <memory>
#include <unordered_set>
#include <set>

#include "HAMT.hh"

void die() {
    std::cerr << "Test failed!\n";
    exit(1);
}

static auto generator = std::mt19937();
using seconds = std::chrono::duration<double>;
using nanoseconds = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;

std::string random_string()
{
    int length = generator() % 256;
    std::string str(length,0);

    std::generate_n(str.begin(), length, generator);

    return str;
}

void benchmark(std::string name, int nIterations, std::function<void(void)> op)
{
    auto clock = std::chrono::steady_clock();
    auto start = clock.now();
    for (int i = 0; i < nIterations; ++i) {
        op();
    }
    auto end = clock.now();
    auto diff = (end-start);
    double total = seconds(diff).count();
    double perOp = nanoseconds(diff).count() / nIterations;

    std::cout << name << ":\n";
    std::cout << "    Total time: " << total << " s.\n";
    std::cout << "    Per operation: " << perOp << " ns.\n";
}

void benchmarkHamt() {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::string> stringsToAdd;

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();

        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        setOfStringsToAdd.insert(str);
        stringsToAdd.push_back(str);
    }

    std::vector<std::string> stringsNotToAdd;

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(str);
    }

    Hamt hamt;

    auto stringsToAddCopy = stringsToAdd;
    auto iter = stringsToAddCopy.begin();
    benchmark("Random string insertion", stringsToAdd.size(), [&]() -> void {
        hamt.insert(std::move(*iter));
        iter++;
    });

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string lookup", stringsNotToAdd.size(),
              [&]() -> void {
        hamt.lookup(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("Successful string lookup", stringsToAdd.size(), [&]() -> void {
        hamt.lookup(*iter);
        iter++;
    });

    std::random_shuffle(stringsToAdd.begin(),    stringsToAdd.end());
    std::random_shuffle(stringsNotToAdd.begin(), stringsNotToAdd.end());

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string lookup (shuffled)",
              stringsNotToAdd.size(),
              [&]() -> void {
        hamt.lookup(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("Successful string lookup (shuffled)",
              stringsToAdd.size(), [&]() -> void {
        hamt.lookup(*iter);
        iter++;
    });

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string deletion (shuffled)",
              stringsNotToAdd.size() / 2, [&]() -> void {
        hamt.remove(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("Successful string deletion (shuffled)",
              stringsToAdd.size() / 2, [&]() -> void {
        hamt.remove(*iter);
        iter++;
    });
}

void benchmarkMap() {
    std::unordered_set<std::string> setOfStringsToAdd;
    std::vector<std::string> stringsToAdd;

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();

        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        setOfStringsToAdd.insert(str);
        stringsToAdd.push_back(str);
    }

    std::vector<std::string> stringsNotToAdd;

    for (int i = 0; i < 1000000; ++i) {
        auto str = random_string();
        if (setOfStringsToAdd.find(str) != setOfStringsToAdd.end()) {
            continue;
        }

        stringsNotToAdd.push_back(str);
    }

    std::unordered_set<std::string> set;

    auto stringsToAddCopy = stringsToAdd;
    auto iter = stringsToAddCopy.begin();
    benchmark("(std::unordered_set) Random string insertion", stringsToAdd.size(), [&]() -> void {
        set.insert(std::move(*iter));
        iter++;
    });

    iter = stringsNotToAdd.begin();
    benchmark("(std::unordered_set) Unsuccessful string lookup", stringsNotToAdd.size(),
              [&]() -> void {
        set.find(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("(std::unordered_set) Successful string lookup", stringsToAdd.size(), [&]() -> void {
        set.find(*iter);
        iter++;
    });

    std::random_shuffle(stringsToAdd.begin(),    stringsToAdd.end());
    std::random_shuffle(stringsNotToAdd.begin(), stringsNotToAdd.end());

    iter = stringsNotToAdd.begin();
    benchmark("(std::unordered_set) Unsuccessful string lookup (shuffled)",
              stringsNotToAdd.size(),
              [&]() -> void {
        set.find(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("(std::unordered_set) Successful string lookup (shuffled)",
              stringsToAdd.size(), [&]() -> void {
        set.find(*iter);
        iter++;
    });

    iter = stringsNotToAdd.begin();
    benchmark("(std::unordered_set) Unsuccessful string deletion (shuffled)",
              stringsNotToAdd.size() / 2, [&]() -> void {
        set.erase(*iter);
        iter++;
    });

    iter = stringsToAdd.begin();
    benchmark("(std::unordered_set) Successful string deletion (shuffled)",
              stringsToAdd.size() / 2, [&]() -> void {
        set.erase(*iter);
        iter++;
    });
}

int main(void) {
    std::cout << "Testing HAMT:\n\n";
    benchmarkHamt();

    std::cout << "\n\nTesting std::unordered_set:\n\n";

    benchmarkMap();

    return 0;
}
