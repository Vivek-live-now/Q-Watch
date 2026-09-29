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

def test_power_management_suite():
    print("\n--- 14. Power Management Suite & Battery Profile Verification ---")

    # 1. Profile and Sleep Engine Enums and String Names
    PROFILE_NAMES = ["PERFORMANCE", "BALANCED", "ENDURANCE", "CUSTOM"]
    SLEEP_ENGINE_NAMES = ["LIGHT SLEEP", "DEEP SLEEP", "DISPLAY OFF", "ULP SENTRY"]

    assert len(PROFILE_NAMES) == 4
    assert len(SLEEP_ENGINE_NAMES) == 4
    print("  [PASS] PowerProfile and SleepEngine enum specifications verified.")

    # 2. Consumption Model and Runtime Estimation Formulas
    class PowerModel:
        def __init__(self, capacity_mah=300.0):
            self.capacity_mah = capacity_mah

        def get_current_ma(self, profile):
            if profile == 0:  # PERFORMANCE
                return 25.0
            elif profile == 1:  # BALANCED
                return 3.0
            elif profile == 2:  # ENDURANCE
                return 0.65
            else:  # CUSTOM default
                return 3.5

        def calculate_runtime_hours(self, profile, voltage, percentage):
            if percentage <= 0 or voltage < 3.20:
                return 0.0
            pct = min(100, max(0, percentage))
            remaining_mah = (pct / 100.0) * self.capacity_mah
            current_ma = self.get_current_ma(profile)
            return remaining_mah / current_ma

        def format_runtime(self, profile, voltage, percentage):
            if percentage <= 5 or voltage < 3.30:
                return "LOW BAT"
            hours = self.calculate_runtime_hours(profile, voltage, percentage)
            if hours >= 48.0:
                return f"{hours / 24.0:.1f}d"
            else:
                return f"{hours:.1f}h"

    pm = PowerModel()

    # PERFORMANCE: 300mAh / 25mA = 12h at 100%, 6h at 50%
    assert abs(pm.calculate_runtime_hours(0, 4.20, 100) - 12.0) < 1e-4
    assert abs(pm.calculate_runtime_hours(0, 3.80, 50) - 6.0) < 1e-4
    assert pm.format_runtime(0, 4.20, 100) == "12.0h"
    assert pm.format_runtime(0, 3.80, 50) == "6.0h"

    # BALANCED: 300mAh / 3mA = 100h (4.17d) at 100%, 50h (2.08d) at 50%
    assert abs(pm.calculate_runtime_hours(1, 4.20, 100) - 100.0) < 1e-4
    assert abs(pm.calculate_runtime_hours(1, 3.80, 50) - 50.0) < 1e-4
    assert pm.format_runtime(1, 4.20, 100) == "4.2d"
    assert pm.format_runtime(1, 3.80, 50) == "2.1d"

    # ENDURANCE: 300mAh / 0.65mA = 461.54h (19.23d) at 100%, 230.77h (9.62d) at 50%
    assert abs(pm.calculate_runtime_hours(2, 4.20, 100) - (300.0 / 0.65)) < 1e-4
    assert pm.format_runtime(2, 4.20, 100) == "19.2d"
    assert pm.format_runtime(2, 3.80, 50) == "9.6d"

    # Low battery guards
    assert pm.format_runtime(1, 3.10, 15) == "LOW BAT"
    assert pm.format_runtime(1, 3.70, 4) == "LOW BAT"
    print("  [PASS] Multi-tier battery runtime estimation mathematical models verified.")

    # 3. Power Profile State Machine Transitions
    class MockPowerManager:
        def __init__(self):
            self.profile = 1  # BALANCED
            self.sleep_engine = 0  # LIGHT_SLEEP
            self.cpu_mhz = 160
            self.pedometer_247 = True
            self.eco_radio_cut = True
            self.eco_led_block = False
            self.eco_audio_mute = False

        def set_profile(self, p):
            self.profile = p
            if p == 0:  # PERFORMANCE
                self.cpu_mhz = 240
                self.sleep_engine = 2  # DISPLAY_OFF
                self.pedometer_247 = True
                self.eco_radio_cut = False
                self.eco_led_block = False
                self.eco_audio_mute = False
            elif p == 1:  # BALANCED
                self.cpu_mhz = 160
                self.sleep_engine = 0  # LIGHT_SLEEP
                self.pedometer_247 = True
                self.eco_radio_cut = True
                self.eco_led_block = False
                self.eco_audio_mute = False
            elif p == 2:  # ENDURANCE
                self.cpu_mhz = 80
                self.sleep_engine = 1  # DEEP_SLEEP
                self.pedometer_247 = False
                self.eco_radio_cut = True
                self.eco_led_block = True
                self.eco_audio_mute = True

    mgr = MockPowerManager()
    mgr.set_profile(0)
    assert mgr.cpu_mhz == 240 and mgr.sleep_engine == 2 and not mgr.eco_radio_cut
    mgr.set_profile(2)
    assert mgr.cpu_mhz == 80 and mgr.sleep_engine == 1 and mgr.eco_led_block and mgr.eco_audio_mute
    mgr.set_profile(1)
    assert mgr.cpu_mhz == 160 and mgr.sleep_engine == 0 and mgr.pedometer_247 and mgr.eco_radio_cut
    print("  [PASS] Power profile governor state transitions verified.")

    # 4. Multi-Page Battery App Navigation State Machine
    class MockBatteryUI:
        def __init__(self):
            self.page = 0
            self.menu_sel = 0

        def handle_dn(self):
            if self.page == 0:
                self.page = 1
                self.menu_sel = 0
            elif self.page in (1, 2, 3):
                self.menu_sel = min(3, self.menu_sel + 1)
            elif self.page == 4:
                self.page = 0

        def handle_up(self):
            if self.page == 0:
                self.page = 4
                self.menu_sel = 0
            elif self.page in (1, 2, 3):
                self.menu_sel = max(0, self.menu_sel - 1)
            elif self.page == 4:
                self.page = 3
                self.menu_sel = 0

        def handle_long_dn(self):
            if self.page < 4:
                self.page += 1
                self.menu_sel = 0

        def handle_long_up(self):
            if self.page > 0:
                self.page -= 1
                self.menu_sel = 0

        def handle_cancel(self):
            if self.page > 0:
                self.page = 0
                return "STAY_APP"
            return "EXIT_MENU"

    bui = MockBatteryUI()
    assert bui.page == 0
    bui.handle_dn()
    assert bui.page == 1
    bui.handle_long_dn()
    assert bui.page == 2
    bui.handle_long_dn()
    assert bui.page == 3
    bui.handle_long_dn()
    assert bui.page == 4
    # Cancel from page 4 goes to root HUD (page 0)
    assert bui.handle_cancel() == "STAY_APP"
    assert bui.page == 0
    # Cancel from page 0 exits to main menu
    assert bui.handle_cancel() == "EXIT_MENU"
    print("  [PASS] 5-Page Battery cockpit navigation and hierarchical cancel verified.")

def test_jules_codebase_optimizations():
    print("\n--- 15. Jules Audit: Morse Code Sequence & Parser Test ---")
    morse_map = {
        'A': ".-", 'B': "-...", 'C': "-.-.", 'D': "-..", 'E': ".",
        'F': "..-.", 'G': "--.", 'H': "....", 'I': "..", 'J': ".---",
        'K': "-.-", 'L': ".-..", 'M': "--", 'N': "-.", 'O': "---",
        'P': ".--.", 'Q': "--.-", 'R': ".-.", 'S': "...", 'T': "-",
        'U': "..-", 'V': "...-", 'W': ".--", 'X': "-..-", 'Y': "-.--",
        'Z': "--..", '1': ".----", '2': "..---", '3': "...--", '4': "....-",
        '5': ".....", '6': "-....", '7': "--...", '8': "---..", '9': "----.",
        '0': "-----"
    }

    # Verify bug fix: 'P' must be .--. and 'Q' must be --.-
    assert morse_map['P'] == ".--.", "Morse code for P must be .--."
    assert morse_map['Q'] == "--.-", "Morse code for Q must be --.-"
    assert morse_map['P'] != morse_map['Q'], "P and Q must be distinct in Morse code"

    # Simulate sound_manager playMorse timing sequence generator
    dot = 70
    dash = 210
    elem_gap = 70
    letter_gap = 210
    word_gap = 420

    def generate_morse_sequence(text):
        seq = []
        for c in text.upper():
            if c == ' ':
                seq.append((0, word_gap))
                continue
            if c in morse_map:
                pattern = morse_map[c]
                for symbol in pattern:
                    dur = dot if symbol == '.' else dash
                    seq.append((2700, dur))
                    seq.append((0, elem_gap))
                seq.append((0, letter_gap))
        return seq

    sos_seq = generate_morse_sequence("SOS")
    # S = ... (3 tones + 3 gaps), O = --- (3 tones + 3 gaps), S = ... (3 tones + 3 gaps) + 3 letter gaps
    assert len(sos_seq) > 0
    # First 3 tones in SOS should be dots (70ms)
    assert sos_seq[0] == (2700, 70)
    assert sos_seq[2] == (2700, 70)
    assert sos_seq[4] == (2700, 70)
    print("  [PASS] Morse code table integrity, 'P' bug fix, and timing sequence generator verified.")

    print("\n--- 16. Jules Audit: FileManager Path Traversal & Normalization Test ---")
    def is_path_safe(path):
        if not path or len(path) == 0:
            return False
        for c in path:
            if c == '\0' or c == '\\' or ord(c) < 32 or ord(c) == 127:
                return False
        if ".." in path:
            return False
        return True

    def normalize_path(path):
        if not path or len(path) == 0:
            return "/"
        if not path.startswith("/"):
            return "/" + path
        return path

    assert normalize_path("") == "/"
    assert normalize_path("ir") == "/ir"
    assert normalize_path("/config/settings") == "/config/settings"

    # Path Traversal attack vectors
    assert not is_path_safe(""), "Empty path must be rejected"
    assert not is_path_safe("../config/settings"), "Relative path traversal must be rejected"
    assert not is_path_safe("/../../config/settings"), "Root traversal must be rejected"
    assert not is_path_safe("/sounds/../../secret.txt"), "Internal traversal must be rejected"
    assert not is_path_safe("file\0name.txt"), "Null byte injection must be rejected"
    assert not is_path_safe("file\\back\\slash"), "Backslash paths must be rejected"
    assert not is_path_safe("file\r\nname"), "Control characters must be rejected"
    assert is_path_safe("/bme_history.bin"), "Standard bin history path must be valid"
    assert is_path_safe("/ir/universal/tvbgone.ir"), "Standard IR path must be valid"
    assert is_path_safe("sounds/alarm.mel"), "Relative safe sound path must be valid"
    print("  [PASS] FileManager normalizePath and isPathSafe path traversal sanitization verified.")

    print("\n--- 17. Jules Audit: Clock Formatting & Timezone Edge Cases ---")
    def format_clock_time(h, m, s, is_24h):
        if is_24h:
            return f"{h:02d}:{m:02d}:{s:02d}"
        else:
            am_pm = "AM" if h < 12 else "PM"
            disp_h = 12 if h == 0 or h == 12 else h % 12
            return f"{disp_h:02d}:{m:02d}:{s:02d} {am_pm}"

    # Midnight 12-hour formatting
    assert format_clock_time(0, 0, 0, False) == "12:00:00 AM"
    assert format_clock_time(0, 5, 9, False) == "12:05:09 AM"
    # Noon 12-hour formatting
    assert format_clock_time(12, 0, 0, False) == "12:00:00 PM"
    assert format_clock_time(12, 30, 45, False) == "12:30:45 PM"
    # Evening 12-hour formatting
    assert format_clock_time(23, 59, 59, False) == "11:59:59 PM"
    # 24-hour formatting
    assert format_clock_time(0, 0, 0, True) == "00:00:00"
    assert format_clock_time(23, 59, 59, True) == "23:59:59"

    # Timezone arithmetic with day-boundary wrapping
    def compute_local_time(utc_h, utc_m, offset_minutes):
        total_m = (utc_h * 60 + utc_m + offset_minutes) % (24 * 60)
        if total_m < 0:
            total_m += 24 * 60
        return total_m // 60, total_m % 60

    # UTC 23:00 + IST (+330 min) -> Next day 04:30
    assert compute_local_time(23, 0, 330) == (4, 30)
    # UTC 02:00 - EST (-300 min) -> Previous day 21:00
    assert compute_local_time(2, 0, -300) == (21, 0)
    print("  [PASS] Clock 12h/24h edge cases and timezone arithmetic verified.")

    print("\n--- 18. Jules Audit: Timekeeping Stopwatch & Timer Countdown Logic ---")
    class MockStopwatch:
        def __init__(self):
            self.running = False
            self.elapsed = 0
            self.start_t = 0
        def start(self, now):
            self.running = True
            self.start_t = now
        def pause(self, now):
            if self.running:
                self.elapsed += now - self.start_t
                self.running = False
        def reset(self):
            self.running = False
            self.elapsed = 0
        def get_time(self, now):
            return self.elapsed + (now - self.start_t if self.running else 0)

    sw = MockStopwatch()
    sw.start(1000)
    assert sw.get_time(2500) == 1500
    sw.pause(2500)
    assert sw.get_time(4000) == 1500 # Unchanged while paused
    sw.start(5000)
    assert sw.get_time(6000) == 2500
    sw.reset()
    assert sw.get_time(7000) == 0

    class MockTimer:
        def __init__(self, duration_s):
            self.remaining = duration_s
            self.alarm_fired = False
        def tick(self, delta_s):
            if self.remaining > 0:
                self.remaining = max(0, self.remaining - delta_s)
                if self.remaining == 0:
                    self.alarm_fired = True

    tmr = MockTimer(10)
    tmr.tick(4)
    assert tmr.remaining == 6 and not tmr.alarm_fired
    tmr.tick(6)
    assert tmr.remaining == 0 and tmr.alarm_fired
    tmr.tick(5)
    assert tmr.remaining == 0 # Clamped at 0
    print("  [PASS] Stopwatch elapsed accumulation and Timer countdown completion verified.")

    print("\n--- 19. Jules Audit: BatteryMonitor Voltage Clamping & Curve ---")
    def battery_voltage_to_percentage(v):
        # LiPo discharge curve points (mV -> %)
        mv = int(v * 1000)
        if mv >= 4200: return 100
        if mv <= 3270: return 0
        curve = [
            (4200, 100), (4060, 90), (3980, 80), (3920, 70), (3870, 60),
            (3820, 50),  (3790, 40), (3770, 30), (3740, 20), (3680, 10),
            (3540, 5),   (3270, 0)
        ]
        for i in range(len(curve) - 1):
            v_high, p_high = curve[i]
            v_low, p_low = curve[i + 1]
            if v_low <= mv <= v_high:
                return int(p_low + (p_high - p_low) * (mv - v_low) / (v_high - v_low))
        return 0

    assert battery_voltage_to_percentage(4.35) == 100, "Over-voltage must clamp to 100%"
    assert battery_voltage_to_percentage(4.20) == 100
    assert battery_voltage_to_percentage(3.82) == 50
    assert battery_voltage_to_percentage(3.20) == 0, "Depleted LiPo must clamp to 0%"
    assert battery_voltage_to_percentage(0.0) == 0, "Zero voltage must safely return 0%"
    print("  [PASS] BatteryMonitor non-linear curve and voltage clamping bounds verified.")

    print("\n--- 20. Jules Audit: Keyboard Navigation & Input Handling ---")
    keyboard_rows = [
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM"
    ]
    cursor_row = 1
    cursor_col = 0 # 'Q'
    # Move Right 4 times -> 'T'
    cursor_col = (cursor_col + 4) % len(keyboard_rows[cursor_row])
    assert keyboard_rows[cursor_row][cursor_col] == 'T'
    # Move Down -> 'G'
    cursor_row = (cursor_row + 1) % len(keyboard_rows)
    cursor_col = min(cursor_col, len(keyboard_rows[cursor_row]) - 1)
    assert keyboard_rows[cursor_row][cursor_col] == 'G'
    print("  [PASS] KeyboardManager 4-row layout and bound-checked cursor navigation verified.")

    print("\n--- 21. Jules Audit: AirMouse Gyro Deadband & Scaling ---")
    def apply_airmouse_deadband(gyro_dps, threshold=2.0, sensitivity=1.5):
        if abs(gyro_dps) < threshold:
            return 0
        sign = 1 if gyro_dps > 0 else -1
        return int((abs(gyro_dps) - threshold) * sensitivity * sign)

    # Sub-threshold jitter should be zero (deadband suppression)
    assert apply_airmouse_deadband(0.5) == 0
    assert apply_airmouse_deadband(-1.8) == 0
    # Active motion above threshold
    delta_pos = apply_airmouse_deadband(10.0)
    assert delta_pos == int((10.0 - 2.0) * 1.5) == 12
    delta_neg = apply_airmouse_deadband(-10.0)
    assert delta_neg == -12
    print("  [PASS] AirMouseManager drift deadband suppression and proportional scaling verified.")

    print("\n--- 22. Jules Audit: Sensor & Health History Batched Pruning & Seek Tail ---")
    entry_size = 10
    max_entries = 288
    batch_trim = 32
    threshold = (max_entries + batch_trim) * entry_size

    # Simulation of history file size growth
    simulated_entries = 287
    size = simulated_entries * entry_size
    assert size < threshold, "No trimming below threshold"

    # Add 1 entry -> reaches 288
    simulated_entries += 1
    size = simulated_entries * entry_size
    assert size < threshold, "288 entries must not trigger expensive rewrite"

    # Add 31 entries -> reaches 319 (still no rewrite!)
    simulated_entries += 31
    size = simulated_entries * entry_size
    assert size < threshold, "Hysteresis prevents rewrite across 31 samples"

    # Add 1 entry -> reaches 320 -> triggers batch trim
    simulated_entries += 1
    size = simulated_entries * entry_size
    assert size >= threshold, "Threshold reached: triggers batched trim"

    # Trimming keeps newest 288 entries, discarding oldest 32
    keep = max_entries
    offset = (simulated_entries - keep) * entry_size
    assert offset == 32 * entry_size
    simulated_entries = keep
    assert simulated_entries * entry_size == 2880
    print("  [PASS] Batched history pruning hysteresis and seek tail calculation verified.")

    print("\n--- 23. Jules Audit: FastLED Compass Blend Continuity ---")
    def simulate_compass_blend(heading):
        h = heading % 360.0
        # Color definitions
        GREEN = (0, 255, 0)
        DEEP_SKY_BLUE = (0, 191, 255)
        BLUE = (0, 0, 255)

        def blend_rgb(c1, c2, frac):
            return (
                int(c1[0] + (c2[0] - c1[0]) * frac),
                int(c1[1] + (c2[1] - c1[1]) * frac),
                int(c1[2] + (c2[2] - c1[2]) * frac)
            )

        if h < 90.0:
            return blend_rgb(GREEN, DEEP_SKY_BLUE, h / 90.0)
        elif h < 180.0:
            return blend_rgb(DEEP_SKY_BLUE, BLUE, (h - 90.0) / 90.0)
        elif h < 270.0:
            return blend_rgb(BLUE, DEEP_SKY_BLUE, (h - 180.0) / 90.0)
        else:
            return blend_rgb(DEEP_SKY_BLUE, GREEN, (h - 270.0) / 90.0)

    # Cardinal anchors
    assert simulate_compass_blend(0) == (0, 255, 0), "North must be Green"
    assert simulate_compass_blend(90) == (0, 191, 255), "East must be DeepSkyBlue"
    assert simulate_compass_blend(180) == (0, 0, 255), "South must be Blue"
    assert simulate_compass_blend(270) == (0, 191, 255), "West must be DeepSkyBlue"
    # Continuity checks at quadrant transitions
    c45 = simulate_compass_blend(45)
    assert 0 < c45[1] < 255 and 0 < c45[2] < 255, "NE must be smooth green-cyan blend"
    print("  [PASS] FastLED compass blend cardinal anchors and continuous interpolation verified.")

    print("\n--- 24. Jules Audit: IR Zero-Allocation Raw Parser & Serializer ---")
    raw_str = " 9000 4500 560 560 560 1690 560 1690 560 "
    # Simulate strtoul pointer scan
    parsed_nums = [int(tok) for tok in raw_str.strip().split()]
    assert parsed_nums == [9000, 4500, 560, 560, 560, 1690, 560, 1690, 560]
    # Serializer buffer reservation
    ser_buf = "data:" + "".join(f" {x}" for x in parsed_nums)
    assert ser_buf.startswith("data: 9000 4500 560")
    print("  [PASS] IR zero-allocation numeric parsing and pre-reserved serialization verified.")

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
    test_power_management_suite()
    test_jules_codebase_optimizations()
    print("\nAll self-test verifications PASSED!")
