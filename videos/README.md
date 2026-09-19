# Test footage

Raw camera files stay local and are ignored by Git. The checked-in web assets
live under `docs/media/video/`.

Current source: `raw_cardputer-control-sesame.MOV`

- Original: 1920×1080 HEVC/HDR, displayed as 1080×1920 portrait, 59.94 fps,
  70.52 seconds, 128,790,600 bytes.
- Web copy: 720×1280 H.264, 30 fps, AAC audio, fast-start MP4,
  8,468,232 bytes.

The source SHA-256 is recorded in `docs/media/video/manifest.json`. Regenerate
the web copy with:

```sh
ffmpeg -i videos/raw_cardputer-control-sesame.MOV \
  -map 0:v:0 -map '0:a:0?' \
  -vf "fps=30,scale=720:-2:flags=lanczos" \
  -c:v libx264 -preset slow -crf 28 -profile:v high -level:v 4.0 \
  -pix_fmt yuv420p -c:a aac -b:a 80k -movflags +faststart \
  -map_metadata -1 docs/media/video/cardputer-control-sesame.mp4
```
