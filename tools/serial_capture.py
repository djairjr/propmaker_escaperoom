"""
Captures serial output and saves it to a file.
Detects the COM port automatically via arduino-cli.

Usage:
    python tools/serial_capture.py              # Detects the port automatically
    python tools/serial_capture.py -p COM8      # Uses a specific port
    python tools/serial_capture.py -o log.txt   # Saves the log to a file
"""

import serial
import subprocess
import re
import time
from datetime import datetime
from pathlib import Path


def detect_rp2040_port():
    """
    Automatically detects the COM port of the Adafruit Feather RP2040
    using arduino-cli board list.

    Returns:
        str: Detected COM port (e.g. "COM8") or "COM3" as fallback
    """
    try:
        result = subprocess.run(
            ["arduino-cli", "board", "list"],
            capture_output=True,
            text=True,
            check=True,
            timeout=10
        )
        output = result.stdout

        # Look for a line with RP2040 and extract the COM port
        for line in output.splitlines():
            lower_line = line.lower()
            if "rp2040" in lower_line or "adafruit" in lower_line:
                match = re.search(r"(COM\d+)", line, re.IGNORECASE)
                if match:
                    port = match.group(1)
                    print(f"RP2040 detected on {port}")
                    return port

        # Fallback: look for any USB serial port
        for line in output.splitlines():
            if "serial" in line.lower() and "usb" in line.lower():
                match = re.search(r"(COM\d+)", line, re.IGNORECASE)
                if match:
                    port = match.group(1)
                    print(f"Generic USB port on {port}")
                    return port

        print("No RP2040 detected. Using COM3 as fallback.")
        return "COM3"

    except subprocess.CalledProcessError as e:
        print(f"ERROR running arduino-cli: {e}")
        return "COM3"
    except FileNotFoundError:
        print("ERROR: arduino-cli not found. Using COM3.")
        return "COM3"
    except subprocess.TimeoutExpired:
        print("ERROR: Timeout running arduino-cli. Using COM3.")
        return "COM3"


def capture_serial(port=None, baud=115200, timeout=5, output_file=None, duration=None):
    """
    Captures serial output and saves it to a file.

    Args:
        port: COM port (detects automatically if None)
        baud: Baud rate (default: 115200)
        timeout: Read timeout in seconds
        output_file: File to save the log to
        duration: Duration in seconds (None = infinite)
    """
    port = port if port else detect_rp2040_port()

    try:
        ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(1)  # Waits for the board reset

        print(f"Monitoring {port} @ {baud}")
        print("-" * 50)

        lines = []
        start_time = time.time()
        last_activity = start_time

        while True:
            # Check duration
            if duration and (time.time() - start_time) > duration:
                print(f"\nTime expired ({duration}s)")
                break

            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    formatted = f"[{timestamp}] {line}"
                    print(formatted)
                    lines.append(formatted)
                    last_activity = time.time()

            # Inactivity timeout (30s)
            if time.time() - last_activity > 30:
                print("\nInactivity timeout (30s)")
                break

        ser.close()

        # Save to file if specified
        if output_file and lines:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))
            print(f"Log saved to {output_file}")

        return lines

    except serial.SerialException as e:
        print(f"ERROR: {e}")
        print("Check that the board is connected and the COM port is correct.")
        print("Tip: Close the Arduino IDE or other serial monitors")
        return []


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Captures serial output from an Arduino RP2040"
    )
    parser.add_argument(
        "-p", "--port",
        default=None,
        help="Serial port (e.g. COM8). If omitted, detects automatically"
    )
    parser.add_argument(
        "-b", "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "-o", "--output",
        help="Output file to save the log to"
    )
    parser.add_argument(
        "-d", "--duration",
        type=int,
        help="Duration in seconds"
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Only list boards and exit"
    )

    args = parser.parse_args()

    if args.list:
        subprocess.run(["arduino-cli", "board", "list"])
        exit(0)

    capture_serial(
        port=args.port,
        baud=args.baud,
        output_file=args.output,
        duration=args.duration
    )
