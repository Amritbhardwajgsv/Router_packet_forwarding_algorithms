#pragma once
#include <string>
#include <vector>

std::string ipToBits(const std::string& ip);

struct Route {
    std::string bits;
    int router;
};

class LinearRouter {
    std::vector<Route> routes_;
public:
    bool loadRoutes(const std::string& file);
    void addRoute(const std::string& cidr, int router);
    int  lookup(const std::string& ip) const;
    void processPackets(const std::string& file) const;
};