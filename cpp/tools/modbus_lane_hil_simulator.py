# File: cpp/tools/modbus_lane_hil_simulator.py

import socket
import struct
import time

HOST = "127.0.0.1"
PORT = 1502

def request(sock, transaction_id, pdu):
    packet = struct.pack(">HHHB", transaction_id, 0, len(pdu) + 1, 1) + pdu
    sock.sendall(packet)
    header = sock.recv(9)
    if len(header) < 9:
        raise RuntimeError("incomplete Modbus response")
    length = struct.unpack(">H", header[4:6])[0]
    payload = header[7:]
    while len(payload) < length - 1:
        payload += sock.recv(length - 1 - len(payload))
    return payload

def write_telemetry(sock, transaction_id, power_w, temperature_c, water_quality):
    registers = [power_w, round(temperature_c * 100), round(water_quality * 1000)]
    values = b"".join(struct.pack(">H", value) for value in registers)
    pdu = struct.pack(">BHHB", 16, 0, len(registers), len(values)) + values
    request(sock, transaction_id, pdu)

def read_advisory(sock, transaction_id):
    payload = request(sock, transaction_id, struct.pack(">BHH", 4, 0, 2))
    if payload[0] != 4 or payload[1] != 4:
        raise RuntimeError("invalid advisory response")
    decision, advisory_only = struct.unpack(">HH", payload[2:6])
    if advisory_only != 1:
        raise RuntimeError("server did not identify advisory-only operation")
    return ("PROCEED", "DERATE", "HALT")[decision]

with socket.create_connection((HOST, PORT), timeout=5) as connection:
    for transaction_id, telemetry in enumerate(
        [(160, 29.0, 0.92), (340, 37.0, 0.78), (620, 44.0, 0.40)], start=1
    ):
        write_telemetry(connection, transaction_id, *telemetry)
        print({"telemetry": telemetry, "advisory": read_advisory(connection, transaction_id + 100)})
        time.sleep(0.25)
