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
//
// Always one of three things:
//  - A pointer to a child node.
//  - A pointer to a leaf node.
//  - NULL, indicating there is nothing at this entry.
//
// Each entry fits in a single pointer; the first two cases are distinguished
// using the pointer's low bit.
//
class HamtNodeEntry {
public:
    explicit HamtNodeEntry(HamtNode *node);

    // Initialize the pointer 
    explicit HamtNodeEntry(HamtLeaf *leaf);

    // Initialize the pointer to NULL.
    HamtNodeEntry();

    // Move another entry into this one.
    //
    // The other entry will be set to NULL.
    HamtNodeEntry(HamtNodeEntry &&other);
    HamtNodeEntry &operator=(HamtNodeEntry &&other);

    // Entries delete whatever they point to, including recursively freeing a
    // subtree.
    ~HamtNodeEntry();

    // Test whether this entry points to a leaf node.
    bool isLeaf();

    // Test whether this entry is NULL.
    bool isNull();

    // Get a pointer to the child node.
    //
    // isLeaf() and isNull() must both be false.
    HamtNode *getChild();

    // Get a pointer to the leaf.
    //
    // isLeaf() must be true.
    HamtLeaf *getLeaf();

private:
    // 0 for NULL. The low bit is set if this points to a leaf.
    // Otherwise, it points to a node.
    uintptr_t ptr;
};

// A leaf node.
//
// Stores a vector of keys, each of which must have the same hash.
// Additionally stores a shifted hash to avoid recomputing the hash on inserts;
// see below.
class HamtLeaf {
public:
    // Construct a new HamtLeaf at the given hash.
    //
    // See below for the nature of the hash.
    explicit HamtLeaf(std::uint64_t hash);

    // The hash, shifted to reflect the level this leaf is at.
    //
    // For example, if this HamtLeaf is one of the children of the root of the
    // HAMT, the full 64 bit hash would be here; if it was one level down, it
    // would be shifted BITS_PER_LEVEL bits to the right.
    std::uint64_t hash;

    // The keys stored at this node, in the order they were inserted.
    std::vector<std::string> data;
};

// A node containing a sub-table.
//
// Guaranteed always to have at least one child.
class HamtNode {
public:
    // Efficiently get the number of children of this node.
    int numberOfChildren();

    // Get the number of child hashes greater than or equal to the given hash,
    // looking only at the first BITS_PER_LEVEL bits.
    //
    // For example, if BITS_PER_LEVEL is 2, and we have hashes 00, 10, and 11
    // already in this node, numberOfHashesAbove(00) would be 2, and
    // numberOfHashesAbove(10) would also be 2.
    std::uint64_t numberOfHashesAbove(std::uint64_t hash);

    // Efficiently test if the hash is in this node, looking only at the first
    // BITS_PER_LEVEL bits.
    bool containsHash(std::uint64_t hash);

    void markHash(std::uint64_t hash);

    // This would create an invalid node, since the entries are always
    // guaranteed nonnull.
    HamtNode() = delete;

    // Don't use new and delete for these; they are too special.
    //
    // We exclusively use `malloc` and `realloc`, because of the
    // variable-length `children` member.
    void operator delete(void *p) = delete;
    void *operator new(size_t size) = delete;

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
    //
    //
    HamtNodeEntry children[1];

private:

};

// The distinguished top-level node.
//
// Just a table of MAX_IDX HamtNodeEntrys. The top node is likely to fill up
// pretty quickly anyway, so we spare the space, and this way avoid a bit of
// fiddling with the bitmap.
class TopLevelHamtNode {
public:
    void insert(uint64_t hash, std::string &&str);

    bool lookup(uint64_t hash, const std::string &str);

private:
    HamtNodeEntry table[MAX_IDX];
};

// The HAMT itself. Users should only use this interface.
class Hamt {
public:
    // Initialize an empty HAMT.
    Hamt() = default;

    // Insert a string into the set.
    void insert(std::string &&str);

    // Lookup a string in the set.
    bool lookup(const std::string &str);
private:
    TopLevelHamtNode root;
    std::hash<std::string> hasher;
};
