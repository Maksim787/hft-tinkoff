#include <config.h>

int main() {
    auto config = read_config();
    std::cout << config["token"].as<std::string>() << "\n";
    return 0;
}