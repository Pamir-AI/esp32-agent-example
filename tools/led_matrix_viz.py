#!/usr/bin/env python3
"""
Generic LED matrix terminal visualizer for microcontroller projects (e.g., ESP32 + NeoPixel).

Features
- Reads frames from serial, stdin, or a file.
- Accepts CSV hex frames (RRGGBB tokens), with or without a "FRAME:" prefix.
- Configurable matrix size, input order, LED wiring (serpentine/progressive), and rotation.
- Renders ANSI truecolor blocks or ASCII glyphs for portability.
- Lightweight, single-file tool with minimal dependencies (pyserial optional).

Examples
- Serial (auto-detect port):
  python3 tools/led_matrix_viz.py --width 8 --height 8

- Serial with explicit port/baud:
  python3 tools/led_matrix_viz.py -p /dev/ttyACM0 -b 115200 --width 8 --height 8

- From stdin (no pyserial needed):
  some_program | python3 tools/led_matrix_viz.py --stdin --width 8 --height 8

- From file with recorded frames:
  python3 tools/led_matrix_viz.py --file frames.txt --width 8 --height 8

- Demo output (no hardware):
  python3 tools/led_matrix_viz.py --demo --width 8 --height 8

Firmware CSV hex format
- One line per frame; 64 tokens for an 8x8.
- Tokens are 6 hex digits (RRGGBB), comma-separated. "FRAME:" prefix optional.
- Example line:
  FRAME:000000,00FF00,00FF00,000000,000000,FF0000,FF0000,000000,...

Tip: In Arduino (FastLED)
  for (int y=0; y<H; y++) {
    for (int x=0; x<W; x++) {
      CRGB c = leds[XY(x,y)];
      char buf[8];
      sprintf(buf, "%02X%02X%02X,", c.r, c.g, c.b);
      Serial.print(buf);
    }
  }
  Serial.println();

License: MIT-like (keep this file within your project).
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import time
from typing import Iterable, List, Optional, Sequence, Tuple

try:
    import serial  # type: ignore
    from serial.tools import list_ports  # type: ignore
except Exception:  # pyserial optional
    serial = None
    list_ports = None  # type: ignore


RGB = Tuple[int, int, int]


def rgb_from_hex(token: str) -> Optional[RGB]:
    token = token.strip().lstrip("#")
    if not re.fullmatch(r"[0-9A-Fa-f]{6}", token):
        return None
    r = int(token[0:2], 16)
    g = int(token[2:4], 16)
    b = int(token[4:6], 16)
    return (r, g, b)


def parse_csv_hex_line(line: str, expected_pixels: int) -> Optional[List[RGB]]:
    # Remove optional prefix and anything after STATE: if present
    if line.startswith("FRAME:"):
        line = line[len("FRAME:") :]
    if "STATE:" in line:
        line = line.split("STATE:", 1)[0]

    # Split on commas; ignore blanks; keep only valid RRGGBB
    tokens = [t for t in (tok.strip() for tok in line.split(",")) if t]
    pixels: List[RGB] = []
    for tok in tokens:
        rgb = rgb_from_hex(tok)
        if rgb is not None:
            pixels.append(rgb)
    if len(pixels) == expected_pixels:
        return pixels
    return None


def idx_xy_to_linear(x: int, y: int, w: int, serpentine: bool) -> int:
    if serpentine and (y % 2 == 1):
        return y * w + (w - 1 - x)
    return y * w + x


def rotate_grid(rgb: List[RGB], w: int, h: int, deg: int) -> Tuple[List[RGB], int, int]:
    # Rotate by 0/90/180/270 degrees clockwise
    deg = deg % 360
    if deg == 0:
        return rgb, w, h
    # Convert to 2D
    grid = [[rgb[y * w + x] for x in range(w)] for y in range(h)]
    if deg == 90:
        rot = list(zip(*grid[::-1]))
        out = [c for row in rot for c in row]
        return out, h, w
    if deg == 180:
        out = [rgb[(h - 1 - y) * w + (w - 1 - x)] for y in range(h) for x in range(w)]
        return out, w, h
    if deg == 270:
        rot = list(zip(*grid))
        rot = rot[::-1]
        out = [c for row in rot for c in row]
        return out, h, w
    return rgb, w, h


def ansi_bg(r: int, g: int, b: int) -> str:
    return f"\x1b[48;2;{r};{g};{b}m"


def ansi_fg(r: int, g: int, b: int) -> str:
    return f"\x1b[38;2;{r};{g};{b}m"


RESET = "\x1b[0m"


def rgb_to_ascii(r: int, g: int, b: int) -> str:
    # Weighted brightness approximation
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    # 10-level gradient
    chars = " .,:;ox%#@"
    idx = int((lum / 255.0) * (len(chars) - 1))
    return chars[idx]


def render_frame(
    rgb: Sequence[RGB],
    w: int,
    h: int,
    colored: bool,
    show_grid: bool,
    double_wide: bool,
    header: Optional[str] = None,
) -> str:
    lines: List[str] = []
    if header:
        lines.append(header)
    if show_grid:
        lines.append("  " + " ".join(str(i) for i in range(w)))
        lines.append("  " + ("-" * (2 * w - 1)))
    for y in range(h):
        row: List[str] = []
        if show_grid:
            row.append(f"{y}|")
        for x in range(w):
            r, g, b = rgb[y * w + x]
            if colored:
                block = "  " if double_wide else " "
                row.append(f"{ansi_bg(r, g, b)}{block}{RESET}")
            else:
                row.append(rgb_to_ascii(r, g, b))
        if show_grid:
            row.append("|")
        lines.append("".join(row))
    if show_grid:
        lines.append("  " + ("-" * (2 * w - 1)))
    return "\n".join(lines)


def clear_screen() -> None:
    sys.stdout.write("\x1b[H\x1b[2J")
    sys.stdout.flush()


def auto_detect_port() -> Optional[str]:
    if list_ports is None:
        return None
    ports = list(list_ports.comports())
    # Prefer typical USB CDC names
    preferred = [
        p.device
        for p in ports
        if any(s in (p.device or "") for s in ("ACM", "USB", "cu.", "COM"))
    ]
    if preferred:
        return preferred[0]
    if ports:
        return ports[0].device
    return None


def read_lines_from_serial(port: str, baud: int) -> Iterable[str]:
    if serial is None:
        raise RuntimeError("pyserial not installed. Run: pip install pyserial")
    ser = serial.Serial(port, baud, timeout=0.05)
    time.sleep(0.2)  # settle
    buf = ""
    try:
        while True:
            n = ser.in_waiting or 0
            if n:
                data = ser.read(n)
                try:
                    buf += data.decode("utf-8", errors="ignore")
                except Exception:
                    continue
                if "\n" in buf:
                    parts = buf.split("\n")
                    buf = parts[-1]
                    for line in parts[:-1]:
                        yield line.strip()
            else:
                time.sleep(0.01)
    finally:
        ser.close()


def read_lines_from_file(path: str) -> Iterable[str]:
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            yield line.rstrip("\n")


def read_lines_from_stdin() -> Iterable[str]:
    for line in sys.stdin:
        yield line.rstrip("\n")


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Generic LED matrix terminal visualizer")
    src = p.add_argument_group("input source")
    src.add_argument("--stdin", action="store_true", help="Read frames from stdin")
    src.add_argument("--file", type=str, default=None, help="Read frames from a file path")
    src.add_argument("-p", "--port", type=str, default=None, help="Serial port (auto-detect by default)")
    src.add_argument("-b", "--baud", type=int, default=115200, help="Serial baud rate")

    mat = p.add_argument_group("matrix config")
    mat.add_argument("--width", type=int, default=8, help="Matrix width (X)")
    mat.add_argument("--height", type=int, default=8, help="Matrix height (Y)")
    mat.add_argument(
        "--input-order",
        choices=["xy", "led"],
        default="xy",
        help="How incoming tokens are ordered: xy=row-major scan, led=physical strip order",
    )
    mat.add_argument(
        "--wiring",
        choices=["serpentine", "progressive"],
        default="serpentine",
        help="LED wiring pattern (used if --input-order=led)",
    )
    mat.add_argument(
        "--rotate",
        type=int,
        choices=[0, 90, 180, 270],
        default=0,
        help="Rotate visualization clockwise",
    )

    fmt = p.add_argument_group("format & render")
    fmt.add_argument(
        "--format",
        choices=["csv-hex"],
        default="csv-hex",
        help="Frame format (CSV RRGGBB tokens per frame)",
    )
    fmt.add_argument("--ascii", action="store_true", help="Use ASCII instead of ANSI colors")
    fmt.add_argument("--no-grid", action="store_true", help="Hide grid axes")
    fmt.add_argument("--no-double-wide", action="store_true", help="Use 1 char per pixel width")
    fmt.add_argument("--flip-x", action="store_true", help="Mirror image horizontally")
    fmt.add_argument("--flip-y", action="store_true", help="Mirror image vertically")

    misc = p.add_argument_group("misc")
    misc.add_argument("--demo", action="store_true", help="Run a small demo pattern (no input)")
    misc.add_argument("--fps", type=float, default=None, help="Limit render rate (frames per second)")
    misc.add_argument("--stats", action="store_true", help="Display FPS stats header")
    misc.add_argument("--verbose", action="store_true", help="Print non-frame lines / debug info to stderr")
    misc.add_argument("--list-ports", action="store_true", help="List available serial ports and exit")
    return p


def list_available_ports() -> None:
    if list_ports is None:
        print("pyserial not installed. Run: pip install pyserial")
        return
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found")
        return
    for p in ports:
        desc = p.description or ""
        hwid = p.hwid or ""
        print(f"{p.device}\t{desc}\t{hwid}")


def map_input_to_xy(
    tokens: List[RGB], w: int, h: int, input_order: str, wiring: str
) -> List[RGB]:
    if input_order == "xy":
        return tokens
    # input_order == "led": convert physical strip order to xy order
    serp = wiring == "serpentine"
    out: List[RGB] = [(0, 0, 0)] * (w * h)
    # LED index increases left->right for even rows; reverse for odd rows (serpentine)
    for y in range(h):
        for x in range(w):
            src_idx = idx_xy_to_linear(x, y, w, serp)
            dst_idx = y * w + x
            if 0 <= src_idx < len(tokens):
                out[dst_idx] = tokens[src_idx]
    return out


def apply_flips(rgb: List[RGB], w: int, h: int, flip_x: bool, flip_y: bool) -> List[RGB]:
    if not flip_x and not flip_y:
        return rgb
    out: List[RGB] = [(0, 0, 0)] * (w * h)
    for y in range(h):
        for x in range(w):
            nx = (w - 1 - x) if flip_x else x
            ny = (h - 1 - y) if flip_y else y
            out[ny * w + nx] = rgb[y * w + x]
    return out


def parse_meta(line: str) -> dict:
    # Expected: META:W=8,H=8,ORDER=xy|led,WIRING=serpentine|progressive,ROT=0,FLIPX=0,FLIPY=0,COLOR=RGB
    meta = {}
    if not line.startswith("META:"):
        return meta
    body = line.split(":", 1)[1]
    for part in body.split(","):
        if "=" in part:
            k, v = part.split("=", 1)
            meta[k.strip().upper()] = v.strip()
    return meta


def run_demo(args: argparse.Namespace) -> None:
    w, h = args.width, args.height
    t0 = time.time()
    phase = 0.0
    fps_limit = args.fps
    last = 0.0
    try:
        while True:
            now = time.time()
            if fps_limit is not None and now - last < 1.0 / max(fps_limit, 1e-6):
                time.sleep(0.001)
                continue
            last = now
            dt = now - t0
            phase = dt * 2.0
            pixels: List[RGB] = []
            for y in range(h):
                for x in range(w):
                    # simple color swirl
                    v = (x + y + phase) * 12.0
                    r = int((1 + __import__("math").sin(v * 0.12)) * 127)
                    g = int((1 + __import__("math").sin(v * 0.10 + 2.1)) * 127)
                    b = int((1 + __import__("math").sin(v * 0.14 + 4.2)) * 127)
                    pixels.append((r, g, b))
            pixels, rw, rh = rotate_grid(pixels, w, h, args.rotate)
            header = None
            if args.stats:
                header = f"LEDViz demo  {rw}x{rh}"
            clear_screen()
            frame = render_frame(
                pixels,
                rw,
                rh,
                colored=not args.ascii,
                show_grid=not args.no_grid,
                double_wide=not args.no_double_wide,
                header=header,
            )
            print(frame)
    except KeyboardInterrupt:
        return


def main() -> int:
    args = build_arg_parser().parse_args()

    w, h = args.width, args.height
    expected = w * h
    source_desc = ""
    input_order = args.input_order
    wiring = args.wiring
    rotate = args.rotate
    flip_x = args.flip_x
    flip_y = args.flip_y

    if args.list_ports:
        list_available_ports()
        return 0

    if args.demo:
        run_demo(args)
        return 0

    # Determine input source
    line_iter: Iterable[str]
    if args.stdin:
        source_desc = "stdin"
        line_iter = read_lines_from_stdin()
        print(f"LEDViz: reading {source_desc} … waiting for FRAME lines", file=sys.stderr)
    elif args.file:
        source_desc = f"file:{args.file}"
        line_iter = read_lines_from_file(args.file)
        print(f"LEDViz: reading {source_desc} … waiting for FRAME lines", file=sys.stderr)
    else:
        port = args.port or os.environ.get("LEDVIZ_PORT") or auto_detect_port() or ""
        if not port:
            print("Error: No serial port found. Use --port or --stdin/--file.")
            return 2
        source_desc = f"serial:{port}@{args.baud}"
        try:
            line_iter = read_lines_from_serial(port, args.baud)
        except Exception as e:
            print(f"Error opening serial: {e}")
            return 2
        print(f"LEDViz: reading {source_desc} … waiting for FRAME lines", file=sys.stderr)

    # Render loop
    t_last = time.time()
    frames = 0
    fps = 0.0
    fps_limit = args.fps

    try:
        for line in line_iter:
            if not line:
                continue
            # Handle runtime meta to auto-configure
            if line.startswith("META:"):
                m = parse_meta(line)
                if m:
                    ow, oh = w, h
                    try:
                        w = int(m.get("W", w))
                        h = int(m.get("H", h))
                    except Exception:
                        pass
                    expected = w * h
                    if m.get("ORDER") in ("xy", "led"):
                        input_order = m["ORDER"]
                    if m.get("WIRING") in ("serpentine", "progressive"):
                        wiring = m["WIRING"]
                    try:
                        rotate = int(m.get("ROT", rotate))
                    except Exception:
                        pass
                    flip_x = m.get("FLIPX", str(int(flip_x))) in ("1", "true", "True")
                    flip_y = m.get("FLIPY", str(int(flip_y))) in ("1", "true", "True")
                    print(
                        f"LEDViz: META updated config -> {w}x{h}, order={input_order}, wiring={wiring}, rot={rotate}, flipx={flip_x}, flipy={flip_y}",
                        file=sys.stderr,
                    )
                continue

            if args.format == "csv-hex":
                tokens = parse_csv_hex_line(line, expected)
                if tokens is None:
                    if args.verbose:
                        if line.startswith("FRAME:"):
                            # Count valid tokens to hint what's wrong
                            tokens_tmp = [t for t in (tok.strip() for tok in line.split(",")) if re.fullmatch(r"[0-9A-Fa-f]{6}", t or "")]
                            print(
                                f"LEDViz: ignored frame (expected {expected} tokens, got {len(tokens_tmp)})",
                                file=sys.stderr,
                            )
                        else:
                            print(f"LEDViz: non-frame line: {line}", file=sys.stderr)
                    continue
                pixels_xy = map_input_to_xy(tokens, w, h, input_order, wiring)
            else:
                continue

            # Rotate if requested
            pixels_xy, rw, rh = rotate_grid(pixels_xy, w, h, rotate)
            pixels_xy = apply_flips(pixels_xy, rw, rh, flip_x, flip_y)

            # FPS calc
            frames += 1
            now = time.time()
            dt = now - t_last
            if dt >= 0.5:
                fps = frames / dt
                frames = 0
                t_last = now

            # FPS limit (render throttling only)
            if fps_limit is not None:
                # naive throttle
                time.sleep(max(0.0, (1.0 / max(fps_limit, 1e-6)) - 0.0005))

            header = None
            if args.stats:
                header = f"LEDViz {rw}x{rh}  src={source_desc}  fps={fps:.1f}"

            clear_screen()
            out = render_frame(
                pixels_xy,
                rw,
                rh,
                colored=not args.ascii,
                show_grid=not args.no_grid,
                double_wide=not args.no_double_wide,
                header=header,
            )
            print(out)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
