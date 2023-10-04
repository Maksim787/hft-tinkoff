#pragma once

#include <constants.h>

#include <yaml-cpp/yaml.h>
#include <iostream>


YAML::Node read_config() {
    YAML::Node config = YAML::LoadFile(config_directory);
    return config;
}