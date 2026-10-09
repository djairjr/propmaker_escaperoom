"""
Serial monitor with real-time analysis for Arduino tests.
Detects the COM port automatically via arduino-cli.

Usage:
    python tools/serial_monitor.py              # Detects the port automatically
    python tools/serial_monitor.py -p COM8      # Uses a specific port
    python tools/serial_monitor.py -d 60        # Monitors for 60 seconds
    python tools/serial_monitor.py -o log.txt   # Saves the log to a file
"""

import serial
import subprocess
import re
import sys
from datetime import datetime
from collections import defaultdict
from pathlib import Path

# Force UTF-8 encoding on Windows
if sys.platform == 'win32':
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'strict')


# Patterns for automatic detection
PATTERNS = {
    'success': [
        r'\[HWTEST\] All Hardware Diagnostics Complete',
        r'System Healthy:\s*YES',
        r'✅.*SUCCESS',
    ],
    'error': [
        r'\[HWTEST\].*FAIL',
        r'ERROR',
        r'assertion failed',
        r'HardFault',
        r'panic',
    ],
    'warning': [
        r'\[AUDIO\].*Not Available',
        r'\[HWTEST\].*SKIP',
        r'⚠️.*WARNING',
    ],
    'audio': [
        r'\[AUDIO\]',
    ],
    'memory': [
        r'Heap.*\d+',
        r'Stack.*\d+',
    ],
}


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
                    print(f"✅ RP2040 detected on {port}")
                    return port

        # Fallback: look for any USB serial port
        for line in output.splitlines():
            if "serial" in line.lower() and "usb" in line.lower():
                match = re.search(r"(COM\d+)", line, re.IGNORECASE)
                if match:
                    port = match.group(1)
                    print(f"⚠️  Generic USB port on {port}")
                    return port

        print("⚠️  No RP2040 detected. Using COM3 as fallback.")
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


class SerialMonitor:
    """Serial monitor with real-time analysis."""

    def __init__(self, port=None, baud=115200, timeout=2):
        # Detect the port automatically if not specified
        self.port = port if port else detect_rp2040_port()
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        self.stats = defaultdict(int)
        self.lines = []
        self.start_time = None
        self.last_activity = None

    def connect(self):
        """Connects to the serial port."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
            # Waits for the board reset (RP2040) - reduced to 1s
            import time
            time.sleep(1)
            print(f"Connected to {self.port} @ {self.baud}")
            return True
        except serial.SerialException as e:
            print(f"Error connecting: {e}")
            print(f"   Check that the board is connected on {self.port}")
            print(f"   Tip: Close the Arduino IDE or other serial monitors")
            return False

    def classify_line(self, line):
        """Classifies a line by patterns."""
        for category, patterns in PATTERNS.items():
            for pattern in patterns:
                if re.search(pattern, line, re.IGNORECASE):
                    return category
        return 'info'

    def parse_line(self, line):
        """Parses a line and updates the statistics."""
        category = self.classify_line(line)
        self.stats[category] += 1

        # Format output with colors (ANSI)
        colors = {
            'success': '\033[92m',  # Green
            'error': '\033[91m',    # Red
            'warning': '\033[93m',  # Yellow
            'audio': '\033[96m',    # Cyan
            'memory': '\033[95m',   # Magenta
        }
        reset = '\033[0m'
        color = colors.get(category, '')

        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        prefix = {
            'success': '✅',
            'error': '❌',
            'warning': '⚠️',
            'audio': '🔊',
            'memory': '💾',
        }.get(category, '  ')

        formatted = f"{color}{prefix} [{timestamp}] {line}{reset}"
        print(formatted)
        self.lines.append(f"[{timestamp}] {line}")

        # Detect crash (HardFault, panic)
        if re.search(r'HardFault|panic', line, re.IGNORECASE):
            print("\n🔴 CRASH DETECTED! Saving log...")
            self.save_log("crash_log.txt")

    def save_log(self, filename):
        """Saves the log to a file."""
        with open(filename, 'w', encoding='utf-8') as f:
            f.write('\n'.join(self.lines))
        print(f"📄 Log saved to {filename}")

    def print_summary(self):
        """Prints the statistics summary."""
        print("\n" + "=" * 50)
        print("📊 TEST SUMMARY")
        print("=" * 50)
        print(f"Duration: {datetime.now() - self.start_time}")
        print(f"Lines captured: {len(self.lines)}")
        print(f"\nBy category:")
        for cat, count in sorted(self.stats.items()):
            emoji = {'success': '✅', 'error': '❌', 'warning': '⚠️'}.get(cat, '  ')
            print(f"  {emoji} {cat}: {count}")

        # Verdict
        if self.stats['error'] > 0:
            print("\n🔴 TEST FAILED - Errors detected")
        elif self.stats['success'] > 0:
            print("\n✅ TEST SUCCEEDED")
        else:
            print("\n⚠️  INCONCLUSIVE TEST")

    def run(self, duration=None, output_file=None):
        """
        Runs the monitoring.

        Args:
            duration: Duration in seconds (None = infinite)
            output_file: File to save the log to
        """
        if not self.connect():
            return

        self.start_time = datetime.now()
        self.last_activity = self.start_time

        print("\nMonitoring... (Ctrl+C to stop)")
        print("-" * 50)

        try:
            while True:
                # Check timeout
                if duration and (datetime.now() - self.start_time).total_seconds() > duration:
                    print(f"\n⏱️  Time expired ({duration}s)")
                    break

                if self.ser.in_waiting > 0:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.parse_line(line)
                        self.last_activity = datetime.now()

                # Inactivity timeout (60s)
                if (datetime.now() - self.last_activity).total_seconds() > 60:
                    print("\n⏱️  Inactivity timeout (60s)")
                    break

        except KeyboardInterrupt:
            print("\n\n⏹️  Interrupted by the user")
        finally:
            if self.ser:
                self.ser.close()

        self.print_summary()

        if output_file:
            self.save_log(output_file)


def list_boards():
    """Lists connected Arduino boards."""
    print("Connected boards:\n")
    subprocess.run(["arduino-cli", "board", "list"])


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Serial monitor with analysis for Arduino RP2040"
    )
    parser.add_argument(
        "-p", "--port",
        default=None,
        help="Serial port (e.g. COM8). If omitted, detects automatically via arduino-cli"
    )
    parser.add_argument(
        "-b", "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "-d", "--duration",
        type=int,
        help="Duration in seconds (optional)"
    )
    parser.add_argument(
        "-o", "--output",
        help="Output file to save the log to"
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Only list connected boards and exit"
    )

    args = parser.parse_args()

    if args.list:
        list_boards()
        exit(0)

    monitor = SerialMonitor(port=args.port, baud=args.baud)
    monitor.run(duration=args.duration, output_file=args.output)
