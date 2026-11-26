#pragma once
#include <cstddef>
#include <string>
#include "router.h"

struct MbNode;

struct MbEntry {
    int router = -1;           // is entry pe best prefix ka router
    int len    = -1;           // us prefix ki length
    MbNode* child = nullptr;   // agla level
};

struct MbNode {
    MbEntry entry[4];          // 2 bits -> 4 raaste
};

class MultibitRouter {
    MbNode* root_;
    int defaultRouter_ = -1;   // /0 route yahan, kyunki uske koi bits nahi hote
    size_t nodeCount_ = 1;

    void freeTree(MbNode* n);

public:
    MultibitRouter() : root_(new MbNode()) {}
    ~MultibitRouter() { freeTree(root_); }
    MultibitRouter(const MultibitRouter&) = delete;
    MultibitRouter& operator=(const MultibitRouter&) = delete;

    bool loadRoutes(const std::string& file);
    void addRoute(const std::string& cidr, int router);
    int  lookup(const std::string& ip) const;
    void processPackets(const std::string& file) const;

    size_t nodeCount() const { return nodeCount_; }
    size_t memoryBytes() const { return nodeCount_ * sizeof(MbNode); }
};