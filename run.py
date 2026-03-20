#!/usr/bin/env python3
import subprocess
import sys
import os

def run_script(script_name):
    print(f"\n--- [Executing {script_name}] ---")
    try:
        # Popen allows streaming output live to console
        process = subprocess.Popen([sys.executable, script_name], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line, end="")
        process.wait()
        return process.returncode
    except Exception as e:
        print(f"Error running {script_name}: {e}")
        return 1

def main():
    # Set working directory to the script's location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print("=== Build and Flash Orchestrator ===")

    # 1. Build
    build_code = run_script("build.py")
    if build_code != 0:
        print("\n[ERROR] Build failed. Aborting flash operations.")
        sys.exit(build_code)

    print("\n[SUCCESS] Build finished.")

    # 2. Flash
    flash_code = run_script("flash.py")
    if flash_code != 0:
        print("\n[ERROR] Flashing failed.")
        sys.exit(flash_code)

    print("\n[ALL SUCCESS] Orchestration completed successfully.")


if __name__ == "__main__":
    main()
