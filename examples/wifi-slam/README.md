# WiFi Gradient Viewer (ESP32‑S3 Matrix) — Stage 1

- Goal: Show the signal strength of a single WiFi network (Garage Member) as a full‑screen color gradient on the 8×8 matrix.
- Out of scope (Stage 1): CSV/file logging, PC visualization, maps, direction/angle.
- Basis: Single‑antenna RSSI only → relative strength (hot/cold), not distance or bearing.

## Scope

- Target network: `Garage Member` (exact SSID; optionally pin strongest BSSID once discovered).
- Display: Entire 8×8 grid filled with a color mapped from smoothed RSSI.
- Controls: Optional button to “rescan/select BSSID” or toggle brightness.

## How It Works

1. Discovery: Run an initial scan, find all BSSIDs with SSID `Garage Member`. Pick the strongest and note its `channel`.
2. Sampling: Repeatedly scan for that SSID/BSSID. Prefer single‑channel scanning on the noted channel for faster updates; otherwise use full scans.
3. Smoothing: Stabilize RSSI with EMA (alpha ≈ 0.2–0.4) and/or a small median window (3–5 samples).
4. Color Map: Convert smoothed RSSI to a color (e.g., -90 dBm = blue/dim → -30 dBm = red/bright) and fill the matrix.
5. Status: If the network isn’t seen this cycle, show a neutral/status color and retry.

## Step‑By‑Step (Implementation)

1. Init WiFi: `WIFI_STA`, `WiFi.disconnect(true, true)`.
2. Initial scan: enumerate networks; filter `SSID == "Garage Member"`; pick best RSSI; store `bssid` (copy bytes) and `channel`.
3. Set defaults: `RSSI_MIN = -90`, `RSSI_MAX = -30`, `ALPHA = 0.3`.
4. Loop:
   - Scan (single channel if known; else full). Pick best RSSI for the chosen SSID/BSSID.
   - Smooth: `rssi_ema = ALPHA*raw + (1-ALPHA)*prev` (init to first reading).
   - Normalize: `t = clamp((rssi_ema - RSSI_MIN)/(RSSI_MAX - RSSI_MIN), 0..1)`.
   - Color: e.g., `R = 255*t`, `G = 0`, `B = 255*(1-t)` (simple red↔blue ramp).
   - Fill 8×8 with `(R,G,B)`.
5. UX states:
   - Scanning / not found: dim amber.
   - Locked and reading: gradient color.
   - Signal lost (previously found, now missing): brief gray flash, then scanning color.
6. Timing: Start simple with blocking scans and a small `delay(50–100)`. Expect ~0.5–3 s per full scan; faster with single‑channel.

## Example: Color Mapping (snippet)

```cpp
float t = (rssi_ema - RSSI_MIN) / float(RSSI_MAX - RSSI_MIN);
if (t < 0) t = 0; if (t > 1) t = 1;
uint8_t R = uint8_t(255 * t);
uint8_t B = uint8_t(255 * (1 - t));
uint8_t G = 0;
// fillMatrix(R, G, B);
```

## Tuning Tips

- Clamp range to your space: adjust `RSSI_MIN/MAX` after a quick walk test.
- Smoothing: Higher `ALPHA` reacts faster but flickers more; add a 3‑sample median before EMA if needed.
- Brightness limit: Cap global brightness to protect power/battery and reduce noise sensitivity.

## Limitations (Stage 1)

- No angle/bearing: Single antenna cannot measure AoA.
- No reliable distance: RSSI indoors is environment‑dependent; treat as relative only.
- Scan speed: Full scans are slow; single‑channel improves rate once the AP channel is known.

## Next Stage (Deferred)

- CSV logging to SPIFFS and/or Serial for later PC visualization.
- Optional step/yaw logging to build a simple offline gradient trail.
- Basic UI to switch BSSID if multiple APs share the SSID.

## Implementation Notes

- After discovery, use ESP‑IDF scan with fixed `channel` for quicker updates.
- Copy BSSID bytes immediately; don’t keep the pointer returned by the scan API.
- Keep the loop simple and non‑blocking apart from the scan; LED updates are lightweight.
