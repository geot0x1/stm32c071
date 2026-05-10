#!/usr/bin/env python3
"""Quick launch script for the serial monitor application"""

import sys
import os

# Add parent directory to path so we can run from anywhere
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from main import main

if __name__ == "__main__":
    main()
