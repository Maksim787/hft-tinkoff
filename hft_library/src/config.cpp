#include <config.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>

ConfigType read_config() {
    try {
        return YAML::LoadFile(CONFIG_DIRECTORY);
    } catch (YAML::BadFile& exception) {
        std::cout << "Failed to open file: " << exception.what() << "\n";
        std::cout << "Current working directory: " << std::filesystem::current_path() << "\n";
        throw;
    }
}