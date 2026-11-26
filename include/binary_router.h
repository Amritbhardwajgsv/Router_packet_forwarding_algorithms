#pragma once
#include <cstddef>
#include <string>
#include "router.h"          // ipToBits yahin declare hai

struct TrieNode {
    TrieNode* left  = nullptr;   // bit '0' ka raasta
    TrieNode* right = nullptr;   // bit '1' ka raasta
    int router = -1;             // -1 matlab yahan koi prefix khatam nahi hota
};

class TrieRouter {
    TrieNode* root_;             // root hi default route (/0) ka ghar hai
    size_t nodeCount_ = 1;       // root ko mila ke, analysis ke liye

    void freeTree(TrieNode* n);  // recursive delete

public:
    TrieRouter() : root_(new TrieNode()) {}
    ~TrieRouter() { freeTree(root_); }

    // copy band: warna do objects same tree ko delete karte (double-delete crash)
    TrieRouter(const TrieRouter&) = delete;
    TrieRouter& operator=(const TrieRouter&) = delete;

    bool loadRoutes(const std::string& file);
    void addRoute(const std::string& cidr, int router);
    int  lookup(const std::string& ip) const;
    void processPackets(const std::string& file) const;

    size_t nodeCount() const { return nodeCount_; }
};