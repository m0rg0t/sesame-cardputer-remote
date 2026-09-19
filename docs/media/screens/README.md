# Firmware screen captures

These images are deterministic captures from `src/main.cpp`, compiled through
the M5GFX SDL backend by `src/sim/main.cpp`. They use the same drawing
functions, fonts, dimensions, and RGB565 colors as the Cardputer firmware;
hardware networking and input are replaced with fixed scenario state.

Regenerate them with:

```sh
python3 sim/render_docs.py
```

The native images are 240×135 pixels. The `@3x` files use integer nearest-
neighbour scaling for the website and README. They are renderer captures, not
photographs of the physical LCD.
