#include <iostream>

#include "config.h"


int main() {
    auto config = read_config();
    std::cout << "token: " << config["runner"]["token"].as<std::string>() << "\n";
    return 0;
}