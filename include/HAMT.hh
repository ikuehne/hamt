#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

// The number of bits we use to index into each level of the trie.
static const std::uint64_t BITS_PER_LEVEL = 6;

// A mask to take those bits off.
static const std::uint64_t FIRST_N_BITS = (1ULL << BITS_PER_LEVEL) - 1;

// (Exclusive) maximum value we can index a node with.
static const std::uint64_t MAX_IDX = 1ULL << BITS_PER_LEVEL;

static_assert(MAX_IDX <= 64,
              "2^MAX_IDX - 1 must fit within a 64-bit word");

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

    inline bool isChild();

    inline bool isNull();

    inline HamtNode *getChild();

    inline HamtLeaf *getLeaf();

    void insert(uint64_t hash, std::string *str);

    void insertLeaf(uint64_t hash, HamtLeaf *leaf);

    bool lookup(uint64_t hash, std::string *str);

private:
    uintptr_t ptr;
};

class HamtLeaf {
public:
    std::uint64_t getHash();

    void setHash(std::uint64_t newHash);

    std::vector<std::string *> &getData();

    explicit HamtLeaf(std::uint64_t hash);

private:
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

    // Look up `hash` in this trie.
    //
    // Return one of three things:
    //    - A NULL pointer indicating the key was not present.
    //    - A 
    bool lookup(uint64_t hash, std::string *str);

    void insert(uint64_t hash, std::string *str);

    void insertLeaf(uint64_t hash, HamtLeaf *leaf);

private:
    inline std::uint64_t getIndex(std::uint64_t firstBits);

    inline std::vector<HamtNodeEntry>::iterator
            findChildForInsert(uint64_t hash);

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
    std::vector<HamtNodeEntry> children;
};

class TopLevelHamtNode {
public:
    inline void insert(uint64_t hash, std::string *str);

    inline bool lookup(uint64_t hash, std::string *str);

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

//////////////////////////////////////////////////////////////////////////////
// HamtNodeEntry method definitions.
//

HamtNodeEntry::HamtNodeEntry(HamtNode *node)
    : ptr(reinterpret_cast<std::uintptr_t>(node)) {}

// Initialize the pointer 
HamtNodeEntry::HamtNodeEntry(HamtLeaf *leaf)
    : ptr(reinterpret_cast<std::uintptr_t>(leaf) | 1) {}

// Initialize the pointer to NULL.
HamtNodeEntry::HamtNodeEntry() : ptr(0) {}

HamtNodeEntry::HamtNodeEntry(HamtNodeEntry &&other): ptr(other.ptr) {
    other.ptr = 0;
}

HamtNodeEntry &HamtNodeEntry::operator=(HamtNodeEntry &&other) {
    ptr = other.ptr;
    other.ptr = 0;
    return *this;
}

inline bool HamtNodeEntry::isChild() {
    return !(ptr & 1);
}

inline bool HamtNodeEntry::isNull() {
    return ptr == 0;
}

inline HamtNode *HamtNodeEntry::getChild() {
    return reinterpret_cast<HamtNode *>(ptr);
}

inline HamtLeaf *HamtNodeEntry::getLeaf() {
    return reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

HamtNodeEntry::~HamtNodeEntry() {
    if (isNull()) {
        return;
    } else if (isChild()) {
        delete getChild();
    } else {
        delete getLeaf();
    }
}

inline bool HamtNodeEntry::lookup(uint64_t hash, std::string *str) {
    if (isNull()) {
        return false;
    } else if (isChild()) {
        return getChild()->lookup(hash >> 6, str);
    } else {
        HamtLeaf *leaf = getLeaf();

        for (const auto *i: leaf->getData()) {
            if (*i == *str) return true;
        }

        return false;
    }
}

inline void HamtNodeEntry::insert(uint64_t hash, std::string *str) {
    if (isNull()) {
        HamtLeaf *leaf = new HamtLeaf(hash);
        leaf->getData().push_back(str);
        *this = HamtNodeEntry(leaf);
    } else if (isChild()) {
        getChild()->insert(hash >> 6, str);
    } else {
        HamtLeaf *oldLeaf = getLeaf();
        std::uint64_t otherHash = oldLeaf->getHash();
        if (hash == otherHash) {
            oldLeaf->getData().push_back(str);
        } else {
            assert((otherHash & FIRST_N_BITS) == (hash & FIRST_N_BITS));
            HamtNode *newNode = new HamtNode();
            *this = HamtNodeEntry(newNode);
            newNode->insertLeaf(otherHash >> 6, oldLeaf);
            newNode->insert(hash >> 6, str);
        }
    }
}

inline void HamtNodeEntry::insertLeaf(uint64_t hash, HamtLeaf *leaf) {
    // Case 1: we created a new node at this level.
    if (isNull()) {
        // We're done, so set the leaf's hash.
        leaf->setHash(hash);
        // Put the leaf in the new node.
        *this = HamtNodeEntry(leaf);
    // Case 2: we need to go another level down the HAMT.
    } else if (isChild()) {
        return getChild()->insertLeaf(hash >> 6, leaf);
    // Case 3: we found a leaf already here. We need to replace it with a
    // new node, and put both leaves in that node.
    } else {
        HamtLeaf *oldLeaf = getLeaf();
        std::uint64_t otherHash = oldLeaf->getHash();
        HamtNode *newNode = new HamtNode();
        *this = HamtNodeEntry(newNode);
        newNode->insertLeaf(otherHash >> 6, oldLeaf);
        newNode->insertLeaf(hash >> 6, leaf);
    }
}

//////////////////////////////////////////////////////////////////////////////
// HamtLeaf method definitions.
//

std::uint64_t HamtLeaf::getHash() {
    return hash;
}

void HamtLeaf::setHash(std::uint64_t newHash) {
    hash = newHash;
}

std::vector<std::string *> &HamtLeaf::getData() {
    return data;
}

HamtLeaf::HamtLeaf(std::uint64_t hash): hash(hash), data() {}

//////////////////////////////////////////////////////////////////////////////
// HamtNode method definitions.
//

HamtNode::HamtNode() : map(0), children() {}

// Look up `hash` in this trie.
//
// Return one of three things:
//    - A NULL pointer indicating the key was not present.
//    - A 
bool HamtNode::lookup(uint64_t hash, std::string *str) {
    auto thisNodeKey = hash & FIRST_N_BITS;
    bool hasChild = (map & (1ULL << thisNodeKey)) != 0;

    if (!hasChild) {
        return false;
    }

    return children[getIndex(thisNodeKey) - 1].lookup(hash, str);
}

void HamtNode::insert(uint64_t hash, std::string *str) {
    findChildForInsert(hash)->insert(hash, str);
}

void HamtNode::insertLeaf(uint64_t hash, HamtLeaf *leaf) {
    findChildForInsert(hash)->insertLeaf(hash, leaf);
}

inline std::uint64_t HamtNode::getIndex(std::uint64_t firstBits) {
    std::uint64_t rest = map >> firstBits;
    return __builtin_popcountll((unsigned long long)rest);
}

inline std::vector<HamtNodeEntry>::iterator
        HamtNode::findChildForInsert(uint64_t hash) {
    auto thisNodeKey = hash & FIRST_N_BITS;
    bool hasChild = (map & (1ULL << thisNodeKey)) != 0;
    int idx = getIndex(thisNodeKey);

    if (hasChild) {
        idx -= 1;
        return children.begin() + idx;
    } else {
        // We need to add a new child. Set the bit in the map:
        map |= (1ULL << thisNodeKey);
        // And stick it in its expected position.
        children.insert(children.begin() + idx, HamtNodeEntry());
        return children.begin() + idx;
    }
}

//////////////////////////////////////////////////////////////////////////////
// TopLevelHamtNode method definitions.
//

inline void TopLevelHamtNode::insert(uint64_t hash, std::string *str) {
    table[hash & FIRST_N_BITS].insert(hash, str);
}

inline bool TopLevelHamtNode::lookup(uint64_t hash, std::string *str) {
    return table[hash & FIRST_N_BITS].lookup(hash, str);
}

//////////////////////////////////////////////////////////////////////////////
// Hamt method definitions.
//

void Hamt::insert(std::string *str) {
    std::uint64_t hash = hasher(*str);
    root.insert(hash, str);
}

bool Hamt::lookup(std::string *str) {
    std::uint64_t hash = hasher(*str);
    return root.lookup(hash, str);
}
