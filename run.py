#!/usr/bin/env python3
import subprocess
import sys
import os
import time

# ANSI color codes
CLR_RESET = "\033[0m"
CLR_BOLD = "\033[1m"
CLR_RED = "\033[31m"
CLR_GREEN = "\033[32m"
CLR_YELLOW = "\033[33m"
CLR_BLUE = "\033[34m"
CLR_CYAN = "\033[36m"

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

def run_script(script_name):
    print_log(f"Executing: {script_name}...", color=CLR_BLUE, bold=True)
    try:
        # Pass sys.executable to ensure we use the same python interpreter
        # stdout=subprocess.PIPE and stderr=subprocess.STDOUT to combine streams
        process = subprocess.Popen(
            [sys.executable, script_name], 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True,
            bufsize=1, # Line-buffered
            universal_newlines=True
        )
        
        # Stream output live
        for line in process.stdout:
            print(line, end="")
        
        process.wait()
        return process.returncode
    except Exception as e:
        print_log(f"Fatal error executing {script_name}: {e}", color=CLR_RED)
        return 1

def main():
    # Set working directory to the script's location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    # Enable ANSI colors if on Windows (some older versions need this)
    if os.name == 'nt':
        os.system('') # This call enables VT100 sequences in Windows cmd/pwsh

    print_log("Starting STM32 Build & Flash Sequence", color=CLR_CYAN, bold=True)
    start_time = time.time()

    # 1. Build
    build_rc = run_script("build.py")
    if build_rc != 0:
        print_log("Build failed. Flash sequence aborted.", color=CLR_RED, bold=True)
        sys.exit(build_rc)

    # 2. Flash
    flash_rc = run_script("flash.py")
    if flash_rc != 0:
        print_log("Flashing failed.", color=CLR_RED, bold=True)
        sys.exit(flash_rc)

    elapsed = time.time() - start_time
    print_log(f"Sequence finished in {elapsed:.2f} seconds.", color=CLR_CYAN)
    print_log("Workflow completed successfully! ✨", color=CLR_GREEN, bold=True)

if __name__ == "__main__":
    main()
