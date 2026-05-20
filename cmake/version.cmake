# Injects FIRMWARE_HASH and FIRMWARE_VERSION compile definitions into a target.
#
# Reads the version from <repo_root>/version.txt (format: MAJOR.MINOR.PATCH).
#   - Each component must be digits only, 1-3 digits (0-999 per field).
#   - FIRMWARE_HASH  : short git commit hash uppercased, or "UNKNOWN"
#   - FIRMWARE_VERSION : "V<MAJOR>.<MINOR>.<PATCH>_<HASH>" (all caps)
#
# Usage:
#   include(cmake/version.cmake)
#   target_firmware_version(<target>)

function(_read_version out_version)
    set(version_file "${CMAKE_SOURCE_DIR}/version.txt")

    if(NOT EXISTS "${version_file}")
        message(FATAL_ERROR "version.txt not found at ${version_file}")
    endif()

    file(READ "${version_file}" raw)
    string(STRIP "${raw}" version_str)

    # Validate: only digits and dots
    if(NOT version_str MATCHES "^[0-9.]+$")
        message(FATAL_ERROR "version.txt: invalid characters in \"${version_str}\" (only digits and dots allowed)")
    endif()

    # Validate: exactly MAJOR.MINOR.PATCH
    if(NOT version_str MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        message(FATAL_ERROR "version.txt: \"${version_str}\" is not in MAJOR.MINOR.PATCH format")
    endif()

    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")

    # Validate: each field max 3 digits
    foreach(field IN ITEMS "${major}" "${minor}" "${patch}")
        string(LENGTH "${field}" field_len)
        if(field_len GREATER 3)
            message(FATAL_ERROR "version.txt: version field \"${field}\" exceeds 3 digits")
        endif()
    endforeach()

    set(${out_version} "${major}.${minor}.${patch}" PARENT_SCOPE)
endfunction()

function(_read_git_hash out_hash)
    find_package(Git QUIET)

    set(hash "UNKNOWN")

    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE raw_hash
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(raw_hash)
            string(TOUPPER "${raw_hash}" hash)
        endif()
    endif()

    set(${out_hash} "${hash}" PARENT_SCOPE)
endfunction()

function(target_firmware_version target_name)
    _read_version(fw_ver)
    _read_git_hash(fw_hash)

    set(fw_version "V${fw_ver}.${fw_hash}")

    message(STATUS "Firmware hash   : ${fw_hash}")
    message(STATUS "Firmware version: ${fw_version}")

    target_compile_definitions(${target_name} PRIVATE
        FIRMWARE_HASH="${fw_hash}"
        FIRMWARE_VERSION="${fw_version}"
    )
endfunction()
