#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

using ConfigType = YAML::Node;

constexpr const char* ENDPOINT = "invest-public-api.tinkoff.ru:443";
constexpr const char* CONFIG_DIRECTORY = "private/config.yaml";
constexpr int MAX_DEPTH = 50;
constexpr static int NUMBER_OF_SPACES_PER_NUMBER = 10;
