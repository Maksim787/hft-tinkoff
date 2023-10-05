# Add test scripts
file(GLOB TEST_SCRIPTS "*.cpp")

# Iterate over sources and scripts to include libraries
foreach (SOURCE_FILE ${TEST_SCRIPTS})
    # Extract the file name without extension
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)

    # Add an executable target
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})

    # Link libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE hft_library)
endforeach ()
