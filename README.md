# ASCIInalog

**ASCIInalog** is an analog watchface for the Pebble smartwatch platform rendered entirely through multi-scale ASCII typography. Inspired by visual density art and text-mode graphics (such as *whattimeisit*), the watchface avoids conventional vector hand drawing. Instead, the analog hands and dial structures emerge as **negative space** carved out from dense, multi-scale alphanumeric characters and typographic contouring.

---

## Overview

Traditional digital-analog watchfaces render hands as solid lines or polygons overlaid on top of a background. ASCIInalog reverses this paradigm:

- **Negative-Space Hands**: The hour and minute hands are voids where characters are excluded. The eye naturally perceives the empty corridors as analog hands pointing outward from the arbor.
- **Multi-Scale Typographic Collage**: Characters range from large hero glyphs (~36–46px) down to micro text (~8–12px), creating depth and texture across the dial.
- **Full-Screen Rectangular & Circular Modes**: Dynamically adapts to both rectangular displays (filling the display edge-to-edge with border-reaching hands) and round dials (Chalk).
- **Sub-Grid Contour Anti-Aliasing**: An AAlib-inspired quadrant coverage algorithm selects directional glyphs (`/`, `\`, `|`, `-`, etc.) along the hand corridors to produce crisp, legible outlines.
- **Embedded Complications**: Seamlessly weaves Day/Date (`SUN-01`), 3-letter Month (`NOV`), a custom 1-character dynamic battery level gauge, and optional digital hour/minute tip indicators into the ASCII matrix.
- **Contrast & Shading Engine**: Supports high-contrast (white complications with grey noise), subtle (grey complications with white noise), and uniform monochrome styles with 50% checkerboard dithering on B&W platforms and native greyscale on color watches.
- **Target Platforms**: Supports all major Pebble architectures: Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro.

---

## System Architecture

The watchface is engineered under strict embedded constraints: deterministic single-minute ticks, zero dynamic heap allocations during rendering, and pure fixed-point integer arithmetic.

```
+--------------------------------------------------------------+
|                     PebbleKit JS (PKJS)                      |
|  - Segmented toggle UI & collapsible advanced settings       |
|  - Round/Rectangular hardware default detection              |
|  - AppMessage JSON serialization                             |
+------------------------------+-------------------------------+
                               | AppMessage IPC
                               v
+--------------------------------------------------------------+
|                    C Watchapp Core Engine                    |
|  [main.c]                                                    |
|  +--------------------------------------------------------+  |
|  | Settings & Persistence (persist_read/write_data)       |  |
|  +--------------------------------------------------------+  |
|  | Geometry & Hand Vectors (trig_lookup, ray-to-rect)     |  |
|  +--------------------------------------------------------+  |
|  | 48x48 Virtual Sub-Grid Occupancy Buffer (Bitmask)      |  |
|  +--------------------------------------------------------+  |
|  | Multi-Scale Packing & Complication Allocation Engine   |  |
|  +--------------------------------------------------------+  |
|  | AAlib Quadrant Coverage & Contour Edge Matcher         |  |
|  +--------------------------------------------------------+  |
|  | Custom 8x8 Bold Pixel Font & Dither Graphics Pipeline  |  |
|  +--------------------------------------------------------+  |
+--------------------------------------------------------------+
```

### 1. Module Responsibilities
- **`src/pkjs/index.js`**: Serves the mobile configuration webview. Features button-style segmented controls, real-time value sync, and collapsible advanced sliders (`min_char_size`, `imperfection`, `punct_ratio`, `contrast_mode`).
- **`src/c/main.c`**: Implements the watchface lifecycle, timer service (`MINUTE_UNIT`), geometry calculations, spatial allocation engine, and hardware-accelerated direct frame buffer rendering.

---

## Core Algorithms & Implementation Details

### 1. Negative-Space Hand Corridor Geometry

The hands are defined by directional rays extending from the dial center $(c_x, c_y)$ calculated via Pebble's integer trigonometry (`sin_lookup`, `cos_lookup` with 65,536-step angles):

$$\theta_{\text{min}} = \frac{\text{min} \times 65536}{60}, \quad \theta_{\text{hour}} = \frac{(\text{hour} \times 60 + \text{min}) \times 65536}{720}$$

#### Ray-to-Rectangle Projection
In Full-Screen Rectangle mode, hands extend directly toward the display boundaries rather than stopping at a circular radius. The distance $D(\theta)$ to the nearest screen edge is computed by ray-box intersection:

$$D(\theta) = \min\left( \left|\frac{w/2}{\sin\theta}\right|, \left|\frac{h/2}{\cos\theta}\right| \right)$$

The minute hand extends to $95\%$ of the border distance, while the hour hand reaches $48\%$ of the border distance, maintaining clear visual hierarchy across all aspect ratios.

#### Hand Distance Metric
For any coordinate $(p_x, p_y)$, the perpendicular squared distance to a hand segment $V = (x_1 - x_0, y_1 - y_0)$ is evaluated using fixed-point dot-product projection:

$$t = \text{clamp}\left(\frac{(p_x - x_0) \cdot V_x + (p_y - y_0) \cdot V_y}{\|V\|^2}, 0, 1\right)$$

$$\text{dist}^2 = (p_x - (x_0 + t V_x))^2 + (p_y - (y_0 + t V_y))^2$$

- **Exclusion Core ($w \le \text{core\_w}$)**: $0\%$ character coverage. Characters are strictly forbidden, carving the clean silhouette of the hand.
- **Flank Boundary ($\text{core\_w} < w \le \text{near\_w}$)**: Partial coverage zone where contour glyphs and smaller alphanumeric characters are placed.

---

### 2. Virtual Sub-Grid & Multi-Scale Packing

To eliminate collision testing between characters of differing scales without heap allocation, the dial area is mapped to a **$48 \times 48$ virtual occupancy sub-grid** ($2,304$ cells):

$$\text{occ}[\lfloor y / 32 \rfloor \times 48 + x] \quad (\text{stored as a bitmask array of } 32\text{-bit integers})$$

Each character size level $L$ corresponds to an integer block footprint (e.g. $L=1 \to 1\times 1$, $L=2 \to 2\times 3$, $L=3 \to 3\times 4$, $L=4 \to 4\times 5$, $L=5 \to 6\times 7$, $L=6 \to 8\times 9$). Checking whether a multi-scale block can fit requires only bitwise tests:

```c
bool subgrid_is_free(const uint32_t *occ, int n, int gx, int gy, int gw, int gh);
void subgrid_occupy(uint32_t *occ, int gx, int gy, int gw, int gh);
```

#### Placement Pipeline (Executed Once Per Minute)
1. **Complications Pre-Allocation**:
   - Computes low-interference zones based on the current minute and hour hand angles.
   - Embeds Day/Date string (`SUN-01`), 3-letter Month (`NOV`), and the single-cell custom battery gauge.
   - If enabled, places digital hour and minute numbers at the hand tips.
   - Marks all occupied complication cells in the sub-grid to protect them from noise overwrite.
2. **Multi-Scale Collage / Semi-Grid Passes**:
   - Loops from `max_char_size` down to `min_char_size`.
   - In Semi-Grid mode, steps row by row with seeded random offsets, testing candidate cells against `big_char_bias`.
   - In Organic mode, uses coprime step generators across the sub-grid to distribute varied character sizes non-linearly.
3. **Quadrant Pixel Sampling & AAlib Contouring**:
   - For every candidate bounding box, 4 quadrant sub-rectangles (NW, NE, SW, SE) are sampled against dial and hand corridors.
   - **Interior Space**: Filled with dense alphanumeric or punctuation glyphs.
   - **Corridor Edge Space**: Quadrant coverage flags determine the direction of the boundary edge. A specialized matcher selects directional contour glyphs (`/`, `\`, `_`, `|`, `[`, `]`, `(`, `)`) that conform to the hand slope.
4. **Flank Gap Sealer**:
   - When `min_char_size >= 2`, tiny $1\times 1$ filler is prohibited. A secondary sealer pass fits $2\times 3$ alphanumeric cells along the hand flanks to eliminate jagged voids while respecting the user's minimum size constraint.

---

### 3. Font Engine & Shading Pipeline

#### 8x8 Custom Bitmap Font
The typography is stored as a compact constant bitmap table of 78 glyphs:
- Digits `0`–`9`, uppercase `A`–`Z`, lowercase `a`–`z`, and punctuation symbols.
- Each glyph is encoded as 8 row-bytes (`uint8_t rows[8]`), with the active glyph occupying a $7\times 7$ area and a 1-pixel right/bottom margin for automatic kerning at any scaling factor.
- Scaled rendering maps glyph pixels to target bounding boxes via integer Bresenham-style spans:
  $$p_{y0} = y_0 + \frac{r \cdot h}{8}, \quad p_{y1} = y_0 + \frac{(r + 1) \cdot h}{8}$$

#### Battery Gauge Glyph
The battery indicator is drawn procedurally into an $8\times 8$ character matrix:
- Outer rectangular border with terminal nipple (`+`).
- 4 interior fill bars dynamically illuminated according to `battery_state_service_peek().charge_percent`.
- When charging, a center lightning bolt bolt pattern replaces the fill bars.

#### Hardware-Agnostic Greyscale & Dithering
To provide visual distinction between complications, hands, and background noise across diverse Pebble hardware:
- **Pebble Color (Basalt, Chalk, Emery)**: Native palette mapping via `GColorLightGray` ($0xAAAAAA$) and `GColorDarkGray` ($0x555555$), or optional pastel palettes.
- **Pebble B&W (Aplite, Diorite)**: An ordered $2 \times 2$ checkerboard dither mask:
  $$\text{pixel\_active} = ((p_x + p_y) \ \& \ 1) == 0$$
  This yields a stable 50% optical halftone without temporal flicker or frame buffer overhead.

---

## Building & Installation

Requirements: [Pebble Tool](https://github.com/pebble-dev/pebble-tool) / Rebble SDK with Python 3 environment.

```bash
# Clean build for all supported platforms
pebble clean && pebble build

# Install on connected emulator (e.g. emery, basalt, chalk, diorite)
pebble install --emulator emery

# Take verification screenshot from emulator
pebble screenshot --emulator emery screenshot.png
```

---

## License

MIT License. Designed and developed for the Pebble smartwatch community.
