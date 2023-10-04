cmake_minimum_required(VERSION 3.25)
project(hft_tinkoff)

set(CMAKE_CXX_STANDARD 23)

# Find YAML
find_package(yaml-cpp REQUIRED)
# Include TinkoffInvestSDK
add_subdirectory(third_party/TinkoffInvestSDK)

# Add sources
file(GLOB SOURCES "src/*.cpp")
# Add test scripts
file(GLOB TEST_SCRIPTS "test/*.cpp")

# Iterate over sources and scripts to include libraries
set(FILES ${SOURCES} ${TEST_SCRIPTS})
foreach (SOURCE_FILE ${FILES})
    # Extract the file name without extension
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)

    # Add an executable target
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})

    # Link libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE yaml-cpp tink_grpc_proto TinkoffInvestSDK)

    # Link include/ folder
    target_include_directories(${EXECUTABLE_NAME} PRIVATE include)
endforeach ()
