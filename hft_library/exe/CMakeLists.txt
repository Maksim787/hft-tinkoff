# Add test scripts
file(GLOB_RECURSE EXECUTABLES "*.cpp")

set(WARNING_AS_ERROR ON)

# Iterate over sources and scripts to include libraries
foreach (EXECUTABLE_RAW_NAME ${EXECUTABLES})
    # Extract the file name without extension
    get_filename_component(EXECUTABLE_NAME ${EXECUTABLE_RAW_NAME} NAME_WE)

    # Add an executable target
    add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_RAW_NAME})

    if(WARNING_AS_ERROR)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE -Werror)
    endif()

    # Link libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE hft_library)
endforeach ()
