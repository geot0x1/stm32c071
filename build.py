#!/usr/bin/env python3
import subprocess
import sys
import argparse
import os
import re
import shutil

CLR_RESET  = "\033[0m"
CLR_BOLD   = "\033[1m"
CLR_DIM    = "\033[2m"
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

def format_configure_line(line):
    stripped = line.rstrip()
    if not stripped:
        return None
    if 'CMake Error' in stripped or re.search(r':\s*error:', stripped, re.IGNORECASE):
        return f"{CLR_RED}{stripped}{CLR_RESET}"
    if 'CMake Warning' in stripped or re.search(r':\s*warning:', stripped, re.IGNORECASE):
        return f"{CLR_YELLOW}{stripped}{CLR_RESET}"
    # Suppress verbose CMake status lines
    if stripped.startswith('-- '):
        return None
    return f"{CLR_DIM}{stripped}{CLR_RESET}"

def format_build_line(line):
    stripped = line.rstrip()
    if not stripped:
        return None

    # Ninja progress: [x/y] action
    m = re.match(r'^\[(\d+)/(\d+)\] (.+)', stripped)
    if m:
        current, total, action = m.groups()
        action = re.sub(r'CMakeFiles/[^.]+\.dir/', '', action)
        return f"  {CLR_CYAN}{CLR_BOLD}[{current}/{total}]{CLR_RESET} {action}"

    # Compiler errors
    if re.search(r':\s*error:', stripped):
        return f"{CLR_RED}{CLR_BOLD}{stripped}{CLR_RESET}"

    # Compiler warnings
    if re.search(r':\s*warning:', stripped):
        return f"{CLR_YELLOW}{stripped}{CLR_RESET}"

    # Compiler notes / context
    if re.search(r':\s*note:', stripped):
        return f"{CLR_DIM}{stripped}{CLR_RESET}"

    # Memory usage table (--print-memory-usage)
    if 'Memory region' in stripped or re.search(r'\b(RAM|FLASH)\b.*%', stripped):
        return f"{CLR_CYAN}{CLR_BOLD}{stripped}{CLR_RESET}"

    return stripped

def run_command(cmd_list, line_formatter=None):
    try:
        process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            formatted = line_formatter(line) if line_formatter else line.rstrip()
            if formatted is not None:
                print(formatted)
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
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)
        mode = ctypes.c_ulong()
        kernel32.GetConsoleMode(handle, ctypes.byref(mode))
        kernel32.SetConsoleMode(handle, mode.value | 0x0004)

    build_dir = "build"

    if args.clean and os.path.exists(build_dir):
        print_log(f"Cleaning build directory '{build_dir}'...", color=CLR_YELLOW)
        shutil.rmtree(build_dir, ignore_errors=True)

    toolchain_file = "cmake/gcc-arm-none-eabi.cmake"

    if not os.path.exists(toolchain_file):
        print_log(f"Toolchain file '{toolchain_file}' not found. Let CMake try automatically.", color=CLR_YELLOW)
        toolchain_file = None

    config_cmd = [
        "cmake", "-G", "Ninja", "-S", ".", "-B", build_dir,
    ]

    if toolchain_file:
        config_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}")

    config_cmd.append("-DCMAKE_BUILD_TYPE=Debug")

    print_log("Configuring CMake...", color=CLR_CYAN, bold=True)
    run_command(config_cmd, line_formatter=format_configure_line)
    print_log("Configuration done.", color=CLR_GREEN)

    build_cmd = ["cmake", "--build", build_dir]

    print_log("Building...", color=CLR_CYAN, bold=True)
    run_command(build_cmd, line_formatter=format_build_line)

    print_log("Build completed successfully.", color=CLR_GREEN, bold=True)

if __name__ == "__main__":
    main()
