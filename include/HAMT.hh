#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// Constants.
//

// The number of bits we use to index into each level of the trie.
const std::uint64_t BITS_PER_LEVEL = 6;

// A mask to take those bits off.
const std::uint64_t FIRST_N_BITS = (1ULL << BITS_PER_LEVEL) - 1;

// (Exclusive) maximum value we can index a node with.
const std::uint64_t MAX_IDX = 1ULL << BITS_PER_LEVEL;

static_assert(MAX_IDX <= 64, "2^MAX_IDX - 1 must fit within a 64-bit word");


//////////////////////////////////////////////////////////////////////////////
// Class prototypes.
//

class HamtNodeEntry;
class HamtLeaf;
class HamtNode;
class Hamt;

// An entry in one of the tables at each node of the trie.
class HamtNodeEntry {
public:
    explicit HamtNodeEntry(HamtNode *node);

    // Initialize the pointer 
    explicit HamtNodeEntry(HamtLeaf *leaf);

    // Initialize the pointer to NULL.
    HamtNodeEntry();

    HamtNodeEntry(HamtNodeEntry &&other);

    HamtNodeEntry &operator=(HamtNodeEntry &&other);

    ~HamtNodeEntry();

    bool isChild();

    bool isNull();

    HamtNode *getChild();

    HamtLeaf *getLeaf();

private:
    uintptr_t ptr;
};

class HamtLeaf {
public:
    explicit HamtLeaf(std::uint64_t hash);

    // The hash, shifted to reflect the level this leaf is at.
    //
    // For example, if this HamtLeaf is one of the children of the root of the
    // HAMT, the full 64 bit hash would be here; if it was one level down, it
    // would be shifted BITS_PER_LEVEL bits to the right.
    std::uint64_t hash;
    std::vector<std::string *> data;
};

class HamtNode {
public:
    HamtNode();

    // Don't use new and delete for these; they are too special.
    //
    // We exclusively use `malloc` and `realloc`, because of the
    // variable-length `children` member
    void operator delete(void *p) = delete;
    void *operator new(size_t size) = delete;

    // Look up `hash` in this trie.
    //
    // Return one of three things:
    //    - A NULL pointer indicating the key was not present.
    //    - A 
    bool lookup(uint64_t hash, std::string *str);

    void insert(uint64_t hash, std::string *str);

    void insertLeaf(uint64_t hash, HamtLeaf *leaf);

    // The map goes low bits to high bits. We'll pretend it's 4 bits instead
    // of 64 for examples. The map `1101` has 0, 2 and 3 set.
    //
    // For index computations, we'd *want* to shift by (i + 1) and count bits,
    // but that might be one more bit than we are allowed to shift. Thus, we
    // also check if the bit we're checking is set; if it is, we subtract one
    // from the count.
    //
    // With the above bitmap as an example, to get the index into `children`
    // for 0, we right shift by 0 to get `1101`, count that 3 bits are set,
    // and then subtract 1 since we see that the 0th (lowest) bit is set. To
    // get the index for 1, we right shift by 1 to get `110`, count the bits
    // to get 2, and don't subtract 1, since the bit is currently unset.
    std::uint64_t map;

    // Sorted from high to low bits. So if the first six bits of a key are the
    // *highest* of the keys stored at this node, it will be *first* in this
    // vector.
    //
    // This is in contiguous memory in the struct for cache reasons. There's
    // always at least 1 element; the number allocated is always equal to the
    // number of bits set in `map`.
    HamtNodeEntry children[1];

    std::uint64_t getIndex(std::uint64_t firstBits);

    int numberOfChildren();

private:
    std::vector<HamtNodeEntry>::iterator findChildForInsert(uint64_t hash);
};

class TopLevelHamtNode {
public:
    void insert(uint64_t hash, std::string *str);

    bool lookup(uint64_t hash, std::string *str);

private:
    HamtNodeEntry table[MAX_IDX];
};

class Hamt {
public:
    // Initialize an empty HAMT.
    Hamt() = default;

    // Insert a string into the set.
    void insert(std::string *str);

    // Lookup a string in the set.
    bool lookup(std::string *str);
private:
    TopLevelHamtNode root;
    std::hash<std::string> hasher;
};
