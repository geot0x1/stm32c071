#!/usr/bin/env python3
"""
test_settings.py — Verify settings command behavior on the STM32C071 thermal
controller over USB CDC.

Confirms audit findings:
    Finding #3 — individual setters (PWMTHRTEMP, FANTEMPON, TEMPCRIT) miss the
                 fan_on <-> throttle_on ordering check, allowing the
                 fan_off < fan_on <= throttle_on < critical invariant to be
                 corrupted by a single command.
    Finding #4 — parse_pwm_throttle_temp() has no range validation; values
                 above 254 or below 0 silently cast to (uint8_t) and produce
                 misleading responses.

Also surfaces the parser/setter inconsistency introduced when settings_set_all
was relaxed to permit fan_on == throttle_on: parse_settings in commands.c
still uses '>=' and rejects the equality case.

Output policy:
    * Each command is printed (CMD).
    * Only response lines (OK / ERR / SETTINGS= / FWVER=) are printed (RSP).
      Telemetry frames, byte echo, and [CMD]-debug lines are filtered out.
    * Each test prints [EXPECTED] or [UNEXPECTED] with a one-line explanation.
    * 1 s wait between commands.

Usage:
    python test_settings.py [COM_PORT]
    (default COM_PORT = COM5)
"""

import sys
import time
from dataclasses import dataclass
from typing import Callable, List

try:
    import serial
except ImportError:
    sys.exit("ERROR: pyserial is required.  Install with:  pip install pyserial")


DEFAULT_PORT = "COM9"
BAUD = 115200
COMMAND_INTERVAL_S = 1.0
READ_BUF = 4096


def is_response_line(line: str) -> bool:
    """Keep only command responses; drop telemetry, byte echo, and [CMD] debug."""
    line = line.strip()
    if not line:
        return False
    return (line.startswith("OK ")
            or line.startswith("ERR ")
            or line.startswith("SETTINGS=")
            or line.startswith("FWVER="))


def send_and_capture(ser, cmd: str) -> List[str]:
    """Send `cmd` + CRLF, wait 1 s, return filtered response lines."""
    ser.reset_input_buffer()
    print(f"\n>>> CMD:  {cmd}")
    ser.write((cmd + "\r\n").encode("ascii"))
    time.sleep(COMMAND_INTERVAL_S)
    raw = ser.read(READ_BUF).decode("ascii", errors="replace")
    lines = [l.strip() for l in raw.splitlines()]
    relevant = [l for l in lines if is_response_line(l)]
    if relevant:
        for l in relevant:
            print(f"<<< RSP:  {l}")
    else:
        print(f"<<< RSP:  (no response line within {COMMAND_INTERVAL_S:.1f}s)")
    return relevant


@dataclass
class Test:
    cmd: str
    check: Callable[[List[str]], bool]
    explanation: str


def has_substring(s: str) -> Callable[[List[str]], bool]:
    return lambda r: any(s in x for x in r)


def has_settings_state(state: str) -> Callable[[List[str]], bool]:
    return lambda r: any(x == f"SETTINGS={state}" for x in r)


def starts_with_err(r: List[str]) -> bool:
    return any(x.startswith("ERR") for x in r)


def has_out_of_range(r: List[str]) -> bool:
    return any("OUT_OF_RANGE" in x for x in r)


def build_tests() -> List[Test]:
    return [
        # --------------------------------------------------------------
        # Phase 1 : baseline
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset to defaults should be accepted."),
        Test("SETTINGS?",
             has_settings_state("30,35,40,60,50,50"),
             "Defaults: fan_off=30, fan_on=35, throttle_on=40, critical=60, pwm_a=50, pwm_b=50."),

        # --------------------------------------------------------------
        # Phase 2 : valid setter usage (sanity)
        # --------------------------------------------------------------
        Test("PWMTHR=A,75",
             has_substring("OK SETTINGSCHANGE PWMTHR A 75"),
             "Valid PWM throttle A update should be accepted."),
        Test("FANTEMPON=36",
             has_substring("OK SETTINGSCHANGE FANTEMPON 36"),
             "Valid fan_on=36 (between fan_off=30 and critical=60) should be accepted."),
        Test("SETTINGS?",
             has_settings_state("30,36,40,60,75,50"),
             "State should reflect the two valid updates above."),

        # --------------------------------------------------------------
        # Phase 3 : Finding #3 — individual-setter invariant gaps
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before Finding #3a."),
        Test("PWMTHRTEMP=32",
             starts_with_err,
             "Finding #3a: throttle_on=32 violates fan_on(35) <= throttle_on. "
             "Device SHOULD reject with ERR. If OK is returned, Finding #3 is confirmed."),
        Test("SETTINGS?",
             has_settings_state("30,35,40,60,50,50"),
             "Finding #3a: state should be unchanged if the previous command was rejected. "
             "If state shows throttle_on=32, the invariant has been corrupted by a single command."),

        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before Finding #3b."),
        Test("FANTEMPON=45",
             starts_with_err,
             "Finding #3b: fan_on=45 violates fan_on <= throttle_on(40). "
             "Device SHOULD reject with ERR. If OK, Finding #3 confirmed."),
        Test("SETTINGS?",
             has_settings_state("30,35,40,60,50,50"),
             "Finding #3b: state should be unchanged if rejected. "
             "If fan_on=45 in state, invariant corrupted."),

        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before Finding #3c."),
        Test("TEMPCRIT=39",
             starts_with_err,
             "Finding #3c: critical=39 violates throttle_on(40) < critical. "
             "Device SHOULD reject with ERR. If OK, Finding #3 confirmed."),
        Test("SETTINGS?",
             has_settings_state("30,35,40,60,50,50"),
             "Finding #3c: state should be unchanged if rejected. "
             "If critical=39 in state, invariant corrupted."),

        # --------------------------------------------------------------
        # Phase 4 : Finding #4 — PWMTHRTEMP missing range validation
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before Finding #4 tests."),
        Test("PWMTHRTEMP=300",
             has_out_of_range,
             "Finding #4a: 300 is above the uint8 range. Parser SHOULD respond ERR OUT_OF_RANGE. "
             "If OK or SAVE_FAILED, Finding #4 confirmed — value silently casts to (uint8_t)300 = 44."),
        Test("PWMTHRTEMP=-5",
             has_out_of_range,
             "Finding #4b: negative value. Parser SHOULD respond ERR OUT_OF_RANGE. "
             "If SAVE_FAILED, Finding #4 confirmed — value casts to 251."),
        Test("PWMTHRTEMP=0",
             lambda r: any("ORDERING" in x for x in r),
             "Finding #4c: zero is in the [0,150] range but below FANTEMPON=35. "
             "Parser SHOULD respond ERR ORDERING (not OUT_OF_RANGE, not SAVE_FAILED). "
             "Verifies that the new ordering gate in parse_pwm_throttle_temp() catches invariant violations with a meaningful message."),

        # --------------------------------------------------------------
        # Phase 5 : new invariant — parser/setter inconsistency
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before Phase 5."),
        Test("SETTINGS=30,35,35,60,50,50",
             has_substring("OK SETTINGSCHANGE SETTINGS"),
             "New invariant permits fan_on == throttle_on (35 == 35). Parser SHOULD accept. "
             "If ERR ORDERING, commands.c parse_settings still uses '>=' (inconsistent with the relaxed setter)."),
        Test("SETTINGS=30,40,35,60,50,50",
             starts_with_err,
             "Sanity: fan_on=40 > throttle_on=35 still violates the invariant. Parser SHOULD reject."),

        # --------------------------------------------------------------
        # Phase 6 : PWMTHR boundary & format tests (range 0..100, channel A|B)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before PWMTHR boundary tests."),
        Test("PWMTHR=A,0",
             has_substring("OK SETTINGSCHANGE PWMTHR A 0"),
             "Boundary: 0%% is the minimum throttle duty cycle. Should be accepted."),
        Test("PWMTHR=B,100",
             has_substring("OK SETTINGSCHANGE PWMTHR B 100"),
             "Boundary: 100%% is the maximum throttle duty cycle. Should be accepted."),
        Test("PWMTHR=A,101",
             has_out_of_range,
             "One above max: 101 > 100. Parser SHOULD respond ERR OUT_OF_RANGE."),
        Test("PWMTHR=A,-1",
             has_out_of_range,
             "Below min: -1 < 0. Parser SHOULD respond ERR OUT_OF_RANGE."),
        Test("PWMTHR=C,50",
             lambda r: any("INVALID_CHANNEL" in x for x in r),
             "Invalid channel 'C' (only A/B allowed). Parser SHOULD respond ERR INVALID_CHANNEL."),
        Test("PWMTHR=A50",
             lambda r: any("INVALID_FORMAT" in x for x in r),
             "Missing comma separator. Parser SHOULD respond ERR INVALID_FORMAT."),
        Test("SETTINGS?",
             has_settings_state("30,35,40,60,0,100"),
             "Verify the two valid PWMTHR updates above stuck (pwm_a=0, pwm_b=100)."),

        # --------------------------------------------------------------
        # Phase 7 : PWMTHRTEMP boundary tests (range 0..150)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before PWMTHRTEMP boundary tests."),
        Test("PWMTHRTEMP=35",
             has_substring("OK SETTINGSCHANGE PWMTHRTEMP 35"),
             "Weak invariant: throttle_on=35 == fan_on=35 should be accepted (new rule)."),
        Test("SETTINGS?",
             has_settings_state("30,35,35,60,50,50"),
             "State should reflect fan_on == throttle_on = 35."),
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset to defaults."),
        Test("PWMTHRTEMP=60",
             lambda r: any("ORDERING" in x for x in r),
             "60 == critical(60). Parser SHOULD respond ERR ORDERING (must be < critical)."),
        Test("PWMTHRTEMP=151",
             has_out_of_range,
             "Boundary: 151 > CMD_PWMTHRTEMP_MAX(150). Parser SHOULD respond ERR OUT_OF_RANGE."),
        Test("PWMTHRTEMP=255",
             has_out_of_range,
             "Boundary: 255 well above max. Parser SHOULD respond ERR OUT_OF_RANGE (not coincidentally match SETTINGS_TEMP_INVALID)."),

        # --------------------------------------------------------------
        # Phase 8 : FANTEMPON boundary tests (range 0..150)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before FANTEMPON boundary tests."),
        Test("FANTEMPON=30",
             lambda r: any("ORDERING" in x for x in r),
             "30 == fan_off(30). Parser SHOULD reject with ERR ORDERING (must be > fan_off)."),
        Test("FANTEMPON=60",
             lambda r: any("ORDERING" in x for x in r),
             "60 == critical(60). Parser SHOULD reject with ERR ORDERING (must be < critical)."),
        Test("FANTEMPON=151",
             has_out_of_range,
             "151 > max(150). Parser SHOULD reject with ERR OUT_OF_RANGE."),
        Test("FANTEMPON=40",
             has_substring("OK SETTINGSCHANGE FANTEMPON 40"),
             "Weak invariant: fan_on=40 == throttle_on=40 should be accepted at the setter."),
        Test("SETTINGS?",
             has_settings_state("30,40,40,60,50,50"),
             "State should reflect fan_on == throttle_on = 40."),
        Test("FANTEMPON=41",
             lambda r: any(x.startswith("ERR") for x in r),
             "41 > throttle_on=40 violates weak invariant. Setter SHOULD reject "
             "(parser misses this — SAVE_FAILED is acceptable until parser also gets the check)."),

        # --------------------------------------------------------------
        # Phase 9 : FANTEMPOFF boundary tests (range 0..150)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before FANTEMPOFF boundary tests."),
        Test("FANTEMPOFF=0",
             has_substring("OK SETTINGSCHANGE FANTEMPOFF 0"),
             "Boundary: 0 is the minimum, well below fan_on=35. Should be accepted."),
        Test("SETTINGS?",
             has_settings_state("0,35,40,60,50,50"),
             "State should reflect fan_off=0."),
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset."),
        Test("FANTEMPOFF=34",
             has_substring("OK SETTINGSCHANGE FANTEMPOFF 34"),
             "Boundary: 34 = fan_on(35) - 1, the largest valid fan_off. Should be accepted."),
        Test("FANTEMPOFF=35",
             lambda r: any("ORDERING" in x for x in r),
             "35 == fan_on(35) violates fan_off < fan_on (strict). Parser SHOULD reject with ERR ORDERING."),
        Test("FANTEMPOFF=151",
             has_out_of_range,
             "151 > max(150). Parser SHOULD respond ERR OUT_OF_RANGE."),

        # --------------------------------------------------------------
        # Phase 10 : TEMPCRIT boundary tests (range 0..150)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before TEMPCRIT boundary tests."),
        Test("TEMPCRIT=35",
             lambda r: any("ORDERING" in x for x in r),
             "35 == fan_on(35). Parser SHOULD reject with ERR ORDERING (must be > fan_on)."),
        Test("TEMPCRIT=40",
             lambda r: any(x.startswith("ERR") for x in r),
             "40 == throttle_on(40) violates strict throttle_on < critical. "
             "Setter SHOULD reject (parser misses; SAVE_FAILED acceptable)."),
        Test("TEMPCRIT=41",
             has_substring("OK SETTINGSCHANGE TEMPCRIT 41"),
             "Boundary: 41 = throttle_on(40) + 1, the smallest valid critical. Should be accepted."),
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset."),
        Test("TEMPCRIT=150",
             has_substring("OK SETTINGSCHANGE TEMPCRIT 150"),
             "Boundary: 150 is the maximum allowed critical temperature. Should be accepted."),
        Test("TEMPCRIT=151",
             has_out_of_range,
             "151 > max(150). Parser SHOULD respond ERR OUT_OF_RANGE."),

        # --------------------------------------------------------------
        # Phase 11 : SETTINGS= bulk edge cases (each field 0..254, throttle 0..100)
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Reset before bulk edge tests."),
        Test("SETTINGS=0,1,1,2,0,0",
             has_substring("OK SETTINGSCHANGE SETTINGS"),
             "Minimal valid invariant: fan_off=0 < fan_on=1 == throttle_on=1 < critical=2. Should be accepted."),
        Test("SETTINGS?",
             has_settings_state("0,1,1,2,0,0"),
             "State should reflect the minimal valid invariant."),
        Test("SETTINGS=200,201,201,202,100,100",
             has_substring("OK SETTINGSCHANGE SETTINGS"),
             "High-end invariant: bulk parser allows 0..254 (wider than individual setters' 0..150). "
             "Should be accepted. NOTE: this exposes the inconsistency between bulk parser's 0..254 range "
             "and individual setters' 0..150 range — both are accessible to the host."),
        Test("SETTINGS?",
             has_settings_state("200,201,201,202,100,100"),
             "State should reflect the high-end invariant."),
        Test("SETTINGS=0,0,0,0,0,0",
             lambda r: any("ORDERING" in x for x in r),
             "All zeros: fan_off=0 >= fan_on=0. Parser SHOULD reject with ERR ORDERING."),
        Test("SETTINGS=255,30,40,60,50,50",
             has_out_of_range,
             "255 > 254 (also collides with SETTINGS_TEMP_INVALID). Parser SHOULD reject with ERR OUT_OF_RANGE."),
        Test("SETTINGS=30,35,40,60,101,50",
             has_out_of_range,
             "throttle_a=101 > 100. Parser SHOULD reject with ERR OUT_OF_RANGE."),
        Test("SETTINGS=30,35,40,60,50",
             lambda r: any("INVALID_VALUE" in x or "ORDERING" in x for x in r),
             "Missing the 6th field (throttle_b). strtok yields NULL for the missing field — "
             "parser SHOULD reject with ERR INVALID_VALUE."),

        # --------------------------------------------------------------
        # Phase 12 : final cleanup
        # --------------------------------------------------------------
        Test("SETDEFAULT",
             has_substring("OK SETTINGSCHANGE SETDEFAULT"),
             "Final reset to defaults."),
    ]


def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    print(f"Opening {port} @ {BAUD} baud …")
    try:
        ser = serial.Serial(port, BAUD, timeout=0.1)
    except serial.SerialException as e:
        sys.exit(f"ERROR: cannot open {port}: {e}\n"
                 f"Usage:  python {sys.argv[0]} [COM_PORT]")

    time.sleep(0.5)  # let the port settle and discard any in-flight telemetry
    tests = build_tests()
    print(f"\nRunning {len(tests)} tests "
          f"(1 s between commands, telemetry filtered)\n")

    expected = 0
    unexpected = 0
    try:
        for i, t in enumerate(tests, start=1):
            print(f"--- Test {i}/{len(tests)} ---")
            responses = send_and_capture(ser, t.cmd)
            passed = t.check(responses)
            tag = "[EXPECTED]" if passed else "[UNEXPECTED]"
            print(f"{tag}  {t.explanation}")
            if passed:
                expected += 1
            else:
                unexpected += 1
    except KeyboardInterrupt:
        print("\n[interrupted]")
    finally:
        ser.close()

    print(f"\nSummary: {expected} expected, {unexpected} unexpected "
          f"(unexpected = audit-finding confirmation).")


if __name__ == "__main__":
    main()
