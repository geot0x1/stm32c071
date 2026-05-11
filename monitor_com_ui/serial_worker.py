"""Serial communication worker thread with robust state management"""

import serial
import time
import logging
from enum import Enum
from queue import Queue, Empty

from PyQt6.QtCore import QThread, pyqtSignal

try:
    from .config import SerialConfig
except ImportError:
    from config import SerialConfig

logger = logging.getLogger(__name__)


class ThreadCommand(Enum):
    """Commands for thread state management"""
    STOP = "stop"


class SerialWorker(QThread):
    """Background thread for serial communication with device.

    State machine enforces clear transitions:
    - Thread starts in IDLE state
    - CONNECT command transitions to CONNECTING -> CONNECTED or back to IDLE
    - STOP command terminates thread immediately
    - Device disconnect detected in CONNECTED state -> RECONNECTING
    - User can interrupt RECONNECTING with STOP at any time
    """

    data_received = pyqtSignal(str)
    status_changed = pyqtSignal(str)
    connection_state = pyqtSignal(bool)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.ser = None
        self.command_queue = Queue()
        self._state = "INITIAL"

    def _set_state(self, new_state):
        """Update internal state (for debugging/monitoring)"""
        if self._state != new_state:
            logger.debug(f"State transition: {self._state} -> {new_state}")
            self._state = new_state

    def _process_commands(self):
        """Check for and process pending UI commands.

        Returns True if STOP was requested, False otherwise.
        Called frequently during waits and loops to ensure responsiveness.
        """
        try:
            while True:
                cmd = self.command_queue.get_nowait()
                if cmd == ThreadCommand.STOP:
                    logger.info("Stop command received - terminating")
                    return True
        except Empty:
            return False

    def _sleep_interruptible(self, duration_seconds):
        """Sleep that can be interrupted by STOP commands.

        Checks command queue every 100ms to allow rapid response to stop.
        Returns True if interrupted by STOP, False if full duration elapsed.
        """
        sleep_chunk = 0.1
        remaining = duration_seconds

        while remaining > 0:
            if self._process_commands():
                return True
            sleep_time = min(sleep_chunk, remaining)
            time.sleep(sleep_time)
            remaining -= sleep_time

        return False

    def _cleanup_connection(self):
        """Close serial port if open"""
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None

    def _try_open_port(self):
        """Attempt to open the serial port.

        Returns True if successful, False otherwise.
        """
        try:
            self.ser = serial.Serial(self.port, SerialConfig.BAUD_RATE, timeout=SerialConfig.TIMEOUT)
            self.ser.reset_input_buffer()
            return True
        except serial.SerialException as e:
            logger.warning(f"Failed to open port: {e}")
            self._cleanup_connection()
            return False
        except Exception as e:
            logger.warning(f"Unexpected error opening port: {e}")
            self._cleanup_connection()
            return False

    def _connect_with_retry(self):
        """Attempt to connect with exponential backoff retry.

        Returns True if connection established, False if STOP received or max retries exceeded.
        """
        self._set_state("CONNECTING")
        retry_count = 0
        current_delay = SerialConfig.INITIAL_RETRY_DELAY

        while retry_count < SerialConfig.MAX_RETRIES:
            if self._process_commands():
                logger.info("Connection attempt interrupted by user")
                return False

            if self._try_open_port():
                self.status_changed.emit(f"Connected to {self.port}")
                self.connection_state.emit(True)
                self._set_state("CONNECTED")
                return True

            retry_count += 1
            if retry_count < SerialConfig.MAX_RETRIES:
                self.status_changed.emit(
                    f"Connection failed - Retrying in {current_delay:.1f}s ({retry_count}/{SerialConfig.MAX_RETRIES})"
                )
                if self._sleep_interruptible(current_delay):
                    logger.info("Connection retry interrupted by user")
                    return False
                current_delay = min(current_delay * 2, SerialConfig.MAX_RETRY_DELAY)
            else:
                self.status_changed.emit(f"Failed to connect after {SerialConfig.MAX_RETRIES} attempts")
                self.connection_state.emit(False)
                return False

        return False

    def _handle_device_reconnection(self):
        """Handle automatic reconnection when device resets.

        Waits with exponential backoff. Can be interrupted by user STOP command.
        Returns True if reconnected, False if STOP or max retries exceeded.
        """
        self._set_state("RECONNECTING")
        self.connection_state.emit(False)
        retry_count = 0
        current_delay = SerialConfig.INITIAL_RETRY_DELAY

        logger.info(f"Starting device reconnection (max {SerialConfig.MAX_RECONNECT_RETRIES} attempts)")

        while retry_count < SerialConfig.MAX_RECONNECT_RETRIES:
            if self._process_commands():
                logger.info("Reconnection interrupted by user")
                return False

            remaining_attempts = SerialConfig.MAX_RECONNECT_RETRIES - retry_count
            self.status_changed.emit(f"Connection lost - Reconnecting in {current_delay:.1f}s... ({remaining_attempts} attempts left)")

            if self._sleep_interruptible(current_delay):
                logger.info("Reconnection wait interrupted by user")
                return False

            logger.debug(f"Reconnection attempt {retry_count + 1}/{SerialConfig.MAX_RECONNECT_RETRIES}")
            if self._try_open_port():
                logger.info(f"Reconnected to {self.port} after {retry_count + 1} attempts")
                self.status_changed.emit(f"Reconnected to {self.port}")
                self.connection_state.emit(True)
                self._set_state("CONNECTED")
                return True

            retry_count += 1
            current_delay = min(current_delay * 2, SerialConfig.MAX_RETRY_DELAY)
            logger.debug(f"Reconnection attempt {retry_count} failed, next delay: {current_delay:.1f}s")

        logger.warning(f"Max reconnection attempts ({SerialConfig.MAX_RECONNECT_RETRIES}) exceeded - giving up")
        self.connection_state.emit(False)
        self.status_changed.emit(f"Failed to reconnect after {SerialConfig.MAX_RECONNECT_RETRIES} attempts")
        return False

    def _read_serial_data(self):
        """Read and emit serial data until connection lost or STOP received.

        Returns False if STOP received, True if connection lost (should reconnect).
        """
        read_errors = 0
        max_consecutive_errors = 5
        empty_reads = 0
        max_empty_reads = 10

        try:
            while True:
                if self._process_commands():
                    return False

                if not self.ser or not self.ser.is_open:
                    logger.warning("Serial port is closed")
                    return True

                try:
                    line = self.ser.readline()
                    if line:
                        read_errors = 0
                        empty_reads = 0
                        text = line.decode('utf-8', errors='ignore')
                        text = text.rstrip('\r\n') + '\n'
                        self.data_received.emit(text)
                    else:
                        empty_reads += 1
                        if empty_reads >= max_empty_reads:
                            logger.warning(f"Port returned empty data {max_empty_reads} times - likely disconnected")
                            return True
                        time.sleep(0.01)
                except serial.SerialException as e:
                    logger.warning(f"SerialException while reading: {e}")
                    read_errors += 1
                    if read_errors >= max_consecutive_errors:
                        logger.warning(f"Too many serial exceptions ({max_consecutive_errors}), disconnecting")
                        return True
                    time.sleep(0.05)
                except UnicodeDecodeError:
                    self.data_received.emit("\n[DECODE ERROR]\n")
                    read_errors += 1
                    if read_errors >= max_consecutive_errors:
                        logger.warning(f"Too many decode errors ({max_consecutive_errors}), disconnecting")
                        return True
                except Exception as e:
                    logger.error(f"Unexpected error reading from serial: {e}")
                    read_errors += 1
                    if read_errors >= max_consecutive_errors:
                        logger.error(f"Too many errors ({max_consecutive_errors}), disconnecting")
                        return True
                    time.sleep(0.05)

        except Exception as e:
            logger.error(f"Fatal error in read loop: {e}")
            return True

    def run(self):
        """Main thread loop - state machine that responds to commands"""
        try:
            self._set_state("CONNECTING")

            while True:
                if self._state == "CONNECTING":
                    if not self._connect_with_retry():
                        break
                elif self._state == "CONNECTED":
                    should_reconnect = self._read_serial_data()
                    if not should_reconnect:
                        break
                    if not self._handle_device_reconnection():
                        break

        except Exception as e:
            logger.error(f"Unhandled thread error: {e}")
            self.status_changed.emit(f"Thread error: {e}")
            self.connection_state.emit(False)
        finally:
            self._cleanup_connection()
            self._set_state("STOPPED")

    def stop(self):
        """Signal the thread to stop immediately"""
        self.command_queue.put(ThreadCommand.STOP)
