#include "patricia_router.h"
#include <fstream>
#include <iostream>
#include <sstream>

// bit '0' -> left, '1' -> right. Reference return karta hai taaki assign ho sake
static PatNode*& slot(PatNode* n, char bit) {
    return bit == '0' ? n->left : n->right;
}

static PatNode* makeNode(const std::string& label, int router) {
    PatNode* n = new PatNode();
    n->label  = label;
    n->router = router;
    return n;
}

void PatriciaRouter::freeTree(PatNode* n) {
    if (n == nullptr) return;
    freeTree(n->left);
    freeTree(n->right);
    delete n;
}

void PatriciaRouter::addRoute(const std::string& cidr, int router) {
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

    // 6. Patricia insert
    PatNode* node = root_;
    size_t pos = 0;                       // bits me kahan tak match ho chuka

    while (true) {
        // case 1: bits khatam -> prefix yahin end hota hai
        if (pos == bits.size()) {
            node->router = router;
            return;
        }

        PatNode*& link = slot(node, bits[pos]);
        PatNode* ch = link;

        // case 2: aage raasta nahi -> baaki ke saare bits ek hi naye leaf me
        if (ch == nullptr) {
            link = makeNode(bits.substr(pos), router);
            nodeCount_++;
            return;
        }

        // child ke label se kitne bits match hote hain
        size_t k = 0;
        while (k < ch->label.size() && pos + k < bits.size()
               && ch->label[k] == bits[pos + k]) {
            k++;
        }

        // case 3: poora label match -> neeche utro
        if (k == ch->label.size()) {
            node = ch;
            pos += k;
            continue;
        }

        // case 4: label ke beech me alag ho gaye -> EDGE SPLIT
        PatNode* mid = new PatNode();
        mid->label = ch->label.substr(0, k);      // common hissa
        ch->label  = ch->label.substr(k);         // bacha hua hissa

        slot(mid, ch->label[0]) = ch;             // purana child mid ke neeche
        link = mid;                               // parent ab mid ko point kare
        nodeCount_++;

        if (pos + k == bits.size()) {
            mid->router = router;                 // prefix exactly split point pe khatam
        } else {
            slot(mid, bits[pos + k]) = makeNode(bits.substr(pos + k), router);
            nodeCount_++;
        }
        return;
    }
}

int PatriciaRouter::lookup(const std::string& ip) const {
    std::string bits = ipToBits(ip);
    if (bits.size() != 32) return -1;

    const PatNode* node = root_;
    int best = node->router;                      // default route (/0), ya -1
    size_t pos = 0;

    while (pos < bits.size()) {
        const PatNode* ch = (bits[pos] == '0') ? node->left : node->right;
        if (ch == nullptr) break;                 // raasta khatam, ab tak ka best hi answer

        // poora label address se match hona chahiye, warna ruk jao (mismatch)
        size_t len = ch->label.size();
        if (bits.compare(pos, len, ch->label) != 0) break;

        pos += len;
        node = ch;
        if (node->router != -1) best = node->router;   // last prefix yaad rakho
    }
    return best;
}

bool PatriciaRouter::loadRoutes(const std::string& file) {
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

void PatriciaRouter::processPackets(const std::string& file) const {
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