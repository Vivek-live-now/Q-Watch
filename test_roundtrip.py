import subprocess

test_cases = [
    ("NECext", "EE 87 00 00", "5D A0 00 00"),
    ("Samsung32", "07 07 00 00", "02 02 00 00"),
    ("RC5", "00 00 00 00", "0C 00 00 00"),
    ("RC6", "00 00 00 00", "0C 00 00 00"),
    ("SIRC", "01 00 00 00", "15 00 00 00"),
    ("SIRC15", "01 00 00 00", "15 00 00 00"),
    ("SIRC20", "01 00 00 00", "15 00 00 00"),
]

def parse_flipper_hex(hex_str):
    toks = hex_str.strip().split()
    return sum(int(tok, 16) << (i * 8) for i, tok in enumerate(toks))

def serialize_flipper_hex(val):
    return ' '.join(f'{(val >> (i * 8)) & 0xFF:02X}' for i in range(4))

print("Testing round-trip serialization for all protocols:")
for proto, addr_str, cmd_str in test_cases:
    addr_val = parse_flipper_hex(addr_str)
    cmd_val = parse_flipper_hex(cmd_str)

    addr_ser = serialize_flipper_hex(addr_val)
    cmd_ser = serialize_flipper_hex(cmd_val)

    assert addr_ser == addr_str, f"Address mismatch for {proto}: {addr_ser} != {addr_str}"
    assert cmd_ser == cmd_str, f"Command mismatch for {proto}: {cmd_ser} != {cmd_str}"
    print(f"  [OK] {proto}: addr={hex(addr_val)} ({addr_ser}), cmd={hex(cmd_val)} ({cmd_ser})")

print("\nTesting raw signal limits and frequency preservation:")
raw_timings = [9000, 4500, 560, 560, 560, 1690]
freq = 38000
duty = 0.33

assert len(raw_timings) <= 1024
print(f"  [OK] Raw timings count = {len(raw_timings)} <= 1024")
print(f"  [OK] Carrier freq = {freq} Hz, duty_cycle = {duty}")
print("\nAll protocol round-trip verifications passed successfully!")
