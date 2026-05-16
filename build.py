#!/usr/bin/env python3
import subprocess
import sys
import argparse
import os
import shutil

CLR_RESET  = "\033[0m"
CLR_BOLD   = "\033[1m"
CLR_RED    = "\033[31m"
CLR_GREEN  = "\033[32m"
CLR_YELLOW = "\033[33m"
CLR_BLUE   = "\033[34m"
CLR_CYAN   = "\033[36m"

def print_log(message, color=CLR_RESET, bold=False):
    prefix = f"{CLR_BOLD}[INFO]{CLR_RESET}"
    if color == CLR_RED:
        prefix = f"{CLR_BOLD}{CLR_RED}[FAIL]{CLR_RESET}"
    elif color == CLR_GREEN:
        prefix = f"{CLR_BOLD}{CLR_GREEN}[PASS]{CLR_RESET}"
    elif color == CLR_YELLOW:
        prefix = f"{CLR_BOLD}{CLR_YELLOW}[WARN]{CLR_RESET}"

    bold_style = CLR_BOLD if bold else ""
    print(f"{prefix} {bold_style}{color}{message}{CLR_RESET}")

def run_command(cmd_list):
    print_log(f"Executing: {' '.join(cmd_list)}", color=CLR_BLUE, bold=True)
    try:
        process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end="")
        process.wait()

        if process.returncode != 0:
            print_log(f"Command failed with code {process.returncode}", color=CLR_RED, bold=True)
            sys.exit(process.returncode)
    except FileNotFoundError:
        print_log(f"Could not find '{cmd_list[0]}'. Please ensure it's in your PATH.", color=CLR_RED, bold=True)
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Build STM32 project using CMake and Ninja.")
    parser.add_argument("--clean", action="store_true", help="Clean build directory first")

    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    if os.name == 'nt':
        os.system('')

    build_dir = "build"

    if args.clean and os.path.exists(build_dir):
        print_log(f"Cleaning build directory '{build_dir}'...", color=CLR_YELLOW)
        shutil.rmtree(build_dir, ignore_errors=True)

    toolchain_file = "cmake/gcc-arm-none-eabi.cmake"

    if not os.path.exists(toolchain_file):
        print_log(f"Toolchain file '{toolchain_file}' not found. Let CMake try automatically.", color=CLR_YELLOW)
        toolchain_file = None

    config_cmd = [
        "cmake",
        "-G", "Ninja",
        "-S", ".",
        "-B", build_dir,
    ]

    if toolchain_file:
        config_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}")

    config_cmd.append("-DCMAKE_BUILD_TYPE=Debug")

    print_log("Configuring CMake", color=CLR_CYAN, bold=True)
    run_command(config_cmd)

    build_cmd = [
        "cmake",
        "--build", build_dir
    ]

    print_log("Building", color=CLR_CYAN, bold=True)
    run_command(build_cmd)

    print_log("Build completed successfully.", color=CLR_GREEN, bold=True)

if __name__ == "__main__":
    main()
