#include "binary_router.h"
#include <fstream>
#include <iostream>
#include <sstream>

void TrieRouter::freeTree(TrieNode* n) {
    if (n == nullptr) return;
    freeTree(n->left);
    freeTree(n->right);
    delete n;
}

void TrieRouter::addRoute(const std::string& cidr, int router) {
    // 1. '/' dhoondo
    size_t slash = cidr.find('/');
    if (slash == std::string::npos) {
        std::cerr << "Invalid CIDR (no '/'): " << cidr << std::endl;
        return;
    }

    // 2. IP aur length alag karo
    std::string ipPart  = cidr.substr(0, slash);
    std::string lenPart = cidr.substr(slash + 1);

    int len;
    try {
        len = std::stoi(lenPart);
    } catch (...) {
        std::cerr << "Invalid prefix length: " << cidr << std::endl;
        return;
    }

    // 3. len 0..32 check
    if (len < 0 || len > 32) {
        std::cerr << "Prefix length out of range: " << cidr << std::endl;
        return;
    }

    // 4. IP ko bits me badlo, 32 bits check
    std::string full = ipToBits(ipPart);
    if (full.size() != 32) {
        std::cerr << "Invalid IP in route: " << cidr << std::endl;
        return;
    }

    // 5. sirf prefix ke bits
    std::string bits = full.substr(0, len);

    // 6. bits ke saath trie me chalo, jahan raasta nahi hai wahan naya node banao
    TrieNode* node = root_;
    for (char c : bits) {
        if (c == '0') {
            if (!node->left) {
                node->left = new TrieNode();
                nodeCount_++;
            }
            node = node->left;
        } else {
            if (!node->right) {
                node->right = new TrieNode();
                nodeCount_++;
            }
            node = node->right;
        }
    }

    // 7. prefix yahan khatam hota hai
    node->router = router;
}

int TrieRouter::lookup(const std::string& ip) const {
    std::string bits = ipToBits(ip);
    if (bits.size() != 32) {
        return -1;
    }

    const TrieNode* node = root_;
    int best = node->router;            // default route (/0), ya -1 agar nahi hai

    for (char c : bits) {
        node = (c == '0') ? node->left : node->right;
        if (node == nullptr) break;     // raasta khatam, ab tak ka best hi answer
        if (node->router != -1) best = node->router;
    }
    return best;
}

bool TrieRouter::loadRoutes(const std::string& file) {
    std::ifstream in(file);
    if (!in) {
        std::cerr << "Cannot open routes file: " << file << std::endl;
        return false;
    }

    std::string line;
    int lineNo = 0;
    int loaded = 0;

    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;

        std::istringstream ss(line);
        std::string cidr, arrow;
        int router;
        ss >> cidr >> arrow >> router;

        if (ss.fail() || arrow != "->") {
            std::cerr << "Skipping bad line " << lineNo << ": " << line << std::endl;
            continue;
        }

        addRoute(cidr, router);
        loaded++;
    }

    std::cout << "Loaded " << loaded << " routes\n";
    return loaded > 0;
}

void TrieRouter::processPackets(const std::string& file) const {
    std::ifstream in(file);
    if (!in) {
        std::cerr << "Cannot open packets file: " << file << std::endl;
        return;
    }

    std::string line;
    int total = 0;
    int routed = 0;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;

        size_t last = line.find_last_not_of(" \t");
        std::string ip = line.substr(first, last - first + 1);

        int router = lookup(ip);
        total++;

        if (router == -1) {
            std::cout << ip << " -> no route\n";
        } else {
            std::cout << ip << " -> router " << router << "\n";
            routed++;
        }
    }

    std::cout << total << " packets, " << routed << " routed\n";
}