
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "router.h"
#include "binary_router.h"
#include "patricia_router.h"
#include "multibit_router.h"
#include "lulea_router.h"

using Clock = std::chrono::steady_clock;

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

static uint32_t maskOf(int len) {
    return len == 0 ? 0u : (~0u << (32 - len));
}

static std::string toDotted(uint32_t v) {
    return std::to_string(v >> 24) + "." +
           std::to_string((v >> 16) & 255) + "." +
           std::to_string((v >> 8) & 255) + "." +
           std::to_string(v & 255);
}

static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// ----------------------------------------------------------------
// Data generator
// ----------------------------------------------------------------

struct RouteSpec {
    uint32_t prefix;
    int len;
    int router;
};

// Generate realistic prefix lengths
static int pickLen(std::mt19937& rng) {
    int x = rng() % 100;

    if (x < 2)  return 8 + rng() % 5;
    if (x < 12) return 13 + rng() % 4;
    if (x < 30) return 17 + rng() % 7;
    if (x < 90) return 24;

    return 25 + rng() % 4;
}

static uint64_t keyOf(uint32_t prefix, int len) {
    return ((uint64_t)prefix << 6) | (uint64_t)len;
}

static std::vector<RouteSpec> generateRoutes(
    size_t n,
    std::mt19937& rng
) {
    std::vector<RouteSpec> routes;
    routes.reserve(n);

    std::unordered_set<uint64_t> seen;
    seen.reserve(n * 2);

    auto add = [&](uint32_t p, int len, int router) {
        if (seen.insert(keyOf(p, len)).second) {
            routes.push_back({p, len, router});
        }
    };

    // Initial sample routes
    add(0u, 0, 1);
    add(100u << 24, 8, 2);
    add(100u << 24, 16, 3);
    add((100u << 24) | (1u << 8), 24, 6);

    while (routes.size() < n) {

        int len = pickLen(rng);
        uint32_t p = rng() & maskOf(len);

        // 60% routes are nested inside existing prefixes
        if (rng() % 100 < 60) {

            for (int t = 0; t < 5; t++) {

                const RouteSpec& par =
                    routes[rng() % routes.size()];

                if (par.len < len) {

                    p = (par.prefix |
                         (rng() & ~maskOf(par.len)))
                        & maskOf(len);

                    break;
                }
            }
        }

        add(p, len, 1 + (int)(rng() % 64));
    }

    return routes;
}

// ----------------------------------------------------------------
// Packet generator
// ----------------------------------------------------------------

static std::vector<std::string> generatePackets(
    size_t n,
    const std::vector<RouteSpec>& routes,
    std::mt19937& rng
) {
    std::vector<std::string> packets;
    packets.reserve(n);

    packets.push_back("100.0.1.55");
    packets.push_back("100.0.9.1");
    packets.push_back("100.7.7.7");
    packets.push_back("55.1.1.1");

    while (packets.size() < n) {

        uint32_t ip;

        if (rng() % 100 < 85) {

            const RouteSpec& r =
                routes[rng() % routes.size()];

            ip = r.prefix | (rng() & ~maskOf(r.len));

        } else {
            ip = rng();
        }

        packets.push_back(toDotted(ip));
    }

    return packets;
}

// ----------------------------------------------------------------
// Dump generated files
// ----------------------------------------------------------------

static void dumpFiles(
    const std::vector<RouteSpec>& routes,
    const std::vector<std::string>& packets
) {
    std::string rf =
        "data/gen_routes_" + std::to_string(routes.size()) + ".txt";

    std::ofstream ro(rf);

    ro << "# prefix            router\n";

    for (const auto& r : routes) {
        ro << toDotted(r.prefix)
           << "/" << r.len
           << "  ->  " << r.router << "\n";
    }

    std::ofstream po("data/gen_packets.txt");

    for (const auto& p : packets) {
        po << p << "\n";
    }

    std::cout << "  wrote " << rf
              << " and data/gen_packets.txt\n";
}

// ----------------------------------------------------------------
// Build hooks
// ----------------------------------------------------------------

static void prepareAlgo(LinearRouter&) {}
static void prepareAlgo(TrieRouter&) {}
static void prepareAlgo(PatriciaRouter&) {}
static void prepareAlgo(MultibitRouter&) {}

// Lulea requires an explicit build step
static void prepareAlgo(LuleaRouter& r) {
    r.build();
}

// ----------------------------------------------------------------
// Node information
// ----------------------------------------------------------------

static size_t getNodes(const LinearRouter&) {
    return 0;
}

static size_t getNodes(const LuleaRouter&) {
    return 0; // Node-count API not currently available
}

template <typename R>
static size_t getNodes(const R& r) {
    return r.nodeCount();
}

// ----------------------------------------------------------------
// Benchmark result
// ----------------------------------------------------------------

struct Out {
    std::string name;

    long long subSum = 0;
    long long fullSum = 0;

    uint64_t fullHash = 14695981039346656037ULL;

    std::vector<int> checkAnswers;

    size_t nUse = 0;
};

// ----------------------------------------------------------------
// Run one algorithm
// ----------------------------------------------------------------

template <typename R>
static Out runAlgo(
    const char* name,
    const std::vector<RouteSpec>& routes,
    const std::vector<std::string>& cidrs,
    const std::vector<std::string>& packets,
    size_t nUse,
    int runs,
    size_t nodeBytes,
    size_t nCheck,
    std::ofstream& csv,
    long long& sink
) {

    Out out;

    out.name = name;
    out.nUse = nUse;

    R r;

    // Insert routing entries
    for (size_t i = 0; i < routes.size(); i++) {
        r.addRoute(cidrs[i], routes[i].router);
    }

    // Build algorithm-specific structures
    prepareAlgo(r);

    // ------------------------------------------------
    // Correctness check: individual packet answers
    // ------------------------------------------------

    size_t checkCount = std::min(nCheck, nUse);

    out.checkAnswers.reserve(checkCount);

    for (size_t i = 0; i < checkCount; i++) {

        int answer = r.lookup(packets[i]);

        out.checkAnswers.push_back(answer);
        out.subSum += answer;
    }

    // ------------------------------------------------
    // Full packet processing and checksum
    // ------------------------------------------------

    for (size_t i = 0; i < nUse; i++) {

        int answer = r.lookup(packets[i]);

        out.fullSum += answer;

        // FNV-style rolling hash
        out.fullHash ^= static_cast<uint32_t>(answer + 1);
        out.fullHash *= 1099511628211ULL;
    }

    // ------------------------------------------------
    // Warm-up
    // ------------------------------------------------

    for (size_t i = 0; i < 200 && i < nUse; i++) {
        sink += r.lookup(packets[i]);
    }

    // ------------------------------------------------
    // Throughput measurement
    // ------------------------------------------------

    std::vector<double> nsPerLookup;

    for (int run = 0; run < runs; run++) {

        auto t0 = Clock::now();

        for (size_t i = 0; i < nUse; i++) {
            sink += r.lookup(packets[i]);
        }

        double sec =
            std::chrono::duration<double>(
                Clock::now() - t0
            ).count();

        nsPerLookup.push_back(sec / nUse * 1e9);
    }

    double ns = median(nsPerLookup);

    // ------------------------------------------------
    // Individual lookup latency
    // ------------------------------------------------

    std::vector<double> lat;
    lat.reserve(nUse);

    for (size_t i = 0; i < nUse; i++) {

        auto a = Clock::now();

        sink += r.lookup(packets[i]);

        auto b = Clock::now();

        lat.push_back(
            std::chrono::duration<double, std::nano>(
                b - a
            ).count()
        );
    }

    std::sort(lat.begin(), lat.end());

    double p50 = lat[lat.size() * 50 / 100];
    double p99 = lat[lat.size() * 99 / 100];

    // ------------------------------------------------
    // Memory estimation
    // ------------------------------------------------

    size_t nodes = getNodes(r);

    // -1 indicates memory measurement unavailable
    double memMB = nodeBytes == 0
        ? -1.0
        : nodes * nodeBytes / (1024.0 * 1024.0);

    // ------------------------------------------------
    // Print results
    // ------------------------------------------------

    std::printf(
        "%-10s %9zu %10zu %11.1f %13.0f %8.0f %9.0f %10zu %9.1f\n",
        name,
        routes.size(),
        nUse,
        ns,
        1e9 / ns,
        p50,
        p99,
        nodes,
        memMB
    );

    // ------------------------------------------------
    // CSV output
    // ------------------------------------------------

    csv << name << ","
        << routes.size() << ","
        << nUse << ","
        << ns << ","
        << 1e9 / ns << ","
        << p50 << ","
        << p99 << ","
        << nodes << ","
        << memMB << "\n";

    csv.flush();

    return out;
}

// ----------------------------------------------------------------
// Main
// ----------------------------------------------------------------

int main(int argc, char** argv) {

    bool dump =
        (argc > 1 && std::strcmp(argv[1], "--dump") == 0);

    const size_t sizes[] = {
        1000,
        10000,
        100000,
        1000000
    };

    const size_t nPackets = 1000000;

    const size_t LINEAR_MAX = 100000;

    const size_t nCheck = 2000;

    std::mt19937 rng(42);

    std::ofstream csv("results.csv");

    csv << "algo,routes,packets,ns_per_lookup,"
           "lookups_per_sec,p50_ns,p99_ns,nodes,mem_mb\n";

    long long sink = 0;

    bool anyMismatch = false;

    std::printf(
        "%-10s %9s %10s %11s %13s %8s %9s %10s %9s\n",
        "algo",
        "routes",
        "packets",
        "ns/lookup",
        "lookups/sec",
        "p50_ns",
        "p99_ns",
        "nodes",
        "mem_MB"
    );

    // ====================================================
    // Test all routing table sizes
    // ====================================================

    for (size_t n : sizes) {

        std::cout << "\nGenerating "
                  << n << " routes...\n";

        std::vector<RouteSpec> routes =
            generateRoutes(n, rng);

        std::vector<std::string> packets =
            generatePackets(nPackets, routes, rng);

        std::vector<std::string> cidrs;

        cidrs.reserve(routes.size());

        for (const auto& r : routes) {

            cidrs.push_back(
                toDotted(r.prefix) +
                "/" +
                std::to_string(r.len)
            );
        }

        if (dump) {
            dumpFiles(routes, packets);
        }

        std::vector<Out> outs;

        // ------------------------------------------------
        // Linear
        // ------------------------------------------------

        if (n <= LINEAR_MAX) {

            outs.push_back(
                runAlgo<LinearRouter>(
                    "linear",
                    routes,
                    cidrs,
                    packets,
                    nCheck,
                    3,
                    0,
                    nCheck,
                    csv,
                    sink
                )
            );
        }

        // ------------------------------------------------
        // Binary Trie
        // ------------------------------------------------

        outs.push_back(
            runAlgo<TrieRouter>(
                "trie",
                routes,
                cidrs,
                packets,
                packets.size(),
                5,
                sizeof(TrieNode),
                nCheck,
                csv,
                sink
            )
        );

        // ------------------------------------------------
        // Patricia Trie
        // ------------------------------------------------

        outs.push_back(
            runAlgo<PatriciaRouter>(
                "patricia",
                routes,
                cidrs,
                packets,
                packets.size(),
                5,
                sizeof(PatNode),
                nCheck,
                csv,
                sink
            )
        );

        // ------------------------------------------------
        // Multibit Trie
        // ------------------------------------------------

        outs.push_back(
            runAlgo<MultibitRouter>(
                "multibit2",
                routes,
                cidrs,
                packets,
                packets.size(),
                5,
                sizeof(MbNode),
                nCheck,
                csv,
                sink
            )
        );

        // ------------------------------------------------
        // Lulea
        // ------------------------------------------------

        outs.push_back(
            runAlgo<LuleaRouter>(
                "lulea",
                routes,
                cidrs,
                packets,
                packets.size(),
                5,
                0,
                nCheck,
                csv,
                sink
            )
        );

        // ====================================================
        // Correctness verification
        // ====================================================

        std::cout << "\nCorrectness verification:\n";

        // Compare individual answers for first nCheck packets
        const Out& reference = outs[0];

        for (size_t a = 1; a < outs.size(); a++) {

            if (outs[a].checkAnswers != reference.checkAnswers) {

                std::cerr
                    << "MISMATCH in first "
                    << nCheck
                    << " packets at "
                    << n
                    << " routes: "
                    << reference.name
                    << " vs "
                    << outs[a].name
                    << "\n";

                anyMismatch = true;
            }
        }

        // Compare complete packet results among algorithms
        // that processed the entire packet batch.

        const Out* fullReference = nullptr;

        for (const auto& o : outs) {

            if (o.nUse != packets.size())
                continue;

            if (!fullReference) {
                fullReference = &o;
                continue;
            }

            if (o.fullSum != fullReference->fullSum ||
                o.fullHash != fullReference->fullHash) {

                std::cerr
                    << "FULL PACKET MISMATCH at "
                    << n
                    << " routes: "
                    << fullReference->name
                    << " vs "
                    << o.name
                    << "\n";

                anyMismatch = true;
            }
        }

        std::cout << "\n";

    }

    // ====================================================
    // Final result
    // ====================================================

    std::cout
        << (anyMismatch
            ? "RESULT: MISMATCH found, results are NOT valid\n"
            : "RESULT: all algorithms agree on checked results\n");

    std::cout << "Checksum: " << sink << "\n";

    return anyMismatch ? 1 : 0;
}
