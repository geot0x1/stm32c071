#!/usr/bin/env python3
import subprocess
import sys
import os
import glob

def find_elf(build_dir):
    # Try finding in the root of build directory first
    elf_files = glob.glob(os.path.join(build_dir, "*.elf"))
    if not elf_files:
        # Try recursively just in case (e.g. presets might place it deeper)
        elf_files = glob.glob(os.path.join(build_dir, "**", "*.elf"), recursive=True)
    
    if elf_files:
        # Return the first one found
        return elf_files[0]
    return None

def main():
    # Set working directory to the script's location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    build_dir = "build"
    elf_path = find_elf(build_dir)

    if not elf_path:
        print(f"Error: Could not find any .elf file in '{build_dir}'.")
        print("Please build the project first (e.g., using build.py).")
        sys.exit(1)

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
