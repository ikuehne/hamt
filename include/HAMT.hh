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

class HamtNodePointer;
class Leaf;
class HamtNode;
class Hamt;

// A pointer, except that the low bit is used to test whether it is a pointer
// to a child node or a result.
class HamtNodePointer {
public:
    explicit HamtNodePointer(HamtNode *node)
        : ptr(reinterpret_cast<std::uintptr_t>(node)) {}

    // Initialize the pointer 
    explicit HamtNodePointer(Leaf *leaf)
        : ptr(reinterpret_cast<std::uintptr_t>(leaf) | 1) {}

    // Initialize the pointer to NULL.
    HamtNodePointer() : ptr(0) {}

    HamtNodePointer(HamtNodePointer &&other): ptr(other.ptr) {
        other.ptr = 0;
    }

    HamtNodePointer &operator=(HamtNodePointer &&other) {
        ptr = other.ptr;
        other.ptr = 0;
        return *this;
    }

    ~HamtNodePointer();

    inline bool isChild() {
        return !(ptr & 1);
    }

    inline bool isNull() {
        return ptr == 0;
    }

    inline HamtNode *getChild() {
        return reinterpret_cast<HamtNode *>(ptr);
    }

    inline Leaf *getLeaf() {
        return reinterpret_cast<Leaf *>(ptr & (~1));
    }

    bool lookup(uint64_t hash, std::string *str);

private:
    uintptr_t ptr;
};

class Leaf {
public:
    std::uint64_t getHash() {
        return hash;
    }

    void setHash(std::uint64_t newHash) {
        hash = newHash;
    }

    std::vector<std::string *> &getData() {
        return data;
    }

    explicit Leaf(std::uint64_t hash): hash(hash), data() {}

private:
    // The hash, shifted to reflect the level this leaf is at.
    //
    // For example, if this Leaf is one of the children of the root of the
    // HAMT, the full 64 bit hash would be here; if it was one level down, it
    // would be shifted BITS_PER_LEVEL bits to the right.
    std::uint64_t hash;
    std::vector<std::string *> data;
};

class HamtNode {
public:
    HamtNode() : map(0), children() {}

    // Look up `hash` in this trie.
    //
    // Return one of three things:
    //    - A NULL pointer indicating the key was not present.
    //    - A 
    bool lookup(uint64_t hash, std::string *str) {
        auto thisNodeKey = hash & FIRST_N_BITS;
        bool hasChild = (map & (1ULL << thisNodeKey)) != 0;

        if (!hasChild) {
            return false;
        }

        return children[getIndex(thisNodeKey) - 1].lookup(hash >> 6, str);
    }

    void insert(uint64_t hash, std::string *str) {
        auto nextNode = findChildForInsert(hash);

        // Case 1: we created a new node at this level.
        if (nextNode->isNull()) {
            // We're done, so create a new leaf and put it in the node.
            Leaf *leaf = new Leaf(hash);
            leaf->getData().push_back(str);
            *nextNode = HamtNodePointer(leaf);
        // Case 2: we need to go another level down the HAMT.
        } else if (nextNode->isChild()) {
            return nextNode->getChild()->insert(hash >> 6, str);
        // Case 3: we found a leaf already here. We need to replace it with a
        // new node, and put both leaves in that node.
        } else {
            Leaf *oldLeaf = nextNode->getLeaf();
            std::uint64_t otherHash = oldLeaf->getHash();
            if (otherHash == hash) {
                oldLeaf->getData().push_back(str);
            } else {
                assert((otherHash & FIRST_N_BITS) == (hash & FIRST_N_BITS));
                HamtNode *newNode = new HamtNode();
                *nextNode = HamtNodePointer(newNode);
                newNode->insertLeaf(otherHash >> 6, oldLeaf);
                newNode->insert(hash >> 6, str);
            }
        }
    }

    void insertLeaf(uint64_t hash, Leaf *leaf) {
        auto nextNode = findChildForInsert(hash);

        // Case 1: we created a new node at this level.
        if (nextNode->isNull()) {
            // We're done, so set the leaf's hash.
            leaf->setHash(hash);
            // Put the leaf in the new node.
            *nextNode = HamtNodePointer(leaf);
        // Case 2: we need to go another level down the HAMT.
        } else if (nextNode->isChild()) {
            return nextNode->getChild()->insertLeaf(hash >> 6, leaf);
        // Case 3: we found a leaf already here. We need to replace it with a
        // new node, and put both leaves in that node.
        } else {
            Leaf *oldLeaf = nextNode->getLeaf();
            std::uint64_t otherHash = oldLeaf->getHash();
            HamtNode *newNode = new HamtNode();
            *nextNode = HamtNodePointer(newNode);
            newNode->insertLeaf(otherHash >> 6, oldLeaf);
            newNode->insertLeaf(hash >> 6, leaf);
        }
    }

private:
    inline std::uint64_t getIndex(std::uint64_t firstBits) {
        std::uint64_t rest = map >> firstBits;
        return __builtin_popcountll((unsigned long long)rest);
    }

    inline std::vector<HamtNodePointer>::iterator
            findChildForInsert(uint64_t hash) {
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
            children.insert(children.begin() + idx, HamtNodePointer());
            return children.begin() + idx;
        }
    }

    // The map goes low bits to high bits. We'll pretend it's 4 bits instead
    // of 64 for examples. This map:
    // 1101
    // Has 0, 2 and 3 set. To get the index into `children` for 0, we right
    // shift by 1 to get `0110`, then count bits. To get it for 2, we right
    // shift by 3 to get `0001`, then count bits.
    std::uint64_t map;

    // Sorted from high to low bits. So if the first six bits of a key are the
    // *highest* of the keys stored at this node, it will be *first* in this
    // vector.
    std::vector<HamtNodePointer> children;
};

class TopLevelHamtNode {
public:
    inline void insert(uint64_t hash, std::string *str) {
        std::uint64_t thisNodeKey = hash & FIRST_N_BITS;
        auto nextNode = &table[thisNodeKey];

        if (nextNode->isNull()) {
            Leaf *leaf = new Leaf(hash);
            leaf->getData().push_back(str);
            *nextNode = HamtNodePointer(leaf);
        } else if (nextNode->isChild()) {
            return nextNode->getChild()->insert(hash >> 6, str);
        } else {
            Leaf *oldLeaf = nextNode->getLeaf();
            std::uint64_t otherHash = oldLeaf->getHash();
            if (otherHash == hash) {
                oldLeaf->getData().push_back(str);
            } else {
                assert((otherHash & FIRST_N_BITS) == (hash & FIRST_N_BITS));
                HamtNode *newNode = new HamtNode();
                *nextNode = HamtNodePointer(newNode);
                newNode->insertLeaf(otherHash >> 6, oldLeaf);
                newNode->insert(hash >> 6, str);
            }
        }
    }

    inline bool lookup(uint64_t hash, std::string *str) {
        return table[hash & FIRST_N_BITS].lookup(hash >> 6, str);
    }

private:
    HamtNodePointer table[MAX_IDX];
};

class Hamt {
public:
    Hamt() : root(), hasher() {}

    void insert(std::string *str) {
        std::uint64_t hash = hasher(*str);
        root.insert(hash, str);
    }

    bool lookup(std::string *str) {
        std::uint64_t hash = hasher(*str);
        return root.lookup(hash, str);
    }
private:
    TopLevelHamtNode root;
    std::hash<std::string> hasher;
};

HamtNodePointer::~HamtNodePointer() {
    if (isNull()) {
        return;
    } else if (isChild()) {
        delete getChild();
    } else {
        delete getLeaf();
    }
}

inline bool HamtNodePointer::lookup(uint64_t hash, std::string *str) {
    if (isNull()) {
        return false;
    } else if (isChild()) {
        return getChild()->lookup(hash, str);
    } else {
        Leaf *leaf = getLeaf();

        for (const auto *i: leaf->getData()) {
            if (*i == *str) return true;
        }

        return false;
    }
}

