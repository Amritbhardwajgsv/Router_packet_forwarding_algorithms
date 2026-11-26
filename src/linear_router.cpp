#include "router.h"
#include <fstream>
#include <iostream>
#include <sstream>

std::string ipToBits(const std::string& ip) {
    std::vector<std::string> octets;
    size_t start = 0;

    for (size_t pos = 0; pos < ip.length(); pos++) {
        if (ip[pos] == '.') {
            octets.push_back(ip.substr(start, pos - start));
            start = pos + 1;
        }
    }
    octets.push_back(ip.substr(start));   // last octet

    if (octets.size() != 4) {
        std::cerr << "Invalid IP address format: " << ip << std::endl;
        return "";
    }

    std::string result;
    for (size_t k = 0; k < octets.size(); k++) {
        int octet = std::stoi(octets[k]);
        for (int b = 7; b >= 0; b--) {
            result += ((octet >> b) & 1) ? '1' : '0';
        }
    }
    return result;
}

void LinearRouter::addRoute(const std::string& cidr, int router) {
    size_t slash = cidr.find('/');
    if (slash == std::string::npos) {
        std::cerr << "Invalid CIDR (no '/'): " << cidr << std::endl;
        return;
    }
    std::string ipPart  = cidr.substr(0, slash);
    std::string lenPart = cidr.substr(slash + 1);
    int len;
    try {
        len = std::stoi(lenPart);
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
    routes_.push_back({bits, router});
    std::cout << bits << " -> " << router << "\n";   
}

int LinearRouter::lookup(const std::string& ip) const {
    std::string bits = ipToBits(ip);
    if (bits.size() != 32) {
        return -1;
    }

    int bestLen = -1;
    int bestRouter = -1;

    for (const Route& r : routes_) {
        int len = static_cast<int>(r.bits.size());

        bool match = true;
        for (int k = 0; k < len; k++) {
            if (bits[k] != r.bits[k]) {
                match = false;
                break;
            }
        }

        if (match && len > bestLen) {
            bestLen = len;
            bestRouter = r.router;
        }
    }
    return bestRouter;
}

bool LinearRouter::loadRoutes(const std::string& file) {
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

        // Windows ka '\r' hatao
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

void LinearRouter::processPackets(const std::string& file) const {
    std::ifstream in(file);
    if (!in) {
        std::cerr << "Cannot open packets file: " << file << std::endl;
        return;
    }

    std::string line;
    int total = 0;
    int routed = 0;

    while (std::getline(in, line)) {
        // Windows ka '\r' hatao
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // khaali line aur comment skip
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;

        // aage-peeche ke spaces hatao
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