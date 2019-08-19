#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "HAMT.hh"

// We do some sketchy memory stuff that GCC doesn't like. Disable that
// warning.
#ifdef __GNUC__
#ifndef __clang__
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
// TopLevelHamtNode method definitions.
//

void TopLevelHamtNode::insert(uint64_t hash, std::string *str) {
    HamtNodeEntry *entryToInsert = &table[hash & FIRST_N_BITS];

    if (entryToInsert->isNull()) {
        HamtLeaf *leaf = new HamtLeaf(hash);
        leaf->data.push_back(str);
        *entryToInsert = HamtNodeEntry(leaf);
        return;
    }

    while (true) {
        if (!entryToInsert->isLeaf()) {
            hash >>= BITS_PER_LEVEL;
            auto nodeToInsert = entryToInsert->getChild();
            int idx = nodeToInsert->numberOfHashesAbove(hash);

            if (nodeToInsert->containsHash(hash)) {
                entryToInsert = &nodeToInsert->children[idx - 1];
                continue;
            } else {
                std::uint64_t nChildren = nodeToInsert->numberOfChildren() + 1;
                std::uint64_t nExtraBytes = (nChildren - 1)
                                          * sizeof(HamtNodeEntry);
                // Say the old node had 5 entries and we're inserting a new
                // one at index 2. That is:
                // Old node:
                //
                //     [A] [B] [C] [D] [E]
                //
                // New node, with X the new entry:
                //
                //     [A] [B] [X] [C] [D] [E]
                //
                // So nChildren is 6, and nExtraBytes is 5*sizeof..., giving
                // space for 6 children.
                //
                // We need to mmove everything from address 2 to address 4 in
                // the old one; that is, 3 entries, or in general:
                //
                //     nChildren - idx - 1
                //

                HamtNode *newNode =
                    (HamtNode *)realloc(nodeToInsert, sizeof(HamtNode) + nExtraBytes);
                // We need to add a new child. Set the bit in the map:
                newNode->markHash(hash);
                // Move the entries in the array after where we're going to
                // insert one over. Needless to say, sketch as hell.
                std::memmove(&newNode->children[idx + 1],
                             &newNode->children[idx],
                             (nChildren - idx - 1) * sizeof(HamtNodeEntry));

                HamtLeaf *leaf = new HamtLeaf(hash);
                leaf->data.push_back(str);
                newNode->children[idx] = HamtNodeEntry(leaf);
                *entryToInsert = HamtNodeEntry(newNode);
                return;
            }
        } else {
            HamtLeaf *otherLeaf = entryToInsert->getLeaf();
            assert((hash & FIRST_N_BITS) == (otherLeaf->hash & FIRST_N_BITS));
            std::uint64_t nextKey = (hash >> BITS_PER_LEVEL) & FIRST_N_BITS;
            std::uint64_t otherNextKey = (otherLeaf->hash >> BITS_PER_LEVEL)
                                       & FIRST_N_BITS;

            if (hash == otherLeaf->hash) {
                otherLeaf->data.push_back(str);
                return;

            } else if (nextKey > otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)malloc(sizeof(HamtNode)
                                                     + sizeof(HamtNodeEntry));
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                HamtLeaf *leaf = new HamtLeaf(hash);
                leaf->data.push_back(str);

                newNode->children[0] = HamtNodeEntry(leaf);
                newNode->children[1] = HamtNodeEntry(otherLeaf);

                *entryToInsert = HamtNodeEntry(newNode);

                return;
            } else if (nextKey < otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)malloc(sizeof(HamtNode)
                                                     + sizeof(HamtNodeEntry));
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                HamtLeaf *leaf = new HamtLeaf(hash);
                leaf->data.push_back(str);

                newNode->children[0] = HamtNodeEntry(otherLeaf);
                newNode->children[1] = HamtNodeEntry(leaf);

                *entryToInsert = HamtNodeEntry(newNode);
                return;
            } else {
                otherLeaf->hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)malloc(sizeof(HamtNode));
                newNode->map = 0;
                newNode->markHash(otherLeaf->hash);
                newNode->children[0] = HamtNodeEntry(otherLeaf);
                *entryToInsert = HamtNodeEntry(newNode);

                continue;
            }
        }
    }
}

bool TopLevelHamtNode::lookup(uint64_t hash, std::string *str) {
    HamtNodeEntry *entry = &table[hash & FIRST_N_BITS];

    if (entry->isNull()) return false;

    while (true) {
        if (!entry->isLeaf()) {
            hash >>= BITS_PER_LEVEL;
            HamtNode *node = entry->getChild();

            if (!node->containsHash(hash)) {
                return false;
            }

            entry = &node->children[node->numberOfHashesAbove(hash) - 1];
            continue;
        } else {
            HamtLeaf *leaf = entry->getLeaf();

            for (const auto *i: leaf->data) {
                if (*i == *str) return true;
            }

            return false;
        }
    }
}

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

bool HamtNodeEntry::isLeaf() {
    return ptr & 1;
}

bool HamtNodeEntry::isNull() {
    return ptr == 0;
}

HamtNode *HamtNodeEntry::getChild() {
    assert(!isNull() && !isLeaf());
    return reinterpret_cast<HamtNode *>(ptr);
}

HamtLeaf *HamtNodeEntry::getLeaf() {
    assert(isLeaf());
    return reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

HamtNodeEntry::~HamtNodeEntry() {
    if (isNull()) {
        return;
    } else if (!isLeaf()) {
        // This gets a bit tricky. We need to free the tree rooted at this
        // node, but we can't call `delete` because we allocated this child
        // with `malloc`. So...
        HamtNode *child = getChild();
        int nChildren = child->numberOfChildren();
        for (int i = 0; i < nChildren; ++i) {
            child->children[i].~HamtNodeEntry();
        }
        free(getChild());
    } else {
        delete getLeaf();
    }
}

//////////////////////////////////////////////////////////////////////////////
// HamtLeaf method definitions.
//

HamtLeaf::HamtLeaf(std::uint64_t hash): hash(hash), data() {}

//////////////////////////////////////////////////////////////////////////////
// HamtNode method definitions.
//

int HamtNode::numberOfChildren() {
    return __builtin_popcountll((unsigned long long)map);
}

std::uint64_t HamtNode::numberOfHashesAbove(std::uint64_t hash) {
    std::uint64_t rest = map >> (hash & FIRST_N_BITS);
    return __builtin_popcountll((unsigned long long)rest);
}

bool HamtNode::containsHash(std::uint64_t hash) {
    return (map & (1ULL << (hash & FIRST_N_BITS))) != 0;
}

void HamtNode::markHash(std::uint64_t hash) {
    map |= (1ULL << (hash & FIRST_N_BITS));
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

// Re-enable the warning we disabled at the start.
// warning.
#ifdef __GNUC__
#ifndef __clang__
    #pragma GCC diagnostic warning "-Wclass-memaccess"
#endif
#endif
