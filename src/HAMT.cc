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
        auto leaf = std::make_unique<HamtLeaf>(hash);
        leaf->data.push_back(std::move(str));
        *entryToInsert = HamtNodeEntry(std::move(leaf));
        return;
    }

    while (true) {
        if (!entryToInsert->isLeaf()) {
            hash >>= BITS_PER_LEVEL;
            auto nodeToInsert = entryToInsert->takeChild();
            int idx = nodeToInsert->numberOfHashesAbove(hash);

            if (nodeToInsert->containsHash(hash)) {
                auto nextEntry = &nodeToInsert->children[idx - 1];
                *entryToInsert = HamtNodeEntry(std::move(nodeToInsert));
                entryToInsert = nextEntry;
                continue;
            } else {
                std::uint64_t nChildren = nodeToInsert->numberOfChildren() + 1;
                std::uint64_t nExtraBytes = (nChildren - 1)
                                          * sizeof(HamtNodeEntry);
                std::uint64_t nBytes = sizeof(HamtNode) + nExtraBytes;

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

                uniqueHamtNode newNode(
                        (HamtNode *)realloc(nodeToInsert.release(), nBytes));
                // We need to add a new child. Set the bit in the map:
                newNode->markHash(hash);
                // Move the entries in the array after where we're going to
                // insert one over. Needless to say, sketch as hell.
                std::memmove(&newNode->children[idx + 1],
                             &newNode->children[idx],
                             (nChildren - idx - 1) * sizeof(HamtNodeEntry));

                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));
                newNode->children[idx].release();
                newNode->children[idx] = HamtNodeEntry(std::move(leaf));
                *entryToInsert = HamtNodeEntry(std::move(newNode));
                return;
            }
        } else {
            auto otherLeaf = entryToInsert->takeLeaf();
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
                *entryToInsert = HamtNodeEntry(std::move(otherLeaf));
                return;

            } else if (nextKey > otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                uniqueHamtNode newNode(
                        (HamtNode *)calloc(sizeof(HamtNode)
                                         + sizeof(HamtNodeEntry), 1));
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));

                newNode->children[0] = HamtNodeEntry(std::move(leaf));
                newNode->children[1] = HamtNodeEntry(std::move(otherLeaf));

                *entryToInsert = HamtNodeEntry(std::move(newNode));

                return;
            } else if (nextKey < otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                uniqueHamtNode newNode(
                        (HamtNode *)calloc(sizeof(HamtNode)
                                         + sizeof(HamtNodeEntry), 1));
                newNode->map = 0;
                newNode->markHash(hash);
                newNode->markHash(otherLeaf->hash);

                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));

                newNode->children[0] = HamtNodeEntry(std::move(otherLeaf));
                newNode->children[1] = HamtNodeEntry(std::move(leaf));

                *entryToInsert = HamtNodeEntry(std::move(newNode));

                return;
            } else {
                otherLeaf->hash >>= BITS_PER_LEVEL;

                uniqueHamtNode newNode(
                        (HamtNode *)calloc(sizeof(HamtNode), 1));
                newNode->map = 0;
                newNode->markHash(otherLeaf->hash);
                newNode->children[0] = HamtNodeEntry(std::move(otherLeaf));
                *entryToInsert = HamtNodeEntry(std::move(newNode));

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
            const HamtLeaf &leaf = entry->getLeaf();

            return std::find(leaf.data.begin(),
                             leaf.data.end(),
                             str) != leaf.data.end();
        } else {
            hash >>= BITS_PER_LEVEL;
            const HamtNode &node = entry->getChild();

            if (!node.containsHash(hash)) {
                return false;
            }

            entry = &node.children[node.numberOfHashesAbove(hash) - 1];
            continue;
        }
    }
}

void deleteFromNode(HamtNodeEntry *entry, std::uint64_t hash) {
    assert(entry != NULL);
    assert(!entry->isNull());

    if (entry->isLeaf()) {
        *entry = HamtNodeEntry();
    } else {
        auto node = entry->takeChild();

        assert(node->containsHash(hash));

        int nChildren = node->numberOfChildren();

        // If we just destructed the node's only child, then delete this node
        // and be done with it:
        if (nChildren == 1) {
            return;
        }

        node->unmarkHash(hash);
        int idx = node->numberOfHashesAbove(hash);

        node->children[idx].~HamtNodeEntry();

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
        uniqueHamtNode newNode(
            (HamtNode *)std::realloc(node.release(),
                                     sizeof(HamtNode)
                                   + (nChildren - 2) * sizeof(HamtNodeEntry)));
        *entry = HamtNodeEntry(std::move(newNode));
    }
}

bool TopLevelHamtNode::remove(uint64_t hash, const std::string &str) {
    HamtNodeEntry *entry = &table[hash & FIRST_N_BITS];
    HamtNodeEntry *entryToDeleteTo = entry;
    std::uint64_t hashToDeleteTo = hash >> 6;

    if (entry->isNull()) return false;

    while (true) {
        hash >>= 6;
        if (entry->isLeaf()) {
            auto &leaf = entry->getLeaf();
            auto it = std::find(leaf.data.begin(), leaf.data.end(), str);

            if (it != leaf.data.end()) {
                leaf.data.erase(it);
                
                if (leaf.data.empty()) {
                    deleteFromNode(entryToDeleteTo, hashToDeleteTo);
                }

                return true;
            }
            return false;
        } else {
            auto &node = entry->getChild();

            if (node.numberOfChildren() > 1) {
                entryToDeleteTo = entry;
                hashToDeleteTo = hash;
            }

            if (!node.containsHash(hash)) {
                return false;
            }

            entry = &node.children[node.numberOfHashesAbove(hash) - 1];
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// HamtNodeEntry method definitions.
//

HamtNodeEntry::HamtNodeEntry(uniqueHamtNode node)
    : ptr(reinterpret_cast<std::uintptr_t>(node.release())) {}

// Initialize the pointer 
HamtNodeEntry::HamtNodeEntry(std::unique_ptr<HamtLeaf> leaf)
    : ptr(reinterpret_cast<std::uintptr_t>(leaf.release()) | 1) {}

// Initialize the pointer to NULL.
HamtNodeEntry::HamtNodeEntry() : ptr(0) {}

HamtNodeEntry::HamtNodeEntry(HamtNodeEntry &&other): ptr(other.ptr) {
    other.ptr = 0;
}

HamtNodeEntry &HamtNodeEntry::operator=(HamtNodeEntry &&other) {
    this->~HamtNodeEntry();
    ptr = other.ptr;
    other.ptr = 0;
    return *this;
}

void HamtNodeEntry::release() {
    ptr = 0;
}

bool HamtNodeEntry::isLeaf() const {
    return ptr & 1;
}

bool HamtNodeEntry::isNull() const {
    return ptr == 0;
}

uniqueHamtNode HamtNodeEntry::takeChild() {
    assert(!isNull() && !isLeaf());
    uniqueHamtNode result(reinterpret_cast<HamtNode *>(ptr));
    ptr = 0;
    return result;
}

HamtNode &HamtNodeEntry::getChild() {
    assert(!isNull() && !isLeaf());
    return *reinterpret_cast<HamtNode *>(ptr);
}

const HamtNode &HamtNodeEntry::getChild() const {
    assert(!isNull() && !isLeaf());
    return *reinterpret_cast<HamtNode *>(ptr);
}

std::unique_ptr<HamtLeaf> HamtNodeEntry::takeLeaf() {
    std::unique_ptr<HamtLeaf> result(reinterpret_cast<HamtLeaf *>(ptr & (~1)));
    ptr = 0;
    return result;
}

HamtLeaf &HamtNodeEntry::getLeaf() {
    assert(isLeaf());
    return *reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

const HamtLeaf &HamtNodeEntry::getLeaf() const {
    assert(isLeaf());
    return *reinterpret_cast<HamtLeaf *>(ptr & (~1));
}

HamtNodeEntry::~HamtNodeEntry() {
    if (isNull()) {
        return;
    } else if (!isLeaf()) {
        // This gets a bit tricky. We need to free the tree rooted at this
        // node, but we can't call `delete` because we allocated this child
        // with `malloc`. So...
        takeChild();
    } else {
        takeLeaf();
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

HamtNode::~HamtNode() {
    int nChildren = numberOfChildren();
    for (int i = 0; i < nChildren; ++i) {
        children[i].~HamtNodeEntry();
    }
}

//////////////////////////////////////////////////////////////////////////////
// HamtNodeDeleter method definitions.
//

void HamtNodeDeleter::operator()(HamtNode *node) {
    node->~HamtNode();
    free(node);
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
