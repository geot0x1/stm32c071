#!/usr/bin/env python3
import subprocess
import sys
import os
import glob
import json

def find_elf(build_dir):
    # Try finding via build_info.json first
    json_path = os.path.join(build_dir, "build_info.json")
    if not os.path.exists(json_path):
        print(f"Error: Could not find build information file: {json_path}")
        print("Please configure the project using CMake first.")
        sys.exit(1)

    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
            executable = data.get("executable")
            if executable and os.path.exists(executable):
                return executable
            else:
                print(f"Error: Executable '{executable}' from {json_path} does not exist.")
                print("Please build the project first.")
                sys.exit(1)
    except (json.JSONDecodeError, IOError) as e:
        print(f"Error: Could not read build_info.json: {e}")
        sys.exit(1)

def main():
    # Set working directory to the script's location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    build_dir = "build"
    elf_path = find_elf(build_dir)



    print(f"Found ELF file: {elf_path}")

    # Flash command as specified by the user
    cmd = [
        "STM32_Programmer_CLI",
        "-c", "port=SWD",
        "-d", elf_path,
        "-rst"
    ]

    print(f"Executing: {' '.join(cmd)}")
    try:
        # Popen allows us to stream output live
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end="")
        process.wait()
        
        if process.returncode != 0:
            print(f"\nError: Flashing failed with code {process.returncode}")
            sys.exit(process.returncode)
        else:
            print("\nFlashing successful!")
    except FileNotFoundError:
        print("\nError: 'STM32_Programmer_CLI' not found in PATH.")
        print("Please ensure STM32CubeProgrammer is installed and added to your System PATH.")
        sys.exit(1)

if __name__ == "__main__":
    main()
