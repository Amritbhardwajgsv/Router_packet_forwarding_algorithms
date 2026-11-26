# IPv4 Router Packet Forwarding Algorithms
### Comparative Analysis of Longest Prefix Matching (LPM) Algorithms

A comparative implementation and performance analysis of IPv4 Longest Prefix Matching algorithms using C++ and Verilog, exploring both software-based routing structures and hardware-accelerated packet forwarding using TCAM.

## Overview

Every router must determine the appropriate next hop for an incoming IP packet based on its destination address. Since routing tables contain prefixes of different lengths, multiple entries can match the same destination.

**Longest Prefix Matching (LPM)** ensures that the most specific matching prefix is selected.

This project implements and compares five software-based IPv4 lookup algorithms and demonstrates a hardware-based TCAM lookup architecture.

The primary objective is to evaluate lookup performance, memory consumption, structural complexity, and update behaviour across different routing approaches.

## Algorithms Implemented

### 1. Software-Based Algorithms (C++)

| Algorithm | Core Mechanism | Key Characteristic |
|---|---|---|
| Linear Search | Sequentially checks every prefix | Reference implementation (Oracle) |
| Binary Trie | Examines one bit per level | Predictable maximum depth of 32 |
| Patricia Trie | Path compression of single-child nodes | Reduced memory consumption |
| Multibit Trie | Examines 2 bits per level | Controlled prefix expansion |
| Luleå-Inspired Algorithm | Three-level hash-based lookup structure | Partitioned IPv4 prefix lookup |

**Algorithm highlights:**

- **Linear Search:** Serves as the reference implementation for validating all other algorithms.
- **Binary Trie:** Organises routing prefixes into a binary tree, reducing lookup complexity from dependence on the number of routes to dependence on address width.
- **Patricia Trie:** Compresses unnecessary intermediate nodes and stores skipped bit sequences on edges.
- **Multibit Trie:** Uses a stride of 2 bits per level, reducing the maximum traversal depth from 32 to 16 levels. Controlled prefix expansion handles prefixes that do not align with stride boundaries.
- **Luleå-Inspired Router:** Uses three prefix-length partitions (/0–/16, /17–/24 and /25–/32) with hash maps and a shared entry table.

**Note:** The Luleå implementation is a prototype inspired by the original Luleå forwarding-table architecture. It does not yet implement the complete bitmap compression and leaf-pushing mechanisms of the original algorithm.

### 2. Hardware-Based Lookup (Verilog)

A TCAM-based lookup engine is designed to demonstrate parallel prefix matching in hardware.

The architecture consists of three primary units:

1. **TCAM Array:** Compares the destination IP address against all stored routing entries simultaneously using value and mask comparisons.
2. **Priority Encoder:** Selects the highest-priority matching entry, ensuring the longest prefix wins.
3. **Next-Hop RAM:** Stores and retrieves the corresponding next-hop identifier.

The design supports an 8-entry TCAM with 32-bit IPv4 addresses.

A self-checking Verilog testbench verifies the lookup functionality, while waveform analysis is performed using GTKWave.

## System Architecture

```text
             IPv4 Routing Table
                     |
                     v
             Route Loading
                     |
          +----------+----------+
          |          |          |
          v          v          v
       Linear     Binary     Patricia
       Search      Trie        Trie
          |          |          |
          +----------+----------+
                     |
             +-------+-------+
             |               |
             v               v
         Multibit       Lulea-Inspired
           Trie             Table
             |               |
             +-------+-------+
                     |
                     v
             Lookup Results
                     |
                     v
          Comparison with Oracle
                     |
                     v
          Performance Evaluation


          Hardware Implementation

              Destination IP
                     |
                     v
                TCAM Array
             (Parallel Matching)
                     |
                     v
              Priority Encoder
                     |
                     v
               Next-Hop RAM
                     |
                     v
               Selected Hop
```

## Technology Stack

| Component | Technology |
|---|---|
| Software implementation | C++ |
| Data structures | Tries, Hash Maps, Bit Manipulation |
| Hardware description | Verilog HDL |
| Simulation | Icarus Verilog |
| Waveform analysis | GTKWave |
| Compilation | GCC / G++ |
| Benchmarking | C++ Chrono Library |
| Optimisation | GCC -O2 / -O3 |

## Project Structure

The core software modules are organised around a common router interface.

```text
Router_packet_forwarding_algorithms/
│
├── main.cpp
│
├── linear_router.cpp
├── binary_trie.cpp
├── patricia_router.cpp
├── multibit_router.cpp
├── lulea.cpp
│
├── data/
│   ├── routes.txt
│   └── packets.txt
│
├── tcam_3units.v
├── tb_tcam_3units.v
│
└── README.md
```

*The above illustrates the logical organisation; adjust it to match the actual repository directory structure.*

## Input Format

### Routing Table

Each route is represented using CIDR notation followed by its next-hop identifier.

```text
192.168.1.0/24 -> 3
192.168.0.0/16 -> 2
10.0.0.0/8 -> 1
0.0.0.0/0 -> 0
```

### Packet File

Each line contains an IPv4 destination address.

```text
192.168.1.10
192.168.2.15
10.10.10.10
8.8.8.8
```

All five software implementations process the same routing table and packet file to ensure a consistent comparison.

## Running the Software Implementation

Compile the software modules using G++ with optimisation enabled.

```bash
g++ -std=c++17 -O2 \
main.cpp \
linear_router.cpp \
binary_trie.cpp \
patricia_trie.cpp \
multibit_trie.cpp \
lulea.cpp \
-o router
```

Run:

```bash
./router
```

Adjust source filenames and include paths if your repository uses
