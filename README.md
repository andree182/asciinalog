# ASCIInalog

**ASCIInalog** is an analog watchface for the Pebble smartwatch platform rendered entirely through multi-scale ASCII typography. Inspired by visual density art and text-mode graphics, the watchface avoids conventional vector hand drawing. Instead, the analog hands and dial structures emerge as **negative space** carved out from dense, multi-scale alphanumeric characters and typographic contouring.

---

## Overview

Traditional watchfaces draw hands as solid lines or polygons overlaid on top of a background. ASCIInalog reverses this paradigm:

- **Negative-Space Hands**: The hour and minute hands are voids where characters are excluded. The eye naturally perceives the empty corridors as analog hands pointing outward from the center arbor.
- **Multi-Scale Typographic Collage**: Characters range from giant hero glyphs down to micro text, creating depth, rhythm, and texture across the dial.
- **Full-Screen Rectangular & Circular Modes**: Dynamically adapts to rectangular displays (filling the screen edge-to-edge with border-reaching hands) and round dials (Chalk).
- **Sub-Grid Contour Anti-Aliasing**: An AAlib-inspired quadrant coverage algorithm selects directional glyphs (`/`, `\`, `|`, `-`, etc.) along the hand corridors to produce crisp, legible outlines.
- **Embedded Complications**: Seamlessly weaves Day/Date (`WED-04`), 3-letter Month (`NOV`), a custom 1-character dynamic battery level gauge, and optional digital hour/minute tip indicators into the matrix.
- **Contrast & Shading Engine**: Supports high-contrast (white complications with grey noise), subtle (grey complications with white noise), full color, and uniform monochrome styles with 50% checkerboard dithering on B&W platforms.
- **Target Platforms**: Supports all major Pebble architectures: Aplite, Basalt, Chalk, Diorite, Emery, Flint, and Gabbro.

---

## System Architecture

The watchface is engineered under strict embedded constraints: deterministic single-minute ticks, zero dynamic heap allocations during rendering, and pure fixed-point integer arithmetic.

```
+--------------------------------------------------------------+
|                     PebbleKit JS (PKJS)                      |
|  - Instant presets (Default, Semi-Grid, Big Hero, Matrix)    |
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
|  | Virtual Sub-Grid Occupancy Buffer (Bitmask)            |  |
|  +--------------------------------------------------------+  |
|  | Multi-Scale Packing & Complication Allocation Engine   |  |
|  +--------------------------------------------------------+  |
|  | AAlib Quadrant Coverage & Contour Edge Matcher         |  |
|  +--------------------------------------------------------+  |
|  | Custom 8x8 Bold Pixel Font & Dither Graphics Pipeline  |  |
|  +--------------------------------------------------------+  |
+--------------------------------------------------------------+
```

### Module Responsibilities
- **`src/pkjs/index.js`**: Serves the mobile configuration webview. Features instant preset buttons, button-style segmented controls, real-time value sync, and collapsible advanced options (`min_char_size`, `imperfection`, `punct_ratio`, `contrast_mode`).
- **`src/c/main.c`**: Implements the watchface lifecycle, timer service (`MINUTE_UNIT`), geometry calculations, spatial allocation engine, and hardware-accelerated direct frame buffer rendering.

---

## Core Ideas & Design Principles

### 1. Negative-Space Hand Corridors
Rather than drawing hand vectors, characters are systematically excluded from entering the hand paths:
- **Exclusion Core**: A central corridor where no characters are allowed, creating the silhouette of the hand. Both the minute and hour corridors share an equal, generous width for clear readability.
- **Ray-to-Rectangle Projection**: On rectangular watches, hands do not stop at an arbitrary circular boundary. Instead, rays project outward to the rectangular screen borders, allowing hands to stretch all the way to the display edges.
- **Hand Tips & Arbor**: A central circular void marks the watch arbor, and the tips of the hands naturally taper or reach toward the perimeter.

### 2. Virtual Occupancy Grid & Multi-Scale Packing
To combine characters of wildly different sizes without slow polygon overlap checks or heap allocations:
- The dial is mapped into a virtual occupancy bitmask.
- Packing proceeds in hierarchical passes from the largest requested size down to the minimum size.
- **Organic Collage Mode**: Disperses varied character sizes non-linearly using coprime stepping across the virtual grid, producing a natural, scattered editorial collage.
- **Semi-Grid Mode**: Aligns character blocks along structured matrix columns and rows, subdividing larger cells recursively.

### 3. Typographic Contour Anti-Aliasing (AAlib-Style)
Diagonal hands cutting across a discrete pixel matrix typically create stair-stepped jagged edges. To smooth and sharpen these boundaries:
- Candidate character cells along the hand perimeter sample their 4 internal quadrants (NW, NE, SW, SE) to evaluate which portions touch the hand void.
- A pattern matcher selects directional contour glyphs conforming to the slope (e.g. `/`, `\`, `_`, `|`, `(`, `)`).
- This produces the illusion of smooth, anti-aliased hand boundaries using purely typographical forms.

### 4. Adaptive Complication Placement
Complications (Day/Date, Month, Battery, and Hand-Tip Digits) adapt dynamically to available space:
- Angles of both hands are evaluated each minute to find the lowest-interference zones on the dial.
- If horizontal space is constrained (such as when both hands are vertical at 6:05 or 12:30), complication words automatically scale down to compact or stacked representations so they never get obscured or cut through by the hands.
- Hand-tip digits (optional digital hour and minute numbers) search in candidate pockets around the tips (ahead, sideways, above, or below) outside the exclusion core so they remain legible without covering the hands.

### 5. Custom 8x8 Bitmap Font & Shading Pipeline
- **Compact Glyph Set**: An embedded 78-glyph bitmap table containing numbers, letters, and density symbols, with a built-in 1-pixel margin for consistent kerning at any integer scale factor.
- **Dynamic Battery Glyph**: A procedurally rendered 8x8 battery gauge featuring an outer casing, terminal nipple, dynamic interior level bars, and a lightning bolt indicator while charging.
- **Hardware-Agnostic Contrast**: Color platforms render crisp white complications with colorful or pastel noise, while B&W screens utilize an optical $2\times 2$ checkerboard halftone to distinguish complications from background noise without frame flicker.

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
