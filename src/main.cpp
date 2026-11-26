
#include <iostream>

#include "router.h"
#include "binary_router.h"
#include "patricia_router.h"
#include "multibit_router.h"
#include "lulea_router.h"

int main() {

    LinearRouter   lin;
    TrieRouter     trie;
    PatriciaRouter pat;
    MultibitRouter mb;
    LuleaRouter    lulea;

    // 1. Load the same routing table into all five algorithms

    if (!lin.loadRoutes("data/routes.txt")) {
        std::cerr << "Linear: could not load routes\n";
        return 1;
    }

    if (!trie.loadRoutes("data/routes.txt")) {
        std::cerr << "Trie: could not load routes\n";
        return 1;
    }

    if (!pat.loadRoutes("data/routes.txt")) {
        std::cerr << "Patricia: could not load routes\n";
        return 1;
    }

    if (!mb.loadRoutes("data/routes.txt")) {
        std::cerr << "Multibit: could not load routes\n";
        return 1;
    }

    if (!lulea.loadRoutes("data/routes.txt")) {
        std::cerr << "Lulea: could not load routes\n";
        return 1;
    }

    // 2. Build Lulea compressed structure
    std::cout << "\nBuilding Lulea structure...\n";
    lulea.build();

    // 3. Display node information

    std::cout << "\n========== STRUCTURE INFORMATION ==========\n";

    std::cout << "Trie nodes: "
              << trie.nodeCount() << "\n";

    std::cout << "Patricia nodes: "
              << pat.nodeCount() << "\n";

    std::cout << "Multibit nodes: "
              << mb.nodeCount()
              << " (" << mb.memoryBytes() << " bytes)\n";

    // 4. Display Lulea internal structures

    std::cout << "\n========== LULEA BITMAPS ==========\n";
    lulea.printBitmaps();

    std::cout << "\n========== LULEA COMPRESSED TABLE ==========\n";
    lulea.printCompressedTable();

    // 5. Process identical packets through all five algorithms

    std::cout << "\n--- Linear ---\n";
    lin.processPackets("data/packets.txt");

    std::cout << "\n--- Binary Trie ---\n";
    trie.processPackets("data/packets.txt");

    std::cout << "\n--- Patricia ---\n";
    pat.processPackets("data/packets.txt");

    std::cout << "\n--- Multibit (stride 2) ---\n";
    mb.processPackets("data/packets.txt");

    std::cout << "\n--- Lulea ---\n";
    lulea.processPackets("data/packets.txt");

    return 0;
}
