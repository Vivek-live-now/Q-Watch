# Q-Link Mochi Animation Market & Gallery Specification

## 1. Vision & Architecture

The **Mochi Animation Market** is an interactive companion experience in the Q-Link Android App that allows users to browse, live-preview, deploy, and manage animations on their 007 Q-Watch with a single tap.

```
+-------------------------------------------------------------+
|                     Q-Link Android App                      |
|                                                             |
|  +---------------------+      +--------------------------+  |
|  | 128x64 OLED Preview | <--- | Local / Asset Animation  |  |
|  |  (Canvas Phosphor)  |      | Catalog (65+ Mochi .anim)|  |
|  +---------------------+      +--------------------------+  |
|             |                              |                |
|             v                              v                |
|     [ Deploy to Watch ]           [ Delete from Watch ]     |
|             |                              |                |
+-------------|------------------------------|----------------+
              |                              |
              | Wi-Fi REST / BLE GATT Sync   |
              v                              v
+-------------------------------------------------------------+
|                     ESP32-S3 Q-Watch                        |
|                                                             |
|  +-------------------+              +--------------------+  |
|  | /mochi/<name>.anim| <----------> | AnimEngine Player  |  |
|  |   in LittleFS     |              | (Mochi Pet / Boot) |  |
|  +-------------------+              +--------------------+  |
+-------------------------------------------------------------+
```

---

## 2. Key Features

### 2.1. Live 128x64 OLED Simulation Preview
- Renders standard 1-bit binary `.anim` frames directly on a virtual high-contrast OLED canvas in real time (20 FPS).
- Phosphor color themes matching the watch:
  - **Tactical Amber** (`#FFB000`)
  - **Cyber Cyan** (`#00E5FF`)
  - **Matrix Green** (`#00FF66`)
  - **Monochrome White** (`#FFFFFF`)
- Play / Pause / Scrubber slider to inspect individual frames.

### 2.2. Watch LittleFS Storage Bar
- Displays a real-time storage gauge queried via Q-Link telemetry:
  - Total LittleFS Size (e.g., `896 KB`)
  - Used Space (e.g., `512 KB`)
  - Free Space (e.g., `384 KB`)
  - Estimated capacity for new animations (`~19 animations remaining`).

### 2.3. 1-Tap Wireless Deploy & Remove
- **Deploy**: Streams the `.anim` binary over Wi-Fi (`POST /api/v1/fs/upload?path=/mochi/<name>.anim`) or BLE chunked GATT transfer directly to LittleFS.
- **Delete**: Sends `DELETE /api/v1/fs/delete?path=/mochi/<name>.anim` to free flash space.
- **Set as Boot Splash**: 1-tap option to copy the chosen animation to `/boot/boot.anim` for custom startup playback.

### 2.4. Curated Animation Catalog
Categorized into high-engagement packs:
1. **Core Reactive (Installed on Watch)**:
   - Petting (`love.anim`), Feeding (`sushi.anim`), Shake (`dizzy.anim`), Idle (`wink`, `smile`, `sparkle`, `giggle`, `dancing`, `hello`, `playful`, `relaxed`).
2. **Expressions & Moods (Market Catalog)**:
   - `adore`, `blinding`, `brave`, `buzzing`, `contempt`, `devil`, `encouragement`, `energetic`, `enraged`, `evil`, `fast`, `fierce`, `furious`, `glowing`, `growing`, `menacing`, `mistake`, `police`, `rain`, `rush`, `scared`, `shrink`, `sick`, `smirk`, `smoke`, `sneeze`, `sobbing`, `splash`, `spraying`, `squint`, `swinging`, `teasing`, `tough`, `weeping`.
3. **Mecha & Sci-Fi**:
   - `gundam.anim`, `intro.anim`, `radar.anim`, `lockon.anim`, `matrix.anim`.
4. **Custom Community GIF Importer**:
   - Pick any GIF from Android device storage.
   - Built-in Android quantization engine converts grayscale/RGB frames to 128x64 1-bit monochome with Floyd-Steinberg dithering.
   - Packs into `.anim` binary and deploys to watch.

---

## 3. Communication Protocol Endpoints

The Animation Market utilizes the established Q-Link REST API:

| Action | Method & Endpoint | Payload / Params |
| :--- | :--- | :--- |
| **Get Installed Anims** | `GET /api/v1/fs/list?dir=/mochi` | Returns JSON array of installed filenames & byte sizes |
| **Get Storage Info** | `GET /api/v1/system/storage` | Returns `{total, used, free}` |
| **Deploy Animation** | `POST /api/v1/fs/upload` | Multipart/binary stream with header `X-Path: /mochi/<name>.anim` |
| **Delete Animation** | `POST /api/v1/fs/delete` | JSON `{ "path": "/mochi/<name>.anim" }` |
| **Set as Boot Anim** | `POST /api/v1/anim/set_boot` | JSON `{ "path": "/mochi/<name>.anim" }` |

---

## 4. Android Implementation Roadmap (Tomorrow)

### Step 1: Animation Binary Reader & Parser (`android/app/.../AnimParser.kt`)
- Parses 16-byte `AnimHeader` (`magic = 0x4D4E4151`, version, frame count, delay ms).
- Decodes 1024-byte sequential 1-bit frames into Android `Bitmap` or Canvas `DrawScope` paths.

### Step 2: AnimMarketViewModel & State (`AnimMarketViewModel.kt`)
- Queries watch LittleFS directory on connect.
- Synchronizes local catalog against remote installed files (displays badges: `[INSTALLED]`, `[AVAILABLE]`, `[DEPLOYING]`).
- Tracks flash memory headroom.

### Step 3: Jetpack Compose Screen (`AnimMarketScreen.kt`)
- Phosphor OLED Canvas preview component at the top.
- Segmented category chips (Core, Moods, Mecha, Custom).
- Grid cards with animated micro-thumbnails.
- Action bar with `Deploy`, `Delete`, and `Set as Boot`.

### Step 4: Asset Bundling (`android/app/src/main/assets/mochi_market/`)
- Place all 38 extra animations from `extras/mochi_market/` into Android app assets so the user has immediate offline access to the full 65-animation library on their phone without needing internet access.
