add_subdirectory(third_party/TinkoffInvestSDK)

# Add library sources
file(GLOB_RECURSE HFT_LIBRARY_SOURCES "src/*.cpp")
add_library(hft_library ${HFT_LIBRARY_SOURCES})
target_include_directories(hft_library PUBLIC include/)

# Find YAML
find_package(yaml-cpp REQUIRED)
# Link libraries
target_link_libraries(hft_library PRIVATE yaml-cpp)
target_link_libraries(hft_library PUBLIC TinkoffInvestSDK tink_grpc_proto)

# Add executables
add_subdirectory(exe)
