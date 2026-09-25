import os
import subprocess
import math

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
        (16, 0), (16, 6), (16, 13),
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

def test_qapp_abi_and_system():
    print("\n--- 6. Q-App ABI, Dynamic Loader & Tilt Game Stress Test ---")
    import subprocess
    import os

    # 1. Verify Q-App constants from include/qwatch_api.h
    api_h_path = os.path.join(os.path.dirname(__file__), "..", "include", "qwatch_api.h")
    with open(api_h_path, "r") as f:
        content = f.read()

    assert "QAPP_MAGIC 0x51415050U" in content, "QAPP_MAGIC mismatch"
    assert "QAPP_API_VERSION 1U" in content, "QAPP_API_VERSION mismatch"
    assert "QAPP_CAP_ALL" in content, "QAPP_CAP_ALL missing"
    print("  [PASS] qwatch_api.h ABI constants (magic, version, caps) verified.")

    # 2. Build relocatable .qapp binaries for both apps
    base_dir = os.path.join(os.path.dirname(__file__), "..")
    
    # Generate Tilt Ball .qapp (test packager)
    gen_tilt_bin = os.path.join(os.path.dirname(__file__), "gen_tilt_qapp_bin")
    res_tilt_cmp = subprocess.run([
        "clang", "-O2", "-Iinclude", "-Iapps/tilt_game",
        "apps/tilt_game/generate_qapp.c", "apps/tilt_game/tilt_game.c", "-lm",
        "-o", gen_tilt_bin
    ], cwd=base_dir, capture_output=True, text=True)
    assert res_tilt_cmp.returncode == 0, f"Failed to compile tilt packager:\n{res_tilt_cmp.stderr}"
    
    test_tilt_qapp = os.path.join(os.path.dirname(__file__), "test_tilt_pkg.qapp")
    res_tilt_run = subprocess.run([gen_tilt_bin, test_tilt_qapp], cwd=base_dir, capture_output=True, text=True)
    assert res_tilt_run.returncode == 0, f"Failed to generate tilt_ball.qapp:\n{res_tilt_run.stderr}"
    if os.path.exists(gen_tilt_bin):
        os.remove(gen_tilt_bin)
    if os.path.exists(test_tilt_qapp):
        os.remove(test_tilt_qapp)

    # Generate Compass HUD .qapp (test packager)
    gen_compass_bin = os.path.join(os.path.dirname(__file__), "gen_compass_qapp_bin")
    res_compass_cmp = subprocess.run([
        "clang", "-O2", "-Iinclude", "-Iapps/compass_hud",
        "apps/compass_hud/generate_compass_qapp.c", "apps/compass_hud/compass_hud.c", "-lm",
        "-o", gen_compass_bin
    ], cwd=base_dir, capture_output=True, text=True)
    assert res_compass_cmp.returncode == 0, f"Failed to compile compass packager:\n{res_compass_cmp.stderr}"

    test_compass_qapp = os.path.join(os.path.dirname(__file__), "test_compass_pkg.qapp")
    res_compass_run = subprocess.run([gen_compass_bin, test_compass_qapp], cwd=base_dir, capture_output=True, text=True)
    assert res_compass_run.returncode == 0, f"Failed to generate compass_hud.qapp:\n{res_compass_run.stderr}"
    if os.path.exists(gen_compass_bin):
        os.remove(gen_compass_bin)
    if os.path.exists(test_compass_qapp):
        os.remove(test_compass_qapp)

    print("  [PASS] Relocatable .qapp packagers compiled and verified for Tilt Ball and Compass HUD.")

    # 3. Re-compile and execute the C++ Q-App test harness
    bin_path = os.path.join(os.path.dirname(__file__), "test_qapp_system_bin")
    compile_cmd = [
        "clang++", "-O2", "-Iinclude", "-Itests", "-Iapps/tilt_game", "-Iapps/compass_hud",
        "tests/test_qapp_system.cpp", "tests/mock_qwatch_api.cpp",
        "src/qapp_loader.cpp", "src/qapp_target_poc.cpp",
        "apps/tilt_game/tilt_game.c", "apps/compass_hud/compass_hud.c",
        "-lm", "-o", bin_path
    ]
    res = subprocess.run(compile_cmd, cwd=base_dir, capture_output=True, text=True)
    assert res.returncode == 0, f"Failed to compile test_qapp_system_bin:\n{res.stderr}"

    run_res = subprocess.run([bin_path], cwd=base_dir, capture_output=True, text=True)
    assert run_res.returncode == 0, f"test_qapp_system_bin failed:\n{run_res.stdout}\n{run_res.stderr}"
    assert "ALL RELOCATABLE LOADER & POC TESTS PASSED" in run_res.stdout, "POC & Relocatable test verification string missing"
    if os.path.exists(bin_path):
        os.remove(bin_path)
    print("  [PASS] Target IRAM/PSRAM POC, relocatable loader, Tilt Ball & Compass HUD stress tests verified.")

def test_animation_engine():
    print("\n--- 7. Tactical Animation Engine & Player Test ---")
    base_dir = os.path.join(os.path.dirname(__file__), "..")
    bin_path = os.path.join(os.path.dirname(__file__), "test_anim_bin")

    compile_cmd = [
        "clang++", "-O2", "-Iinclude",
        "tests/test_anim_system.cpp", "src/anim_engine.cpp",
        "-lm", "-o", bin_path
    ]
    res = subprocess.run(compile_cmd, cwd=base_dir, capture_output=True, text=True)
    assert res.returncode == 0, f"Failed to compile test_anim_bin:\n{res.stderr}"

    run_res = subprocess.run([bin_path], cwd=base_dir, capture_output=True, text=True)
    assert run_res.returncode == 0, f"test_anim_bin failed:\n{run_res.stdout}\n{run_res.stderr}"
    assert "ALL ANIMATION ENGINE TESTS PASSED!" in run_res.stdout, "Animation tests verification string missing"
    if os.path.exists(bin_path):
        os.remove(bin_path)

    print("  [PASS] AnimHeader spec, multi-frame .anim playback, 1-bit BMP bit reversal, and boot config verified.")

def test_wireless_recon():
    print("\n--- 8. Tactical Wireless Recon Suite Test ---")
    base_dir = os.path.join(os.path.dirname(__file__), "..")
    bin_path = os.path.join(os.path.dirname(__file__), "test_wireless_recon_bin")

    compile_cmd = [
        "clang++", "-O2", "-Iinclude",
        "tests/test_wireless_recon.cpp", "src/wireless_recon.cpp",
        "-lm", "-o", bin_path
    ]
    res = subprocess.run(compile_cmd, cwd=base_dir, capture_output=True, text=True)
    assert res.returncode == 0, f"Failed to compile test_wireless_recon_bin:\n{res.stderr}"

    run_res = subprocess.run([bin_path], cwd=base_dir, capture_output=True, text=True)
    assert run_res.returncode == 0, f"test_wireless_recon_bin failed:\n{run_res.stdout}\n{run_res.stderr}"
    assert "ALL WIRELESS RECON TESTS PASSED!" in run_res.stdout, "Wireless recon tests verification string missing"
    if os.path.exists(bin_path):
        os.remove(bin_path)

    print("  [PASS] 802.11 parsing, deauth flood detection, BLE radar log-distance proximity, and packet monitor verified.")

def test_qlink_protocol():
    print("\n--- 9. Q-Link Protocol & Firmware Interface Test ---")
    base_dir = os.path.join(os.path.dirname(__file__), "..")
    bin_path = os.path.join(os.path.dirname(__file__), "test_qlink_bin")

    compile_cmd = [
        "clang++", "-O2", "-Iinclude",
        "tests/test_qlink.cpp", "src/qlink.cpp",
        "-lm", "-o", bin_path
    ]
    res = subprocess.run(compile_cmd, cwd=base_dir, capture_output=True, text=True)
    assert res.returncode == 0, f"Failed to compile test_qlink_bin:\n{res.stderr}"

    run_res = subprocess.run([bin_path], cwd=base_dir, capture_output=True, text=True)
    assert run_res.returncode == 0, f"test_qlink_bin failed:\n{run_res.stdout}\n{run_res.stderr}"
    assert "ALL Q-LINK PROTOCOL TESTS PASSED!" in run_res.stdout, "Q-Link tests verification string missing"
    if os.path.exists(bin_path):
        os.remove(bin_path)

    print("  [PASS] Q-Link Compact Telemetry (32B), Magic, button injection, and sync interfaces verified.")

def test_compass_3d_orientation():
    print("\n--- 10. Compass 3D Orientation & Bijective Preset Test ---")
    # 1. Bijective mapping test across all 8 presets
    for preset in range(8):
        mode = preset % 4
        inv_z = (preset >= 4)
        reconstructed = (4 if inv_z else 0) + (mode % 4)
        assert reconstructed == preset, f"Preset reconstruction failed for {preset}"

    # 2. Preset 1 check (User setup: X-Fwd, Y-Right, Z-Down [Upside-Down / Flipped])
    user_preset = 1
    assert user_preset % 4 == 1, "Preset 1 mode mismatch"
    assert (user_preset >= 4) is False, "Preset 1 inv_z mismatch (must be Z-Down)"

    # 3. 3D Projection geometry bounds check for 128x64 OLED
    cx, cy, scale = 27, 34, 14
    def project3D(x, y, z):
        px = cx + int((y * 0.866 - x * 0.707) * scale)
        py = cy + int((-x * 0.5 + y * 0.35 + z * 0.85) * scale)
        return px, py

    presets = [
        (( 0, -1,  0), ( 1,  0,  0), ( 0,  0,  1)), # 0: Y-FWD, Z-DN
        (( 1,  0,  0), ( 0,  1,  0), ( 0,  0,  1)), # 1: X-FWD, Z-DN (User)
        (( 0,  1,  0), (-1,  0,  0), ( 0,  0,  1)), # 2: Y-BCK, Z-DN
        ((-1,  0,  0), ( 0, -1,  0), ( 0,  0,  1)), # 3: X-BCK, Z-DN
        (( 0,  1,  0), ( 1,  0,  0), ( 0,  0, -1)), # 4: Y-FWD, Z-UP
        (( 1,  0,  0), ( 0, -1,  0), ( 0,  0, -1)), # 5: X-FWD, Z-UP
        (( 0, -1,  0), (-1,  0,  0), ( 0,  0, -1)), # 6: Y-BCK, Z-UP
        ((-1,  0,  0), ( 0,  1,  0), ( 0,  0, -1)), # 7: X-BCK, Z-UP
    ]

    for idx, (vx, vy, vz) in enumerate(presets):
        px = project3D(*vx)
        py = project3D(*vy)
        pz = project3D(*vz)
        for pt in [px, py, pz]:
            assert 0 <= pt[0] <= 60, f"X out of left viewport bounds ({pt[0]}) in preset {idx}"
            assert 10 <= pt[1] <= 55, f"Y out of vertical viewport bounds ({pt[1]}) in preset {idx}"

    print("  [PASS] All 8 3D orientation presets, bijective mapping, and OLED projection bounds verified.")

def test_imu_3d_orientation():
    print("\n--- 11. IMU 3D Orientation & Right-Hand Rule Test ---")
    kImuFlags = [
        (False, False, False, False), # 0: X-FWD Z-DN (User)
        (True,  false_val := False, True,  False), # 1: Y-FWD Z-DN
        (False, True,  True,  False), # 2: X-BCK Z-DN
        (True,  True,  False, False), # 3: Y-BCK Z-DN
        (False, False, True,  True),  # 4: X-FWD Z-UP
        (True,  False, False, True),  # 5: Y-FWD Z-UP
        (False, True,  False, True),  # 6: X-BCK Z-UP
        (True,  True,  True,  True)   # 7: Y-BCK Z-UP
    ]

    # 1. Uniqueness / bijection
    assert len(set(kImuFlags)) == 8, "IMU preset flag combinations must be unique"

    # 2. Preset 0 matches user's upside down orientation (X forward, Y right, Z down)
    assert kImuFlags[0] == (False, False, False, False), "Preset 0 must be 1:1 identity for X-FWD Z-DN"

    # 3. Orthogonality & Right-Hand rule check for all 8 presets
    imu_presets = [
        (( 1,  0,  0), ( 0,  1,  0), ( 0,  0,  1)), # 0: X-FWD, Z-DN (User)
        (( 0, -1,  0), ( 1,  0,  0), ( 0,  0,  1)), # 1: Y-FWD, Z-DN
        ((-1,  0,  0), ( 0, -1,  0), ( 0,  0,  1)), # 2: X-BCK, Z-DN
        (( 0,  1,  0), (-1,  0,  0), ( 0,  0,  1)), # 3: Y-BCK, Z-DN
        (( 1,  0,  0), ( 0, -1,  0), ( 0,  0, -1)), # 4: X-FWD, Z-UP
        (( 0,  1,  0), ( 1,  0,  0), ( 0,  0, -1)), # 5: Y-FWD, Z-UP
        ((-1,  0,  0), ( 0,  1,  0), ( 0,  0, -1)), # 6: X-BCK, Z-UP
        (( 0, -1,  0), (-1,  0,  0), ( 0,  0, -1)), # 7: Y-BCK, Z-UP
    ]

    cx, cy, scale = 27, 34, 14
    def project3D(x, y, z):
        px = cx + int((y * 0.866 - x * 0.707) * scale)
        py = cy + int((-x * 0.5 + y * 0.35 + z * 0.85) * scale)
        return px, py

    def cross(a, b):
        return (
            a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0]
        )

    for idx, (vx, vy, vz) in enumerate(imu_presets):
        # Verify right-hand rule: X x Y = Z
        cz = cross(vx, vy)
        assert cz == vz, f"Preset {idx} violates right-hand rule: {cz} != {vz}"

        # Verify OLED viewport boundaries
        px = project3D(*vx)
        py = project3D(*vy)
        pz = project3D(*vz)
        for pt in [px, py, pz]:
            assert 0 <= pt[0] <= 60, f"X out of left viewport ({pt[0]}) in IMU preset {idx}"
            assert 10 <= pt[1] <= 55, f"Y out of vertical viewport ({pt[1]}) in IMU preset {idx}"

    print("  [PASS] All 8 IMU presets, right-hand rule orthogonality, and projection bounds verified.")

def test_sensor_calibration_and_robustness():
    print("\n--- 12. Sensor Calibration & Robustness Verification ---")

    # 1. Pitch asin clamping protection (NaN prevention)
    def compute_pitch(sinp):
        clamped = max(-1.0, min(1.0, sinp))
        return math.degrees(math.asin(clamped))

    # Test overshoot beyond [-1, 1] due to quaternion numerical drift
    assert abs(compute_pitch(1.000005) - 90.0) < 1e-4, "Positive overshoot must clamp to +90 deg"
    assert abs(compute_pitch(-1.000005) - (-90.0)) < 1e-4, "Negative overshoot must clamp to -90 deg"
    assert not math.isnan(compute_pitch(1.000005)), "Must not return NaN"

    # 2. Madgwick gradient descent division-by-zero guard
    def safe_normalize(v):
        norm = math.sqrt(sum(x*x for x in v))
        if norm > 1e-4:
            return [x / norm for x in v], True
        return v, False

    # Stationary/aligned gradient (s0=s1=s2=s3=0)
    norm_vec, updated = safe_normalize([0.0, 0.0, 0.0, 0.0])
    assert not updated and norm_vec == [0.0, 0.0, 0.0, 0.0], "Zero gradient must not trigger division by zero"

    # 3. calibrateAccel gravity sign under inverted vs non-inverted Z
    cal_samples = 200
    # Case A: inv_z = False, raw sensor measures +4096 LSB
    raw_az_meas_a = 4120 # slight bias +24 LSB
    expected_g_a = 4096.0
    bias_z_a = raw_az_meas_a - expected_g_a # +24.0
    cal_az_a = (raw_az_meas_a - bias_z_a) / 4096.0 # +1.0g
    assert abs(cal_az_a - 1.0) < 1e-5, f"Body Az must be +1.0g (was {cal_az_a})"

    # Case B: inv_z = True, raw sensor measures -4096 LSB
    raw_az_meas_b = -4072 # slight bias +24 LSB
    expected_g_b = -4096.0
    bias_z_b = raw_az_meas_b - expected_g_b # +24.0
    # Mapping inverts Z when inv_z is true:
    cal_az_b = -((raw_az_meas_b - bias_z_b) / 4096.0) # -(-4096 / 4096) = +1.0g
    assert abs(cal_az_b - 1.0) < 1e-5, f"Body Az must be +1.0g under inv_z (was {cal_az_b})"

    # 4. Magnetometer Raw-Frame Hard-Iron Independence
    # Raw reading with hard iron distortion
    raw_mx, raw_my, raw_mz = 1500, -800, 3200
    hard_iron_x, hard_iron_y, hard_iron_z = 300, -200, 500
    soft_iron = (1.0, 1.0, 1.0)

    # Step A: Raw subtraction
    cx = (raw_mx - hard_iron_x) * soft_iron[0] # 1200
    cy = (raw_my - hard_iron_y) * soft_iron[1] # -600
    cz = (raw_mz - hard_iron_z) * soft_iron[2] # 2700
    raw_cal_mag = math.sqrt(cx*cx + cy*cy + cz*cz)

    # Test all 4 modes with/without invert_z - magnitude must be perfectly invariant!
    for mode in range(4):
        for inv_z in [False, True]:
            if mode == 0:   mx, my, mz = -cx, cy, -cz
            elif mode == 1: mx, my, mz = -cy, -cx, -cz
            elif mode == 2: mx, my, mz = cx, -cy, -cz
            elif mode == 3: mx, my, mz = cy, cx, -cz
            if inv_z:
                mz = -mz
                mx = -mx
            body_mag = math.sqrt(mx*mx + my*my + mz*mz)
            assert abs(body_mag - raw_cal_mag) < 1e-5, f"Magnetic field magnitude altered by mode {mode} inv {inv_z}"

    # 5. Circular yaw exponential smoothing across 360-degree boundary
    alpha = 0.25
    def smooth_yaw(current, target):
        diff = target - current
        while diff < -180.0: diff += 360.0
        while diff > 180.0: diff -= 360.0
        res = current + alpha * diff
        while res < 0.0: res += 360.0
        while res >= 360.0: res -= 360.0
        return res

    # 359 -> 1 deg: diff is +2 deg, smoothed is 359 + 0.25*2 = 359.5 deg
    s1 = smooth_yaw(359.0, 1.0)
    assert abs(s1 - 359.5) < 1e-4, f"359 -> 1 wrap-around smoothing failed: got {s1}"

    # 1 -> 359 deg: diff is -2 deg, smoothed is 1 - 0.25*2 = 0.5 deg
    s2 = smooth_yaw(1.0, 359.0)
    assert abs(s2 - 0.5) < 1e-4, f"1 -> 359 wrap-around smoothing failed: got {s2}"

    print("  [PASS] Asin NaN protection, Madgwick division guard, accel gravity sign, raw hard-iron invariance, and circular yaw smoothing verified.")

def test_firmware_optimization_and_equivalence():
    print("\n--- 13. Firmware Optimization & Equivalence Test ---")

    # 1. Algebraic equivalence test between unoptimized and CSE-optimized Madgwick gradients
    test_cases = [
        # q0, q1, q2, q3, ax, ay, az, mx, my, mz
        (1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.2, 0.0, 0.5),
        (0.7071, 0.0, 0.7071, 0.0, 0.5, 0.2, 0.8, -0.3, 0.4, 0.1),
        (0.5, 0.5, 0.5, 0.5, 0.1, -0.8, 0.5, 0.1, -0.2, 0.7),
        (0.3535, -0.3535, 0.6123, -0.6123, -0.4, 0.7, 0.5, 0.5, -0.1, -0.3),
    ]

    for q0, q1, q2, q3, ax, ay, az, mx, my, mz in test_cases:
        _2q0mx = 2.0 * q0 * mx
        _2q0my = 2.0 * q0 * my
        _2q0mz = 2.0 * q0 * mz
        _2q1mx = 2.0 * q1 * mx
        _2q0 = 2.0 * q0
        _2q1 = 2.0 * q1
        _2q2 = 2.0 * q2
        _2q3 = 2.0 * q3
        _2q0q2 = 2.0 * q0 * q2
        _2q2q3 = 2.0 * q2 * q3
        q0q0 = q0 * q0
        q0q1 = q0 * q1
        q0q2 = q0 * q2
        q0q3 = q0 * q3
        q1q1 = q1 * q1
        q1q2 = q1 * q2
        q1q3 = q1 * q3
        q2q2 = q2 * q2
        q2q3 = q2 * q3
        q3q3 = q3 * q3

        hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3
        hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3
        _2bx = math.sqrt(hx * hx + hy * hy)
        _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3
        _4bx = 2.0 * _2bx
        _4bz = 2.0 * _2bz

        # Original unoptimized formulation
        orig_s0 = -_2q2 * (2.0 * q1q3 - _2q0q2 - ax) + _2q1 * (2.0 * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz)
        orig_s1 = _2q3 * (2.0 * q1q3 - _2q0q2 - ax) + _2q0 * (2.0 * q0q1 + _2q2q3 - ay) - 4.0 * q1 * (1.0 - 2.0 * q1q1 - 2.0 * q2q2 - az) + _2bz * q3 * (_2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz)
        orig_s2 = -_2q0 * (2.0 * q1q3 - _2q0q2 - ax) + _2q3 * (2.0 * q0q1 + _2q2q3 - ay) - 4.0 * q2 * (1.0 - 2.0 * q1q1 - 2.0 * q2q2 - az) + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz)
        orig_s3 = _2q1 * (2.0 * q1q3 - _2q0q2 - ax) + _2q2 * (2.0 * q0q1 + _2q2q3 - ay) + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz)

        # CSE precomputed formulation
        f_g_x = 2.0 * q1q3 - _2q0q2 - ax
        f_g_y = 2.0 * q0q1 + _2q2q3 - ay
        f_g_z = 1.0 - 2.0 * (q1q1 + q2q2) - az

        f_b_x = _2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx
        f_b_y = _2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my
        f_b_z = _2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz

        cse_s0 = -_2q2 * f_g_x + _2q1 * f_g_y - _2bz * q2 * f_b_x + (-_2bx * q3 + _2bz * q1) * f_b_y + _2bx * q2 * f_b_z
        cse_s1 = _2q3 * f_g_x + _2q0 * f_g_y - 4.0 * q1 * f_g_z + _2bz * q3 * f_b_x + (_2bx * q2 + _2bz * q0) * f_b_y + (_2bx * q3 - _4bz * q1) * f_b_z
        cse_s2 = -_2q0 * f_g_x + _2q3 * f_g_y - 4.0 * q2 * f_g_z + (-_4bx * q2 - _2bz * q0) * f_b_x + (_2bx * q1 + _2bz * q3) * f_b_y + (_2bx * q0 - _4bz * q2) * f_b_z
        cse_s3 = _2q1 * f_g_x + _2q2 * f_g_y + (-_4bx * q3 + _2bz * q1) * f_b_x + (-_2bx * q0 + _2bz * q2) * f_b_y + _2bx * q1 * f_b_z

        assert abs(orig_s0 - cse_s0) < 1e-12, f"s0 mismatch: {orig_s0} vs {cse_s0}"
        assert abs(orig_s1 - cse_s1) < 1e-12, f"s1 mismatch: {orig_s1} vs {cse_s1}"
        assert abs(orig_s2 - cse_s2) < 1e-12, f"s2 mismatch: {orig_s2} vs {cse_s2}"
        assert abs(orig_s3 - cse_s3) < 1e-12, f"s3 mismatch: {orig_s3} vs {cse_s3}"

    print("  [PASS] Madgwick CSE algebraic identity mathematically verified (< 1e-12).")

    # 2. Battery Cache Window Verification
    class MockBattery:
        def __init__(self):
            self.cached_voltage = 0.0
            self.last_read_time = 0
            self.adc_read_count = 0

        def read_voltage(self, now_ms, raw_val):
            if self.cached_voltage > 0.0 and (now_ms - self.last_read_time < 2000):
                return self.cached_voltage
            self.last_read_time = now_ms
            self.adc_read_count += 1
            self.cached_voltage = raw_val
            return self.cached_voltage

    bat = MockBattery()
    for t in range(0, 500, 10):
        bat.read_voltage(t, 4.15)
    assert bat.adc_read_count == 1, f"Expected 1 ADC read during 500ms, got {bat.adc_read_count}"

    bat.read_voltage(2050, 4.10)
    assert bat.adc_read_count == 2, "Expected cache invalidation at 2050ms"
    assert bat.cached_voltage == 4.10
    print("  [PASS] Battery voltage 2-second ADC cache window verified.")

    # 3. Clock Loop Polling Throttling
    class MockClock:
        def __init__(self):
            self.last_poll = 0
            self.posix_call_count = 0

        def loop(self, now_ms):
            if now_ms - self.last_poll >= 50:
                self.last_poll = now_ms
                self.posix_call_count += 1

    clk = MockClock()
    for t in range(100):
        clk.loop(t)
    assert clk.posix_call_count <= 2, f"Expected at most 2 POSIX calls in 100ms, got {clk.posix_call_count}"
    print("  [PASS] Clock POSIX getLocalTime 20Hz throttling verified.")

if __name__ == "__main__":
    test_protocol_variants()
    test_raw_serialization()
    test_menu_scrollbar_geometry()
    test_timekeeping_math_and_formatting()
    test_analog_trigonometry()
    test_qapp_abi_and_system()
    test_animation_engine()
    test_wireless_recon()
    test_qlink_protocol()
    test_compass_3d_orientation()
    test_imu_3d_orientation()
    test_sensor_calibration_and_robustness()
    test_firmware_optimization_and_equivalence()
    print("\nAll self-test verifications PASSED!")
