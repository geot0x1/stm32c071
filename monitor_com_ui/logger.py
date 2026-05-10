"""Logging configuration"""

import logging
import sys


def setup_logging(debug: bool = False) -> logging.Logger:
    """Configure logging for the application"""
    level = logging.DEBUG if debug else logging.INFO

    logging.basicConfig(
        level=level,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.StreamHandler(sys.stdout),
        ]
    )

    return logging.getLogger(__name__)
