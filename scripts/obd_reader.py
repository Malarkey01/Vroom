#!/usr/bin/env python3
"""
obd_reader.py ― Polls the car's OBD-II bus and streams data to stdout
====================================================================

Every 0.5 s the script:

1. Queries a small set of useful PIDs (RPM, speed, etc.).
2. Builds a JSON dictionary: {"RPM": 814, "SPEED": 0, …}.
3. Writes the JSON as **one line** to stdout and flushes immediately.
"""

import json
import time
import sys
import signal
import obd   # python-OBD auto-detects the first /dev/ttyUSB* ELM327 device

# Exit cleanly when the parent C program sends SIGINT (e.g. on window close)
signal.signal(signal.SIGINT, lambda *_: sys.exit(0))

# Open the OBD-II serial link (blocking until the adapter is ready)
connection = obd.OBD(fast=False, baudrate=115200, timeout=1)

# PID set for the dashboard
PIDS = {
    "RPM":                     obd.commands.RPM,
    "SPEED":                   obd.commands.SPEED,
    "ENGINE LOAD":             obd.commands.ENGINE_LOAD,
    "THROTTLE POSITION":       obd.commands.THROTTLE_POS,
    "INTAKE PRESSURE":         obd.commands.INTAKE_PRESSURE,
    "TIMING ADVANCE":          obd.commands.TIMING_ADVANCE,
    "FUEL LEVEL":              obd.commands.FUEL_LEVEL,
    "CONTROL MODULE VOLTAGE":  obd.commands.CONTROL_MODULE_VOLTAGE,
}

while True:
    frame = {}
    for label, cmd in PIDS.items():
        result = connection.query(cmd, force=True)   # ask ECU every time
        if not result.is_null():
            val = result.value
            # python-OBD sometimes returns a Pint Quantity; strip the unit
            frame[label] = (
                float(val.magnitude) if hasattr(val, "magnitude") else val
            )
    print(json.dumps(frame), flush=True)  # one atomic JSON line
    time.sleep(0.5)
