#!/usr/bin/env python3
"""Replaces the contents of the #wasm-engine-core <script type="text/plain">
block in ../pack.html with the freshly built engine.wasm.js. Run via build.sh."""
import re, pathlib

here = pathlib.Path(__file__).parent
pack_path = here / '..' / 'pack.html'
wasm_js = (here / 'engine.wasm.js').read_text()

pack = pack_path.read_text()
pattern = re.compile(
    r'(<script type="text/plain" id="wasm-engine-core">)(.*?)(</script>)',
    re.DOTALL
)
if not pattern.search(pack):
    raise SystemExit('Could not find #wasm-engine-core script block in pack.html')

new_pack = pattern.sub(lambda m: m.group(1) + '\n' + wasm_js + '\n' + m.group(3), pack, count=1)
pack_path.write_text(new_pack)
print(f'Embedded {len(wasm_js)} bytes of engine.wasm.js into pack.html')
