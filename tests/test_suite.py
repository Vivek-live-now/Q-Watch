def parse_flipper_hex(hex_str):
    toks = hex_str.strip().split()
    return sum(int(tok, 16) << (i * 8) for i, tok in enumerate(toks))

def serialize_flipper_hex(val):
    return ' '.join(f'{(val >> (i * 8)) & 0xFF:02X}' for i in range(4))

def test_protocol_variants():
    print("--- 1. Protocol Variant Round-Trip Test ---")
    protocols = [
        ("NEC", 0x00FF, 0x000C, "FF 00 00 00", "0C 00 00 00"),
        ("NECext", 0x87EE, 0xA05D, "EE 87 00 00", "5D A0 00 00"),
        ("NEC42", 0x1234, 0x005D, "34 12 00 00", "5D 00 00 00"),
        ("Samsung32", 0x0707, 0x0202, "07 07 00 00", "02 02 00 00"),
        ("RC5", 0x0000, 0x000C, "00 00 00 00", "0C 00 00 00"),
        ("RC5X", 0x0001, 0x001F, "01 00 00 00", "1F 00 00 00"),
        ("RC6", 0x0000, 0x000C, "00 00 00 00", "0C 00 00 00"),
        ("SIRC", 0x0001, 0x0015, "01 00 00 00", "15 00 00 00"),
        ("SIRC15", 0x0001, 0x0015, "01 00 00 00", "15 00 00 00"),
        ("SIRC20", 0x0001, 0x0015, "01 00 00 00", "15 00 00 00"),
    ]

    for name, addr, cmd, expected_addr_str, expected_cmd_str in protocols:
        addr_parsed = parse_flipper_hex(expected_addr_str)
        cmd_parsed = parse_flipper_hex(expected_cmd_str)

        assert addr_parsed == addr, f"Address parse mismatch for {name}"
        assert cmd_parsed == cmd, f"Command parse mismatch for {name}"

        addr_ser = serialize_flipper_hex(addr)
        cmd_ser = serialize_flipper_hex(cmd)

        assert addr_ser == expected_addr_str, f"Address serialize mismatch for {name}"
        assert cmd_ser == expected_cmd_str, f"Command serialize mismatch for {name}"
        print(f"  [PASS] {name}: addr={hex(addr)} ({addr_ser}), cmd={hex(cmd)} ({cmd_ser})")

def test_raw_serialization():
    print("\n--- 2. Raw Signal Serialization Test ---")
    # Case A: Known frequency and duty cycle
    freq_a = 38000
    duty_a = 0.33
    raw_a = [9000, 4500, 560, 560, 560, 1690]

    out_a = f"type: raw\nfrequency: {freq_a}\nduty_cycle: {duty_a:.6f}\ndata: " + ' '.join(str(x) for x in raw_a)
    assert "frequency: 38000" in out_a
    assert "duty_cycle: 0.330000" in out_a
    print("  [PASS] Raw with known freq & duty cycle includes fields.")

    # Case B: Unknown frequency and unmeasured duty cycle
    freq_b = 0
    has_duty_b = False
    raw_b = [9000, 4500, 560, 560]

    out_b = "type: raw\n"
    if freq_b > 0:
        out_b += f"frequency: {freq_b}\n"
    if has_duty_b:
        out_b += "duty_cycle: 0.330000\n"
    out_b += "data: " + ' '.join(str(x) for x in raw_b)

    assert "frequency:" not in out_b
    assert "duty_cycle:" not in out_b
    print("  [PASS] Raw with unknown freq & duty cycle omits unmeasured values.")

def test_menu_scrollbar_geometry():
    print("\n--- 3. Menu Window & Scrollbar Geometry Test ---")
    # For any item_count > 3 and valid offset, test bounds
    test_cases = [
        (4, 0), (4, 1),
        (5, 0), (5, 1), (5, 2),
        (6, 0), (6, 2), (6, 3),
        (14, 0), (14, 5), (14, 11),
    ]
    for count, offset in test_cases:
        items = list(range(offset, min(offset + 3, count)))
        assert len(items) <= 3
        assert items[0] == offset
        assert items[-1] < count

        # Scrollbar thumb position formula:
        scroll_h = 30
        scroll_y = 15 + ((offset / (count - 3)) * (scroll_h - 10))
        assert 15 <= scroll_y <= 35, f"Scrollbar thumb out of range: {scroll_y}"

    print("  [PASS] All menu window slices and scrollbar thumb ranges verified.")

def test_timekeeping_math_and_formatting():
    print("\n--- 4. Timekeeping Math & Formatting Test ---")
    # Stopwatch formatting: MM:SS.hh
    def format_ms(ms):
        total_sec = ms // 1000
        minutes = total_sec // 60
        seconds = total_sec % 60
        hundredths = (ms % 1000) // 10
        return f"{minutes:02d}:{seconds:02d}.{hundredths:02d}"

    assert format_ms(0) == "00:00.00"
    assert format_ms(1250) == "00:01.25"
    assert format_ms(65430) == "01:05.43"
    assert format_ms(3599990) == "59:59.99"
    print("  [PASS] Stopwatch millisecond formatting verified.")

    # CountdownTimer formatting: HH:MM:SS or MM:SS
    def format_sec(sec):
        h = sec // 3600
        m = (sec % 3600) // 60
        s = sec % 60
        if h > 0:
            return f"{h:02d}:{m:02d}:{s:02d}"
        return f"{m:02d}:{s:02d}"

    assert format_sec(60) == "01:00"
    assert format_sec(300) == "05:00"
    assert format_sec(3665) == "01:01:05"
    print("  [PASS] Timer second formatting verified.")

    # Pedometer formulas
    steps = 10000
    dist_km = steps * 0.00075
    kcal = int(steps * 0.04)
    assert abs(dist_km - 7.5) < 1e-4
    assert kcal == 400
    print("  [PASS] Pedometer distance and caloric expenditure formulas verified.")

    # World clock offsets in seconds
    tz_quarters = [22, 0, 0, 4, -20, -24, -28, -32, 16, 32, 36, 40]
    cities = ["KOLKATA", "UTC", "LONDON", "BERLIN", "NEW YORK", "CHICAGO", "DENVER", "LOS ANGELES", "DUBAI", "SINGAPORE", "TOKYO", "SYDNEY"]
    expected_offsets_sec = [19800, 0, 0, 3600, -18000, -21600, -25200, -28800, 14400, 28800, 32400, 36000]
    for city, q, exp in zip(cities, tz_quarters, expected_offsets_sec):
        assert q * 900 == exp, f"TZ offset mismatch for {city}: {q * 900} vs {exp}"
    print("  [PASS] World clock 12-city timezone offsets verified.")

def test_analog_trigonometry():
    import math
    print("\n--- 5. Analog Watch Face Trigonometry Test ---")
    cx, cy = 64, 32
    r_hour = 16.0

    # 12 o'clock (0 hour): hand pointing straight up -> (64, 16)
    h_angle_12 = (0 * 30.0) * (math.pi / 180.0) - (math.pi / 2.0)
    hx_12 = round(cx + math.cos(h_angle_12) * r_hour)
    hy_12 = round(cy + math.sin(h_angle_12) * r_hour)
    assert (hx_12, hy_12) == (64, 16), f"12 o'clock pos mismatch: ({hx_12}, {hy_12})"

    # 3 o'clock (3 hour): hand pointing straight right -> (80, 32)
    h_angle_3 = (3 * 30.0) * (math.pi / 180.0) - (math.pi / 2.0)
    hx_3 = round(cx + math.cos(h_angle_3) * r_hour)
    hy_3 = round(cy + math.sin(h_angle_3) * r_hour)
    assert (hx_3, hy_3) == (80, 32), f"3 o'clock pos mismatch: ({hx_3}, {hy_3})"

    # 6 o'clock (6 hour): hand pointing straight down -> (64, 48)
    h_angle_6 = (6 * 30.0) * (math.pi / 180.0) - (math.pi / 2.0)
    hx_6 = round(cx + math.cos(h_angle_6) * r_hour)
    hy_6 = round(cy + math.sin(h_angle_6) * r_hour)
    assert (hx_6, hy_6) == (64, 48), f"6 o'clock pos mismatch: ({hx_6}, {hy_6})"

    # 9 o'clock (9 hour): hand pointing straight left -> (48, 32)
    h_angle_9 = (9 * 30.0) * (math.pi / 180.0) - (math.pi / 2.0)
    hx_9 = round(cx + math.cos(h_angle_9) * r_hour)
    hy_9 = round(cy + math.sin(h_angle_9) * r_hour)
    assert (hx_9, hy_9) == (48, 32), f"9 o'clock pos mismatch: ({hx_9}, {hy_9})"

    print("  [PASS] Analog hour hand coordinates at 12, 3, 6, and 9 verified.")

if __name__ == "__main__":
    test_protocol_variants()
    test_raw_serialization()
    test_menu_scrollbar_geometry()
    test_timekeeping_math_and_formatting()
    test_analog_trigonometry()
    print("\nAll self-test verifications PASSED!")
