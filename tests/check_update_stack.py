"""Check ESP32 machine-code stack frames; host mocks cannot detect task overflow.

Run after the release build. This bounds our callers' frames, not mbedTLS's
complete dynamic call tree; on-device stack-watermark logs remain necessary.
"""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, default=ROOT / '.pio/build/release/firmware.elf')
    parser.add_argument('--source', type=Path, default=ROOT / 'src')
    args = parser.parse_args()
    objdump = Path.home() / '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-objdump.exe'
    disassembly = subprocess.check_output([str(objdump), '-d', '-C', str(args.elf)], text=True)
    frames = {}
    for name, body in re.findall(r'^[0-9a-f]+ <([^\n]+)>:\n((?:(?!\n\n).)*?)\n\n', disassembly, re.M | re.S):
        entry = re.search(r'\bentry\s+a1,\s*(0x[0-9a-f]+|[0-9]+)', body)
        if entry:
            frames[name] = int(entry[1], 0)
    def frame(prefix):
        found = [(name, value) for name, value in frames.items() if name.startswith(prefix)]
        assert len(found) == 1, f'Expected separate function {prefix}: {found}'
        name, size = found[0]
        print(f'{name}: {size} bytes')
        return size
    task = frame('netTask(')
    tick = frame('onlineUpdateTick(')
    metadata = frame('fetchRelease(')
    install = frame('installRelease(')
    opening = frame('_ZL12openDownload')
    callers = task + tick + max(metadata, install) + opening
    assert callers <= 1536, f'Application frames leave too little TLS headroom: {callers}'
    source = (args.source / 'main.cpp').read_text(encoding='utf-8')
    stack = int(re.search(r'NET_TASK_STACK_BYTES\s*=\s*(\d+)', source)[1])
    assert stack >= 16384, f'Network stack too small for HTTPS: {stack}'
    assert re.search(r'xTaskCreatePinnedToCore\(netTask,"net",NET_TASK_STACK_BYTES,', source)
    assert re.search(r'xTaskCreatePinnedToCore\(netTask,"net",NET_TASK_STACK_BYTES,nullptr,tskIDLE_PRIORITY,nullptr,0\)', source), 'TLS worker must share IDLE0 priority to avoid watchdog resets during crypto'
    print(f'PASS: application frames <= {callers} bytes; task stack {stack} bytes; TLS headroom >= {stack-callers} bytes')

if __name__ == '__main__':
    main()
