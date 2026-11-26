#include "multibit_router.h"
#include <fstream>
#include <iostream>
#include <sstream>

// s[pos .. pos+n-1] ke '0'/'1' characters ko number banata hai
static int bitsToInt(const std::string& s, int pos, int n) {
    int v = 0;
    for (int j = 0; j < n; j++) v = v * 2 + (s[pos + j] - '0');
    return v;
}

void MultibitRouter::freeTree(MbNode* n) {
    if (n == nullptr) return;
    for (int i = 0; i < 4; i++) freeTree(n->entry[i].child);
    delete n;
}

void MultibitRouter::addRoute(const std::string& cidr, int router) {
    size_t slash = cidr.find('/');
    if (slash == std::string::npos) {
        std::cerr << "Invalid CIDR (no '/'): " << cidr << std::endl;
        return;
    }
    std::string ipPart = cidr.substr(0, slash);
    int len;
    try {
        len = std::stoi(cidr.substr(slash + 1));
    } catch (...) {
        std::cerr << "Invalid prefix length: " << cidr << std::endl;
        return;
    }
    if (len < 0 || len > 32) {
        std::cerr << "Prefix length out of range: " << cidr << std::endl;
        return;
    }
    std::string full = ipToBits(ipPart);
    if (full.size() != 32) {
        std::cerr << "Invalid IP in route: " << cidr << std::endl;
        return;
    }
    std::string bits = full.substr(0, len);

    // /0: koi bits nahi, to default route
    if (len == 0) {
        defaultRouter_ = router;
        return;
    }

    // poore 2-bit levels pe neeche jao, jahan raasta nahi wahan node banao
    int levels = (len - 1) / 2;
    MbNode* node = root_;
    for (int lvl = 0; lvl < levels; lvl++) {
        int idx = bitsToInt(bits, lvl * 2, 2);
        if (!node->entry[idx].child) {
            node->entry[idx].child = new MbNode();
            nodeCount_++;
        }
        node = node->entry[idx].child;
    }

    // aakhri level: r bache hue bits (1 ya 2), expansion se 2^(2-r) entries bharo
    int r = len - levels * 2;
    int base = bitsToInt(bits, levels * 2, r) << (2 - r);
    int count = 1 << (2 - r);
    for (int i = 0; i < count; i++) {
        MbEntry& e = node->entry[base + i];
        if (e.len <= len) {          // chhota prefix bade ko na mitaye
            e.router = router;
            e.len = len;
        }
    }
}

int MultibitRouter::lookup(const std::string& ip) const {
    std::string bits = ipToBits(ip);
    if (bits.size() != 32) return -1;

    int best = defaultRouter_;
    const MbNode* node = root_;
    for (int lvl = 0; lvl < 16; lvl++) {
        int idx = bitsToInt(bits, lvl * 2, 2);
        const MbEntry& e = node->entry[idx];
        if (e.router != -1) best = e.router;
        if (!e.child) break;         // raasta khatam, ab tak ka best hi answer
        node = e.child;
    }
    return best;
}

// loadRoutes aur processPackets: binary_trie.cpp se copy karo,
// bas "TrieRouter::" ki jagah "MultibitRouter::" likho
bool MultibitRouter::loadRoutes(const std::string& file) {
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

void MultibitRouter::processPackets(const std::string& file) const {
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