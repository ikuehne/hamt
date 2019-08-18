#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

class HamtNode;
class Leaf;

static const std::uint64_t FIRST_6_BITS = (1 << 6) - 1;

//////////////////////////////////////////////////////////////////////////////
// Class prototypes.
//

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
    // would be shifted 6 bits to the right.
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
    Leaf *lookup(uint64_t hash) {
        auto thisNodeKey = hash & FIRST_6_BITS;
        bool hasChild = (map & (1ULL << thisNodeKey)) != 0;
        if (!hasChild) {
            return NULL;
        }
        // thisNodeKey has 6 bits, so this can't shift off all of map.
        // However, it does keep the bit that we just found, so we have to
        // subtract 1 from the count of set bits.
        std::uint64_t rest = map >> thisNodeKey;
        int idx = __builtin_popcountll((unsigned long long)rest) - 1;
        HamtNodePointer &next = children[idx];
        if (next.isChild()) {
            assert(!next.isNull());
            return next.getChild()->lookup(hash >> 6);
        } else {
            return next.getLeaf();
        }
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
                assert((otherHash & FIRST_6_BITS) == (hash & FIRST_6_BITS));
                HamtNode *newNode = new HamtNode();
                *nextNode = HamtNodePointer(newNode);
                newNode->insertLeaf(otherHash >> 6, oldLeaf);
                newNode->insert(hash >> 6, str);
            }
        }
    }

private:
    inline std::uint64_t getIndex(std::uint64_t firstBits) {
        std::uint64_t rest = map >> firstBits;
        return __builtin_popcountll((unsigned long long)rest);
    }

    inline std::vector<HamtNodePointer>::iterator
            findChildForInsert(uint64_t hash) {
        auto thisNodeKey = hash & FIRST_6_BITS;
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

class Hamt {
public:
    Hamt() : root(new HamtNode()), hasher() {}
    ~Hamt() {
        delete root;
    }

    void insert(std::string *str) {
        std::uint64_t hash = hasher(*str);
        root->insert(hash, str);
    }

    bool lookup(std::string *str) {
        std::uint64_t hash = hasher(*str);
        Leaf *leaf = root->lookup(hash);
        if (leaf == NULL) return false;

        for (const auto *i: leaf->getData()) {
            if (i->compare(*str) == 0) return true;
        }
        return false;
    }
private:
    HamtNode *root;
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
