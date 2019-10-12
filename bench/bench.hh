#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <random>

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
    std::cout << "    Per operation: " << (unsigned)perOp << " ns.\n";
}
