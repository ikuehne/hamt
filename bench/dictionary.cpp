#include <fstream>
#include <unordered_set>

#include "HAMT.hh"
#include "bench.hh"

std::vector<std::string> readDictionary() {
    std::ifstream dict("/usr/share/dict/american-english");
    std::string word;
    std::vector<std::string> result;

    while (getline(dict, word)) {
        result.push_back(std::move(word));
    }

    std::random_shuffle(result.begin(), result.end());

    return result;
}

template <typename Set> void benchmark(const std::vector<std::string> &dict) {
    Set set;

    auto dictCopy = dict;
    auto iter = dictCopy.begin();

    benchmark("Word insertion", dictCopy.size(), [&]() -> void {
        set.insert(std::move(*iter));
        iter++;
    });

    dictCopy = dict;
    iter = dictCopy.begin();
    benchmark("Word lookup", dictCopy.size(), [&]() -> void {
        set.find(*iter);
        iter++;
    });

    std::random_shuffle(dictCopy.begin(), dictCopy.end());
    iter = dictCopy.begin();
    benchmark("Word lookup (shuffled)", dictCopy.size(), [&]() -> void {
        set.find(*iter);
        iter++;
    });

    std::random_shuffle(dictCopy.begin(), dictCopy.end());
    iter = dictCopy.begin();
    benchmark("Word deletion", dictCopy.size(), [&]() -> void {
        set.erase(*iter);
        iter++;
    });
}

int main(void) {
    std::cout << "ENGLISH DICTIONARY BENCHMARKS:\n\n";

    auto dict = readDictionary();

    std::cout << "Testing HAMT:\n\n";
    benchmark<Hamt>(dict);

    std::cout << "\n\nTesting std::unordered_set:\n\n";
    benchmark<std::unordered_set<std::string>>(dict);

    return 0;
}
