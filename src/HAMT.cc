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
                int nChildren = nodeToInsert->numberOfChildren() + 1;
                int nExtraBytes = (nChildren - 1) * sizeof(HamtNodeEntry);
                void *mem = malloc(sizeof(HamtNode) + nExtraBytes);
                std::unique_ptr<HamtNode> newNode(new (mem) HamtNode());

                // We need to add a new child. Set the bit in the map:
                newNode->map = nodeToInsert->map;
                nodeToInsert->map = 0;
                newNode->markHash(hash);

                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));

                // This is obviously sketchy, but with GCC benchmarking shows
                // it's vastly faster than copying the elements over one by
                // one.
                std::memcpy(&newNode->children[0],
                            &nodeToInsert->children[0],
                            idx * sizeof(HamtNodeEntry));

                new (&newNode->children[idx]) HamtNodeEntry(std::move(leaf));

                std::memcpy(&newNode->children[idx + 1],
                            &nodeToInsert->children[idx],
                            (nChildren - idx - 1) * sizeof(HamtNodeEntry));

                std::memset(&nodeToInsert->children[0], 0,
                            sizeof(HamtNodeEntry) * (nChildren - 1));

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

                // We know the first child will be a leaf containing the key
                // we're inserting. Create that leaf:
                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));

                // And make a new node with that leaf, plus space for one more:
                size_t nBytes = sizeof(HamtNode) + sizeof(HamtNodeEntry);
                void *mem = calloc(nBytes, 1);
                std::unique_ptr<HamtNode> newNode(
                        new (mem) HamtNode(hash,
                                           HamtNodeEntry(std::move(leaf))));

                // Add the other leaf as the second child:
                newNode->markHash(otherLeaf->hash);
                newNode->children[1] = HamtNodeEntry(std::move(otherLeaf));

                *entryToInsert = HamtNodeEntry(std::move(newNode));

                return;
            } else if (nextKey < otherNextKey) {
                otherLeaf->hash >>= BITS_PER_LEVEL;
                hash >>= BITS_PER_LEVEL;

                size_t nBytes = sizeof(HamtNode) + sizeof(HamtNodeEntry);
                void *mem = calloc(nBytes, 1);
                std::unique_ptr<HamtNode> newNode(
                        new (mem) HamtNode(otherLeaf->hash,
                                           HamtNodeEntry(std::move(otherLeaf))));

                newNode->markHash(hash);

                auto leaf = std::make_unique<HamtLeaf>(hash);
                leaf->data.push_back(std::move(str));
                newNode->children[1] = HamtNodeEntry(std::move(leaf));

                *entryToInsert = HamtNodeEntry(std::move(newNode));

                return;
            } else {
                otherLeaf->hash >>= BITS_PER_LEVEL;

                void *mem = calloc(sizeof(HamtNode), 1);
                std::unique_ptr<HamtNode> newNode(
                        new (mem) HamtNode(otherLeaf->hash,
                                           HamtNodeEntry(std::move(otherLeaf))));

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
        std::unique_ptr<HamtNode> node = entry->takeChild();
        assert(node->containsHash(hash));

        int nChildren = node->numberOfChildren();

        // If we just destructed the node's only child, then delete this node
        // and be done with it:
        if (nChildren == 1) {
            return;
        }

        // Otherwise, we'll want to allocate a new, smaller node.

        // Continuing the above example, we need to realloc to 5 children
        // (nChildren - 1). Since the HamtNode already has space for 1 child
        // built in, that leaves nChildren - 2.
        size_t nBytes = sizeof(HamtNode)
                      + sizeof(HamtNodeEntry) * (nChildren - 2);
        void *mem = calloc(nBytes, 1);
        std::unique_ptr<HamtNode> newNode(new (mem) HamtNode());
        newNode->map = node->map;
        node->map = 0;
        newNode->unmarkHash(hash);
        int idx = newNode->numberOfHashesAbove(hash);

        // As with the corresponding junk in insert, measurements show that
        // this is actually substantially faster than just moving the children
        // one by one.

        // memcpy and then zero out the bytes before the child we're deleting.
        size_t firstHalfBytes = idx * sizeof(HamtNodeEntry);
        std::memcpy(&newNode->children[0],
                    &node->children[0],
                    firstHalfBytes);
        std::memset(&node->children[0], 0, firstHalfBytes);

        // Delete the actual child we're looking at.
        node->children[idx] = HamtNodeEntry();

        // memcpy and then zero out the bytes after the child we're deleting.
        size_t sndHalfBytes = (nChildren - 1 - idx) * sizeof(HamtNodeEntry);
        std::memcpy(&newNode->children[idx],
                    &node->children[idx + 1],
                    sndHalfBytes);
        std::memset(&node->children[0], 0, sndHalfBytes);

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

HamtNodeEntry::HamtNodeEntry(std::unique_ptr<HamtNode> node)
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

std::unique_ptr<HamtNode> HamtNodeEntry::takeChild() {
    assert(!isNull() && !isLeaf());
    std::unique_ptr<HamtNode> result(reinterpret_cast<HamtNode *>(ptr));
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

HamtNode::HamtNode(): map(0) {
    children[0] = HamtNodeEntry();
}

HamtNode::HamtNode(uint64_t hash, HamtNodeEntry entry)
    : map(1ULL << (hash & FIRST_N_BITS))
{
    children[0] = std::move(entry);
}

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

void HamtNode::operator delete(void *p) {
    free(p);
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
