#include <unordered_set>

#include "HAMT.hh"
#include "bench.hh"

template <typename Set> void benchmark() {
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

    Set set;

    int i = 0;
    auto stringsToAddCopy = stringsToAdd;
    auto iter = stringsToAddCopy.begin();
    benchmark("Random string insertion", stringsToAdd.size(), [&]() -> void {
        i++;
        set.insert(std::move(*iter));
        iter++;
    });

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string lookup", stringsNotToAdd.size(),
              [&]() -> void {
                  set.find(*iter);
                  iter++;
              });

    iter = stringsToAdd.begin();
    benchmark("Successful string lookup", stringsToAdd.size(), [&]() -> void {
        set.find(*iter);
        iter++;
    });

    std::random_shuffle(stringsToAdd.begin(), stringsToAdd.end());
    std::random_shuffle(stringsNotToAdd.begin(), stringsNotToAdd.end());

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string lookup (shuffled)", stringsNotToAdd.size(),
              [&]() -> void {
                  set.find(*iter);
                  iter++;
              });

    iter = stringsToAdd.begin();
    benchmark("Successful string lookup (shuffled)", stringsToAdd.size(),
              [&]() -> void {
                  set.find(*iter);
                  iter++;
              });

    iter = stringsNotToAdd.begin();
    benchmark("Unsuccessful string deletion (shuffled)",
              stringsNotToAdd.size() / 2, [&]() -> void {
                  set.erase(*iter);
                  iter++;
              });

    iter = stringsToAdd.begin();
    benchmark("Successful string deletion (shuffled)", stringsToAdd.size() / 2,
              [&]() -> void {
                  set.erase(*iter);
                  iter++;
              });
}

int main(void) {
    std::cout << "RANDOM STRING BENCHMARKS:\n\n";

    std::cout << "Testing HAMT:\n\n";
    benchmark<Hamt>();

    std::cout << "\n\nTesting std::unordered_set:\n\n";

    benchmark<std::unordered_set<std::string>>();

    return 0;
}
