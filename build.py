#!/usr/bin/env python3
import subprocess
import sys
import argparse
import os
import shutil

def run_command(cmd_list):
    print(f"Executing: {' '.join(cmd_list)}")
    try:
        # On Windows, list form with shell=False is usually safe if executable is in PATH.
        # We read output live from the stream.
        process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end="")
        process.wait()
        
        if process.returncode != 0:
            print(f"\nError: Command failed with code {process.returncode}")
            sys.exit(process.returncode)
    except FileNotFoundError:
        print(f"Error: Could not find '{cmd_list[0]}'. Please ensure it's in your PATH.")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Build STM32 project using CMake and Ninja.")
    parser.add_argument("--clean", action="store_true", help="Clean build directory first")
    
    args = parser.parse_args()

    # Set working directory to the script's location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    build_dir = "build"

    # Cleaning step
    if args.clean and os.path.exists(build_dir):
        print(f"Cleaning build directory '{build_dir}'...")
        shutil.rmtree(build_dir, ignore_errors=True)

    # Toolchain selection
    toolchain_file = "cmake/gcc-arm-none-eabi.cmake"
    
    if not os.path.exists(toolchain_file):
        print(f"Warning: Toolchain file '{toolchain_file}' not found. Let CMake try automatically.")
        toolchain_file = None

    # Configure command
    config_cmd = [
        "cmake",
        "-G", "Ninja",
        "-S", ".",
        "-B", build_dir,
    ]
    
    if toolchain_file:
        config_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}")
        
    config_cmd.append("-DCMAKE_BUILD_TYPE=Debug")

    print("--- Configuring CMake ---")
    run_command(config_cmd)

    # Build command
    build_cmd = [
        "cmake",
        "--build", build_dir
    ]

    print("\n--- Building CMake ---")
    run_command(build_cmd)

if __name__ == "__main__":
    main()
