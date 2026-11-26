
#include "lulea_router.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string luleaIpToBits(const std::string& ip) {
    std::stringstream ss(ip);
    std::string octet;
    std::string bits;
    int count = 0;

    while (std::getline(ss, octet, '.')) {
        if (count >= 4 || octet.empty()) return "";

        int value = -1;
        try {
            std::size_t pos = 0;
            value = std::stoi(octet, &pos);

            if (pos != octet.size() || value < 0 || value > 255)
                return "";
        } catch (...) {
            return "";
        }

        for (int bit = 7; bit >= 0; --bit) {
            bits += ((value >> bit) & 1) ? '1' : '0';
        }

        ++count;
    }

    return count == 4 ? bits : "";
}

uint32_t bitsToInt(const std::string& bits) {
    uint32_t value = 0;

    for (char bit : bits) {
        value = (value << 1) | static_cast<uint32_t>(bit == '1');
    }

    return value;
}

uint32_t prefixMask(int length) {
    if (length <= 0) return 0;
    if (length >= 32) return 0xFFFFFFFFu;

    return 0xFFFFFFFFu << (32 - length);
}

uint32_t getPrefix(uint32_t address, int length) {
    return address & prefixMask(length);
}

} // namespace

// --------------------------------------------------
// ADD ROUTE
// --------------------------------------------------

void LuleaRouter::addRoute(const std::string& cidr, int router) {
    const std::size_t slash = cidr.find('/');

    if (slash == std::string::npos) {
        std::cerr << "Invalid CIDR: " << cidr << '\n';
        return;
    }

    const std::string ipPart = cidr.substr(0, slash);
    const std::string lengthPart = cidr.substr(slash + 1);

    int length = -1;

    try {
        std::size_t pos = 0;
        length = std::stoi(lengthPart, &pos);

        if (pos != lengthPart.size() || length < 0 || length > 32) {
            std::cerr << "Invalid prefix length: " << cidr << '\n';
            return;
        }
    } catch (...) {
        std::cerr << "Invalid prefix length: " << cidr << '\n';
        return;
    }

    const std::string fullBits = luleaIpToBits(ipPart);

    if (fullBits.size() != 32) {
        std::cerr << "Invalid IPv4 address: " << ipPart << '\n';
        return;
    }

    routes_.push_back({
        fullBits.substr(0, static_cast<std::size_t>(length)),
        length,
        router
    });
}

// --------------------------------------------------
// LOAD ROUTES
// --------------------------------------------------

bool LuleaRouter::loadRoutes(const std::string& file) {
    std::ifstream input(file);

    if (!input) {
        std::cerr << "Cannot open routes file: " << file << '\n';
        return false;
    }

    std::string line;
    int loaded = 0;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::size_t first = line.find_first_not_of(" \t");

        if (first == std::string::npos || line[first] == '#')
            continue;

        std::istringstream row(line);

        std::string cidr;
        std::string arrow;
        int router = -1;

        if (!(row >> cidr >> arrow >> router) || arrow != "->") {
            std::cerr << "Skipping malformed route line "
                      << lineNumber << ": " << line << '\n';
            continue;
        }

        const std::size_t oldSize = routes_.size();

        addRoute(cidr, router);

        if (routes_.size() > oldSize)
            ++loaded;
    }

    std::cout << "Loaded " << loaded << " Lulea routes\n";

    return loaded > 0;
}

// --------------------------------------------------
// PLACEHOLDERS
// --------------------------------------------------

void LuleaRouter::buildTrie() {
    // Placeholder: original Lulea trie construction
    // is not implemented in this prototype.
}

void LuleaRouter::leafPush() {
    // Placeholder: original leaf-pushing stage
    // is not implemented in this prototype.
}

void LuleaRouter::compressBitmaps() {
    // Placeholder: original bitmap compression
    // is not implemented in this prototype.
}

// --------------------------------------------------
// LEVEL 1: PREFIXES /0 TO /16
// --------------------------------------------------

void LuleaRouter::buildLevel1() {
    for (const LuleaRoute& route : routes_) {
        const int length = route.prefixLength;

        if (length > 16)
            continue;

        const uint32_t prefix = bitsToInt(route.bits);

        const uint32_t key =
            (static_cast<uint32_t>(length) << 16) | prefix;

        compressedEntries_.push_back({route.router, false});

        level1_[key] =
            static_cast<int>(compressedEntries_.size() - 1);
    }
}

// --------------------------------------------------
// LEVEL 2: PREFIXES /17 TO /24
// --------------------------------------------------

void LuleaRouter::buildLevel2() {
    for (const LuleaRoute& route : routes_) {
        const int length = route.prefixLength;

        if (length <= 16 || length > 24)
            continue;

        const uint32_t prefix =
            bitsToInt(route.bits) << (32 - length);

        const uint32_t key =
            (static_cast<uint32_t>(length) << 24) |
            (prefix >> 8);

        compressedEntries_.push_back({route.router, false});

        level2_[key] =
            static_cast<int>(compressedEntries_.size() - 1);
    }
}

// --------------------------------------------------
// LEVEL 3: PREFIXES /25 TO /32
// --------------------------------------------------

void LuleaRouter::buildLevel3() {
    for (const LuleaRoute& route : routes_) {
        const int length = route.prefixLength;

        if (length <= 24)
            continue;

        const uint32_t prefix =
            bitsToInt(route.bits) << (32 - length);

        const uint64_t key =
            (static_cast<uint64_t>(length) << 32) |
            prefix;

        compressedEntries_.push_back({route.router, false});

        level3_[key] =
            static_cast<int>(compressedEntries_.size() - 1);
    }
}

// --------------------------------------------------
// BUILD ROUTING TABLE
// --------------------------------------------------

void LuleaRouter::build() {
    compressedEntries_.clear();
    bitmaps_.clear();
    codeWords_.clear();
    mapTable_.clear();

    level1_.clear();
    level2_.clear();
    level3_.clear();

    buildTrie();
    leafPush();

    buildLevel1();
    buildLevel2();
    buildLevel3();

    compressBitmaps();

    std::cout << "Prefix lookup prototype built: "
              << "L1=" << level1_.size()
              << ", L2=" << level2_.size()
              << ", L3=" << level3_.size()
              << '\n';
}

// --------------------------------------------------
// RANK
// --------------------------------------------------

int LuleaRouter::rank(uint32_t bitmap, int position) const {
    if (position < 0)
        return 0;

    if (position >= 31) {
        int count = 0;

        while (bitmap) {
            bitmap &= (bitmap - 1);
            ++count;
        }

        return count;
    }

    const uint32_t mask =
        (uint32_t{1} << (position + 1)) - 1;

    bitmap &= mask;

    int count = 0;

    while (bitmap) {
        bitmap &= (bitmap - 1);
        ++count;
    }

    return count;
}

// --------------------------------------------------
// LONGEST PREFIX MATCH LOOKUP
// --------------------------------------------------

int LuleaRouter::lookup(const std::string& ip) const {
    const std::string bits = luleaIpToBits(ip);

    if (bits.size() != 32)
        return -1;

    const uint32_t address = bitsToInt(bits);

    int bestLength = -1;
    int bestRouter = -1;

    // LEVEL 1: /0 TO /16
    for (int length = 0; length <= 16; ++length) {

        const uint32_t prefix =
            (length == 0)
                ? 0
                : (address >> (32 - length));

        const uint32_t key =
            (static_cast<uint32_t>(length) << 16) |
            prefix;

        const auto it = level1_.find(key);

        if (it != level1_.end() && length > bestLength) {
            bestLength = length;
            bestRouter = compressedEntries_[it->second].value;
        }
    }

    // LEVEL 2: /17 TO /24
    for (int length = 17; length <= 24; ++length) {

        const uint32_t prefix =
            getPrefix(address, length) >> 8;

        const uint32_t key =
            (static_cast<uint32_t>(length) << 24) |
            prefix;

        const auto it = level2_.find(key);

        if (it != level2_.end() && length > bestLength) {
            bestLength = length;
            bestRouter = compressedEntries_[it->second].value;
        }
    }

    // LEVEL 3: /25 TO /32
    for (int length = 25; length <= 32; ++length) {

        const uint64_t prefix =
            getPrefix(address, length);

        const uint64_t key =
            (static_cast<uint64_t>(length) << 32) |
            prefix;

        const auto it = level3_.find(key);

        if (it != level3_.end() && length > bestLength) {
            bestLength = length;
            bestRouter = compressedEntries_[it->second].value;
        }
    }

    return bestRouter;
}

// --------------------------------------------------
// PROCESS PACKETS
// --------------------------------------------------

void LuleaRouter::processPackets(const std::string& file) const {
    std::ifstream input(file);

    if (!input) {
        std::cerr << "Cannot open packets file: " << file << '\n';
        return;
    }

    std::string line;

    int total = 0;
    int routed = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::size_t first =
            line.find_first_not_of(" \t");

        if (first == std::string::npos || line[first] == '#')
            continue;

        const std::size_t last =
            line.find_last_not_of(" \t");

        const std::string ip =
            line.substr(first, last - first + 1);

        const int router = lookup(ip);

        ++total;

        if (router == -1) {
            std::cout << ip << " -> no route\n";
        } else {
            std::cout << ip << " -> router "
                      << router << '\n';

            ++routed;
        }
    }

    std::cout << "\nTotal packets: " << total
              << "\nRouted packets: " << routed
              << "\nUnrouted packets: " << total - routed
              << '\n';
}

// --------------------------------------------------
// PRINT BITMAPS
// --------------------------------------------------

void LuleaRouter::printBitmaps() const {
    std::cout << "\nBitmap chunks:\n";

    if (bitmaps_.empty()) {
        std::cout
            << "No bitmap compression is implemented "
            << "in this prototype.\n";
        return;
    }

    for (std::size_t i = 0; i < bitmaps_.size(); ++i) {
        std::cout << "Chunk " << i
                  << " Bitmap: " << bitmaps_[i].bitmap
                  << " Base: " << bitmaps_[i].baseIndex
                  << '\n';
    }
}

// --------------------------------------------------
// PRINT COMPRESSED TABLE
// --------------------------------------------------

void LuleaRouter::printCompressedTable() const {
    std::cout << "\nCompressed entries:\n";

    for (std::size_t i = 0; i < compressedEntries_.size(); ++i) {
        std::cout << i << " -> router "
                  << compressedEntries_[i].value
                  << '\n';
    }
}
