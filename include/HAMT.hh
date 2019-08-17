#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

class HamtNode;
class Leaf;

static const std::uint64_t FIRST_6_BITS = (1 << 6) - 1;

// A pointer, except that the low bit is used to test whether it is a pointer
// to a child node or a result.
class HamtNodePointer {
public:
    HamtNodePointer(HamtNode *node)
        : ptr(reinterpret_cast<std::uintptr_t>(node))
    {}

    HamtNodePointer(void *data)
        : ptr(reinterpret_cast<std::uintptr_t>(data) | 1)
    {}

    HamtNodePointer()
        : ptr(0)
    {}

    inline bool isChild() {
        return !(ptr & 1);
    }

    inline bool isNull() {
        return ptr == 0;
    }

    inline HamtNode *getChild() {
        return reinterpret_cast<HamtNode *>(ptr);
    }

    inline void setChild(HamtNode *child) {
        ptr = reinterpret_cast<uintptr_t>(child);
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

    Leaf(std::uint64_t hash): hash(hash), data() {}

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
        HamtNodePointer next = children[idx];
        if (next.isChild()) {
            return next.getChild()->lookup(hash >> 6);
        } else {
            return next.getLeaf();
        }
    }

    void insert(uint64_t hash, std::string *str) {
        auto thisNodeKey = hash & FIRST_6_BITS;
        bool hasChild = map & (1ULL << thisNodeKey);
        std::uint64_t rest = map >> thisNodeKey;
        int idx = __builtin_popcountll((unsigned long long)rest);

        if (hasChild) {
            HamtNodePointer next = children[idx].getChild();
            if (next.isChild()) {
                next.getChild()->insert(hash >> 6, str);
            } else {
                Leaf *oldLeaf = next.getLeaf();
                std::uint64_t otherHash = oldLeaf->getHash();
                if (otherHash == hash) {
                    oldLeaf->getData().push_back(str);
                }
                HamtNode *newNode = new HamtNode();
                next.setChild(newNode);
                newNode->insertLeaf(otherHash >> 6, oldLeaf);
                newNode->insert(hash >> 6, str);
            }
        } else {
            // We need to add a new child. Set the bit in the map:
            map |= (1ULL << thisNodeKey);
            Leaf *leaf = new Leaf(hash);
            // Add the string to the new vector.
            leaf->getData().push_back(str);
            // And stick it in its expected position.
            children.insert(children.begin() + idx, HamtNodePointer(leaf));
        }
    }

private:
    void insertLeaf(uint64_t hash, Leaf *leaf) {
        auto thisNodeKey = hash & FIRST_6_BITS;
        bool hasChild = map & (1 << thisNodeKey);
        std::uint64_t rest = map >> thisNodeKey;
        int idx = __builtin_popcountll((unsigned long long)rest);

        if (hasChild) {
            HamtNodePointer next = children[idx].getChild();
            if (next.isChild()) {
                return next.getChild()->insertLeaf(hash >> 6, leaf);
            } else {
                Leaf *oldLeaf = next.getLeaf();
                std::uint64_t otherHash = oldLeaf->getHash();
                HamtNode *newNode = new HamtNode();
                next.setChild(newNode);
                newNode->insertLeaf(otherHash >> 6, oldLeaf);
                newNode->insertLeaf(hash >> 6, leaf);
            }
        } else {
            // We need to add a new child. Set the bit in the map:
            map |= (1 << thisNodeKey);
            leaf->setHash(hash);
            // And stick it in its expected position.
            children.insert(children.begin() + idx, HamtNodePointer(leaf));
        }
    }
    // The map goes low bits to high bits. We'll pretend it's 4 bits instead
    // of 64 for examples. This map:
    // 1101
    // Has 0, 2 and 3 set. To get the index into `children` for 0, we right
    // shift by 1 to get `0110`, then count bits. To get it for 2, we right
    // shift by 3 to get `0001`, then count bits.
    std::uint64_t map;
    std::vector<HamtNodePointer> children;
};

class Hamt {
public:
    Hamt() : root(new HamtNode()), hasher() {}

    void insert(std::string *str) {
        std::uint64_t hash = hasher(*str);
        root->insert(hash, str);
    }

    bool lookup(std::string *str) {
        std::uint64_t hash = hasher(*str);
        Leaf *leaf = root->lookup(hash);
        if (leaf == NULL) return false;

        for (const auto *i: leaf->getData()) {
            if (*i == *str) {
                return true;
            } else {
                return false;
            }
        }
        assert(false);
    }
private:
    HamtNode *root;
    std::hash<std::string> hasher;
};
