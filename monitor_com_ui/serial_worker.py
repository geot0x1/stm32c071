"""Serial communication worker thread"""

import serial
import time
import logging

from PyQt6.QtCore import QThread, pyqtSignal

try:
    from .config import SerialConfig
except ImportError:
    from config import SerialConfig

logger = logging.getLogger(__name__)


class SerialWorker(QThread):
    """Background thread for serial communication with device"""

    data_received = pyqtSignal(str)
    status_changed = pyqtSignal(str)
    connection_state = pyqtSignal(bool)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = True
        self.ser = None

    def _connect_with_retry(self):
        """Attempt to connect with exponential backoff retry"""
        retry_count = 0
        current_delay = SerialConfig.INITIAL_RETRY_DELAY

        while retry_count < SerialConfig.MAX_RETRIES and self.running:
            try:
                self.ser = serial.Serial(self.port, SerialConfig.BAUD_RATE, timeout=SerialConfig.TIMEOUT)
                self.ser.reset_input_buffer()
                self.status_changed.emit(f"Connected to {self.port}")
                self.connection_state.emit(True)
                return True
            except serial.SerialException as e:
                self._cleanup_partial_connection()
                retry_count += 1
                if retry_count < SerialConfig.MAX_RETRIES:
                    self.status_changed.emit(
                        f"Connection failed: {e} - Retrying in {current_delay:.1f}s ({retry_count}/{SerialConfig.MAX_RETRIES})"
                    )
                    time.sleep(current_delay)
                    current_delay = min(current_delay * 2, SerialConfig.MAX_RETRY_DELAY)
                else:
                    self.status_changed.emit(f"Failed to connect after {SerialConfig.MAX_RETRIES} attempts: {e}")
                    self.connection_state.emit(False)
                    return False
            except Exception as e:
                self._cleanup_partial_connection()
                retry_count += 1
                if retry_count < SerialConfig.MAX_RETRIES:
                    self.status_changed.emit(
                        f"Unexpected error: {e} - Retrying in {current_delay:.1f}s ({retry_count}/{SerialConfig.MAX_RETRIES})"
                    )
                    time.sleep(current_delay)
                    current_delay = min(current_delay * 2, SerialConfig.MAX_RETRY_DELAY)
                else:
                    self.status_changed.emit(f"Failed after {SerialConfig.MAX_RETRIES} attempts: {e}")
                    self.connection_state.emit(False)
                    return False

        return False

    def _cleanup_partial_connection(self):
        """Close serial port if open"""
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None

    def run(self):
        """Main thread loop for reading serial data"""
        if not self._connect_with_retry():
            return

        try:
            while self.running:
                read_errors = 0
                max_consecutive_errors = 3

                try:
                    while self.running:
                        if not self.ser or not self.ser.is_open:
                            raise serial.SerialException("Port closed")

                        try:
                            line = self.ser.readline()
                            if line:
                                read_errors = 0
                                text = line.decode('utf-8', errors='ignore')
                                text = text.rstrip('\r\n') + '\n'
                                self.data_received.emit(text)
                            else:
                                time.sleep(0.01)
                        except serial.SerialException as e:
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise
                            time.sleep(0.05)
                        except UnicodeDecodeError:
                            self.data_received.emit(f"\n[DECODE ERROR]\n")
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise serial.SerialException("Multiple decode errors")
                        except Exception as e:
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise serial.SerialException(str(e))
                            time.sleep(0.05)

                except serial.SerialException as e:
                    self._cleanup_partial_connection()
                    if self.running:
                        self.connection_state.emit(False)
                        retry_count = 0
                        current_delay = SerialConfig.INITIAL_RETRY_DELAY

                        while self.running:
                            self.status_changed.emit(
                                f"Connection lost - Retrying in {current_delay:.1f}s..."
                            )
                            time.sleep(current_delay)

                            if not self.running:
                                break

                            try:
                                self.ser = serial.Serial(self.port, SerialConfig.BAUD_RATE, timeout=SerialConfig.TIMEOUT)
                                self.ser.reset_input_buffer()
                                self.status_changed.emit(f"Reconnected to {self.port}")
                                self.connection_state.emit(True)
                                break
                            except (serial.SerialException, Exception):
                                retry_count += 1
                                current_delay = min(current_delay * 2, SerialConfig.MAX_RETRY_DELAY)
                                if retry_count >= SerialConfig.MAX_RECONNECT_RETRIES:
                                    logger.warning(f"Max reconnection attempts exceeded for {self.port}")
                                    break
                                if not self.running:
                                    break
                        else:
                            continue

                        if self.running:
                            continue
                        else:
                            break
                    else:
                        break

        except Exception as e:
            logger.error(f"Thread error: {e}")
            self.status_changed.emit(f"Thread error: {e}")
            self.connection_state.emit(False)
        finally:
            self._cleanup_partial_connection()

    def stop(self):
        """Signal the thread to stop gracefully"""
        self.running = False
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None
