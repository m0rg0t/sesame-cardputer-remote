#!/usr/bin/env python3
"""Check the actual-source native renderer's exit status and reference captures."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from sim.render_docs import read_ppm, scale_nearest, write_png


def check_renderer():
    program = ROOT / '.pio/build/native-sim/program'
    manifest = json.loads((ROOT / 'docs/media/screens/manifest.json').read_text())
    environment = dict(os.environ)
    environment.setdefault('SDL_VIDEODRIVER', 'dummy')
    # Repeated fresh processes catch the worker-thread shutdown race. Reference
    # files remain untouched: the captures and generated PNGs are temporary.
    for attempt in range(3):
        with tempfile.TemporaryDirectory(prefix='sesame-render-') as temporary:
            output = Path(temporary)
            subprocess.run([str(program), '--shots', str(output)], cwd=ROOT,
                           env=environment, check=True, timeout=30)
            for screen in manifest['screens']:
                width, height, pixels = read_ppm(output / (screen['id'] + '.ppm'))
                if (width, height) != tuple(manifest['native_size']):
                    raise ValueError('Unexpected renderer dimensions: ' + screen['id'])
                for key, scale in [('native', 1), ('large', manifest['scale'])]:
                    path = output / screen[key]
                    write_png(path, width * scale, height * scale,
                              scale_nearest(width, height, pixels, scale))
                    digest = hashlib.sha256(path.read_bytes()).hexdigest()
                    if digest != screen[key + '_sha256']:
                        raise ValueError('Renderer output changed: ' + screen[key])
        print(f'Render pass {attempt + 1}: clean exit and all 10 capture hashes match')


if __name__ == '__main__':
    check_renderer()
