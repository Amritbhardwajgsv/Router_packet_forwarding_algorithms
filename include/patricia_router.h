#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "router.h"

struct PatNode {
    std::string label;          
    int skip = 0;
    PatNode* left  = nullptr;
    PatNode* right = nullptr;
    int router = -1;
};

class PatriciaRouter {
    PatNode* root_;
    size_t nodeCount_ = 1;

    void freeTree(PatNode* n);

public:
    PatriciaRouter() : root_(new PatNode()) {}
    ~PatriciaRouter() { freeTree(root_); }

    // copy band: warna do objects same tree ko delete karte (double-delete crash)
    PatriciaRouter(const PatriciaRouter&) = delete;
    PatriciaRouter& operator=(const PatriciaRouter&) = delete;

    bool loadRoutes(const std::string& file);
    void addRoute(const std::string& cidr, int router);
    int  lookup(const std::string& ip) const;
    void processPackets(const std::string& file) const;

    size_t nodeCount() const { return nodeCount_; }
};