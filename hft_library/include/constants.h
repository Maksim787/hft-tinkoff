#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

using ConfigType = YAML::Node;

const std::string ENDPOINT = "invest-public-api.tinkoff.ru:443";
const std::string CONFIG_DIRECTORY = "private/config.yaml";
