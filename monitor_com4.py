import serial
import time
import sys

PORT = 'COM4'
BAUD = 115200

def monitor_serial():
    print(f"--- Serial Monitor started on {PORT} ({BAUD} baud) ---")
    print("--- Press Ctrl+C to exit ---")
    
    while True:
        try:
            # Try to open the serial port
            with serial.Serial(PORT, BAUD, timeout=0.1) as ser:
                print(f"\n[CONNECTED] {PORT} opened successfully.")
                while True:
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting)
                        try:
                            # Try to decode as utf-8, ignore errors for binary data
                            text = data.decode('utf-8', errors='ignore')
                            sys.stdout.write(text)
                            sys.stdout.flush()
                        except Exception as e:
                            print(f"\n[DECODE ERROR] {e}")
                    time.sleep(0.01)
        except serial.SerialException:
            # Port not found or busy
            sys.stdout.write(".")
            sys.stdout.flush()
            time.sleep(0.01)
        except KeyboardInterrupt:
            print("\n[EXITING] Monitor stopped by user.")
            break
        except Exception as e:
            print(f"\n[ERROR] {e}")
            time.sleep(1)

if __name__ == "__main__":
    # Ensure pyserial is installed: pip install pyserial
    monitor_serial()
