add_library(
        hft_library
        runner.cpp
        config.cpp
        strategy.cpp
        connector/market.cpp
        connector/user.cpp
        connector/utils.cpp
)
target_include_directories(hft_library PUBLIC ../include)

# Find YAML
find_package(yaml-cpp REQUIRED)
# Link libraries
target_link_libraries(hft_library PRIVATE yaml-cpp)
target_link_libraries(hft_library PUBLIC TinkoffInvestSDK tink_grpc_proto)
