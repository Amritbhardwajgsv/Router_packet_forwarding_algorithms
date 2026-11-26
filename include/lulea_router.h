#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Lulea-specific route type; avoids conflict with Route in router.h.
struct LuleaRoute {
    std::string bits;       // prefix bits only, length == prefixLength
    int prefixLength = 0;
    int router = -1;
};

struct BitmapChunk {
    uint32_t bitmap = 0;
    int baseIndex = 0;
};

struct CompressedEntry {
    int value = -1;
    bool isSubTrie = false;
};

class LuleaRouter {
private:
    std::vector<LuleaRoute> routes_;
    std::vector<CompressedEntry> compressedEntries_;
    std::vector<BitmapChunk> bitmaps_;
    std::vector<uint16_t> codeWords_;
    std::vector<uint16_t> mapTable_;

    // Prefix-index prototype tables. Level 3 needs 64-bit keys.
    std::unordered_map<uint32_t, int> level1_;
    std::unordered_map<uint32_t, int> level2_;
    std::unordered_map<uint64_t, int> level3_;

    void buildTrie();
    void leafPush();
    void buildLevel1();
    void buildLevel2();
    void buildLevel3();
    void compressBitmaps();
    int rank(uint32_t bitmap, int position) const;

public:
    bool loadRoutes(const std::string& file);
    void addRoute(const std::string& cidr, int router);
    void build();
    int lookup(const std::string& ip) const;
    void processPackets(const std::string& file) const;
    void printBitmaps() const;
    void printCompressedTable() const;
};
