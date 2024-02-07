# Add test scripts
file(GLOB_RECURSE EXECUTABLES "*.cpp")

# Iterate over sources and scripts to include libraries
foreach (EXECUTABLE_RAW_NAME ${EXECUTABLES})
    # Extract the file name without extension
    get_filename_component(EXECUTABLE_NAME ${EXECUTABLE_RAW_NAME} NAME_WE)

    # Add an executable target
    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_RAW_NAME})

    # Link libraries
    target_include_directories(${EXECUTABLE_NAME} PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/third_party/clickhouse-cpp/ $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/third_party/clickhouse-cpp/contrib/absl)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE hft_library clickhouse-cpp-lib)
endforeach ()
