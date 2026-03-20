# Function to generate a JSON file with build information
# Arguments:
#   target_name - The CMake target name (e.g., executable)
function(generate_build_info_json target_name)
    # The output path
    set(json_output "${CMAKE_CURRENT_BINARY_DIR}/build_info.json")

    # Generate content using generator expressions
    # This will be evaluated at generate time, resolving $<TARGET_FILE:...>
    file(GENERATE OUTPUT "${json_output}"
         CONTENT "{\n  \"executable\": \"$<TARGET_FILE:${target_name}>\",\n  \"project_name\": \"${PROJECT_NAME}\"\n}\n")

    message(STATUS "Build info JSON generation configured for target: ${target_name}")
    message(STATUS "Output: ${json_output}")
endfunction()
