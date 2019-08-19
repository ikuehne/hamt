#include <algorithm>
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

void TopLevelHamtNode::insert(uint64_t hash, std::string &&str) {
    HamtNodeEntry *entryToInsert = &table[hash & FIRST_N_BITS];

    if (entryToInsert->isNull()) {
        HamtLeaf *leaf = new HamtLeaf(hash);
        leaf->data.push_back(std::move(str));
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
                leaf->data.push_back(std::move(str));
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
                if (std::find(otherLeaf->data.begin(),
                              otherLeaf->data.end(),
                              str) == otherLeaf->data.end()) {
                    otherLeaf->data.push_back(std::move(str));
                }
                return;

            } else if (nextKey > otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)calloc(sizeof(HamtNode)
                                                     + sizeof(HamtNodeEntry), 1);
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                HamtLeaf *leaf = new HamtLeaf(hash);
                leaf->data.push_back(std::move(str));

                newNode->children[0] = HamtNodeEntry(leaf);
                newNode->children[1] = std::move(*entryToInsert);

                *entryToInsert = HamtNodeEntry(newNode);

                return;
            } else if (nextKey < otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)calloc(sizeof(HamtNode)
                                                     + sizeof(HamtNodeEntry), 1);
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                HamtLeaf *leaf = new HamtLeaf(hash);
                leaf->data.push_back(std::move(str));

                newNode->children[0] = std::move(*entryToInsert);
                newNode->children[1] = HamtNodeEntry(leaf);

                *entryToInsert = HamtNodeEntry(newNode);
                return;
            } else {
                otherLeaf->hash >>= BITS_PER_LEVEL;

                HamtNode *newNode = (HamtNode *)calloc(sizeof(HamtNode), 1);
                newNode->map = 0;
                newNode->markHash(otherLeaf->hash);
                newNode->children[0] = std::move(*entryToInsert);
                *entryToInsert = HamtNodeEntry(newNode);

                continue;
            }
        }
    }
}

bool TopLevelHamtNode::lookup(uint64_t hash, const std::string &str) const {
    const HamtNodeEntry *entry = &table[hash & FIRST_N_BITS];

    if (entry->isNull()) return false;

    while (true) {
        if (entry->isLeaf()) {
            const HamtLeaf *leaf = entry->getLeaf();

            return std::find(leaf->data.begin(),
                             leaf->data.end(),
                             str) != leaf->data.end();
        } else {
            hash >>= BITS_PER_LEVEL;
            const HamtNode *node = entry->getChild();

            if (!node->containsHash(hash)) {
                return false;
            }

            entry = &node->children[node->numberOfHashesAbove(hash) - 1];
            continue;
        }
    }
}

void deleteFromNode(HamtNodeEntry *entry, std::uint64_t hash) {
    assert(entry != NULL);
    assert(!entry->isNull());

    if (entry->isLeaf()) {
        entry->free();
        *entry = HamtNodeEntry();
    } else {
        HamtNode *node = entry->getChild();

        assert(node->containsHash(hash));

        int nChildren = node->numberOfChildren();

        // If we just destructed the node's only child, then delete this node
        // and be done with it:
        if (nChildren == 1) {
            entry->free();
            *entry = HamtNodeEntry();
            return;
        }

        node->unmarkHash(hash);
        int idx = node->numberOfHashesAbove(hash);

        node->children[idx].free();

        // Say there were 5 children before, and we're removing idx 2. Then
        // before we had:
        //
        //     [A] [B] [C] [D] [E]
        //
        // and now we want:
        // 
        //     [A] [B] [D] [E]
        //
        // So we need to move 2 children, i.e. nChildren - idx - 1.
        //
        std::memmove(&node->children[idx],
                     &node->children[idx + 1],
                     (nChildren - idx - 1) * sizeof(HamtNodeEntry));


        // Continuing the above example, we need to realloc to 5 children
        // (nChildren - 1). Since the HamtNode already has space for 1 child
        // built in, that leaves nChildren - 2.
        HamtNode *newNode
            = (HamtNode *)std::realloc(node,
                                       sizeof(HamtNode)
                                     + (nChildren - 2) * sizeof(HamtNodeEntry));
        *entry = HamtNodeEntry(newNode);
    }
}

bool TopLevelHamtNode::remove(uint64_t hash, const std::string &str) {
    HamtNodeEntry *entry = &table[hash & FIRST_N_BITS];

    // These two variables let us clean up a sub-tree if we delete the only
    // key it contains. entryToDeleteTo points to either:
    //
    // - The entry we're deleting in the top-level table.
    // - The last node we looked at that had more than one child.
    //
    // hashToDeleteTo contains the hash that we had when we looked at that
    // entry.
    //
    // If we delete the last key in a leaf, we delete the subtree starting at
    // entryToDeleteTo; see deleteFromNode.
    HamtNodeEntry *entryToDeleteTo = entry;
    std::uint64_t hashToDeleteTo = hash >> 6;

    if (entry->isNull()) return false;

    while (true) {
        hash >>= 6;
        if (entry->isLeaf()) {
            HamtLeaf *leaf = entry->getLeaf();
            auto it = std::find(leaf->data.begin(), leaf->data.end(), str);

            if (it != leaf->data.end()) {
                leaf->data.erase(it);
                
                if (leaf->data.empty()) {
                    deleteFromNode(entryToDeleteTo, hashToDeleteTo);
                }

                return true;
            }
            return false;
        } else {
            HamtNode *node = entry->getChild();

            if (node->numberOfChildren() > 1) {
                entryToDeleteTo = entry;
                hashToDeleteTo = hash;
            }

            if (!node->containsHash(hash)) {
                return false;
            }

            entry = &node->children[node->numberOfHashesAbove(hash) - 1];
        }
    }
}

TopLevelHamtNode::~TopLevelHamtNode() {
    for (std::uint64_t i = 0; i < MAX_IDX; ++i) {
        table[i].free();
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

bool HamtNodeEntry::isLeaf() const {
    return ptr & 1;
}

bool HamtNodeEntry::isNull() const {
    return ptr == 0;
}

HamtNode *HamtNodeEntry::getChild() {
    assert(!isNull() && !isLeaf());
    return reinterpret_cast<HamtNode *>(ptr);
}

const HamtNode *HamtNodeEntry::getChild() const {
    assert(!isNull() && !isLeaf());
    return reinterpret_cast<HamtNode *>(ptr);
}

HamtLeaf *HamtNodeEntry::getLeaf() {
    assert(isLeaf());
    return reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

const HamtLeaf *HamtNodeEntry::getLeaf() const {
    assert(isLeaf());
    return reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

void HamtNodeEntry::free() noexcept {
    if (isNull()) {
        return;
    } else if (!isLeaf()) {
        // This gets a bit tricky. We need to free the tree rooted at this
        // node, but we can't call `delete` because we allocated this child
        // with `malloc`. So...
        HamtNode *child = getChild();
        int nChildren = child->numberOfChildren();
        for (int i = 0; i < nChildren; ++i) {
            child->children[i].free();
        }
        std::free(getChild());
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

int HamtNode::numberOfChildren() const {
    return __builtin_popcountll((unsigned long long)map);
}

std::uint64_t HamtNode::numberOfHashesAbove(std::uint64_t hash) const {
    std::uint64_t rest = map >> (hash & FIRST_N_BITS);
    return __builtin_popcountll((unsigned long long)rest);
}

bool HamtNode::containsHash(std::uint64_t hash) const {
    return (map & (1ULL << (hash & FIRST_N_BITS))) != 0;
}

void HamtNode::markHash(std::uint64_t hash) {
    map |= (1ULL << (hash & FIRST_N_BITS));
}

void HamtNode::unmarkHash(std::uint64_t hash) {
    map &= ~(1ULL << (hash & FIRST_N_BITS));
}

//////////////////////////////////////////////////////////////////////////////
// Hamt method definitions.
//

void Hamt::insert(std::string &&str) {
    std::uint64_t hash = hasher(str);
    root.insert(hash, std::move(str));
}

bool Hamt::lookup(const std::string &str) const {
    std::uint64_t hash = hasher(str);
    return root.lookup(hash, str);
}

bool Hamt::remove(const std::string &str) {
    std::uint64_t hash = hasher(str);
    return root.remove(hash, str);
}

// Re-enable the warning we disabled at the start.
// warning.
#ifdef __GNUC__
#ifndef __clang__
    #pragma GCC diagnostic warning "-Wclass-memaccess"
#endif
#endif
