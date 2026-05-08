import serial
import serial.tools.list_ports
import time
import sys

BAUD = 115200
TINYUSB_VID = 0xCAFE  # TinyUSB default Vendor ID
TINYUSB_CDC_PID = 0x4000  # TinyUSB default CDC PID


def find_stm_port():
    for port in serial.tools.list_ports.comports():
        if port.vid == TINYUSB_VID and port.pid == TINYUSB_CDC_PID:
            return port.device
    return None


def monitor_serial():
    print(f"--- Serial Monitor: scanning for STM32 native USB CDC (VID=0x{TINYUSB_VID:04X} PID=0x{TINYUSB_CDC_PID:04X}, {BAUD} baud) ---")
    print("--- Press Ctrl+C to exit ---")

    current_port = None

    while True:
        try:
            port = find_stm_port()

            if port is None:
                if current_port is not None:
                    print("\n[DISCONNECTED] STM device not found. Scanning...")
                    current_port = None
                sys.stdout.write(".")
                sys.stdout.flush()
                time.sleep(1)
                continue

            if port != current_port:
                print(f"\n[FOUND] STM device on {port}")
                current_port = port

            with serial.Serial(port, BAUD, timeout=0.1) as ser:
                print(f"[CONNECTED] {port} opened successfully.")
                while True:
                    # Check device is still present
                    if find_stm_port() != port:
                        print(f"\n[DISCONNECTED] {port} lost.")
                        break
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting)
                        try:
                            text = data.decode('utf-8', errors='ignore')
                            sys.stdout.write(text)
                            sys.stdout.flush()
                        except Exception as e:
                            print(f"\n[DECODE ERROR] {e}")
                    time.sleep(0.01)

        except serial.SerialException:
            sys.stdout.write(".")
            sys.stdout.flush()
            time.sleep(1)
        except KeyboardInterrupt:
            print("\n[EXITING] Monitor stopped by user.")
            break
        except Exception as e:
            print(f"\n[ERROR] {e}")
            time.sleep(1)


if __name__ == "__main__":
    # Ensure pyserial is installed: pip install pyserial
    monitor_serial()
