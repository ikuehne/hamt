#include <iostream>

#include "HAMT.hh"

void die() {
    std::cerr << "Test failed!\n";
    exit(1);
}

int main(void) {
    std::string test1 = "hello";
    std::string test2 = "hell";
    std::string test3 = "hellggjrkdn";
    std::string test4 = "garbrudzken";
    std::string test5 = "";

    Hamt hamt;

    hamt.insert(&test1);
    hamt.insert(&test2);
    hamt.insert(&test3);
    hamt.insert(&test4);
    hamt.insert(&test5);

    std::string test6 = "hello";

    if (!hamt.lookup(&test1)) die();
    if (!hamt.lookup(&test2)) die();
    if (!hamt.lookup(&test3)) die();
    if (!hamt.lookup(&test4)) die();
    if (!hamt.lookup(&test5)) die();
    if (!hamt.lookup(&test6)) die();

    std::string test7 = "ajklde";

    if (hamt.lookup(&test7)) die();

    std::cerr << "Test succeeded!\n";

    return 0;
}
