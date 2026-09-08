"""Explicit on-device check-only regression. Never uploads or installs firmware.

Requires a reachable clock with no critical alarm. Each POST asks the clock to
fetch its release metadata, then verifies the version and monotonic uptime.
Only run when the user has authorized testing the connected device.
"""
import argparse
import json
import time
from pathlib import Path
from urllib.request import Request, urlopen

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', required=True)
    p.add_argument('--current', required=True)
    p.add_argument('--latest', required=True)
    p.add_argument('--count', type=int, default=1)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    base = args.base.rstrip('/')
    results = []
    def state():
        with urlopen(base + '/api/state', timeout=4) as response:
            return json.load(response)
    for number in range(args.count):
        before = state()
        assert before['version'] == args.current
        assert not before['alarm'] and not before['update']['busy']
        request = Request(base + '/api/update/check', data=b'', method='POST', headers={'Origin': base})
        started = time.monotonic()
        with urlopen(request, timeout=8) as response:
            assert response.status == 202
        previous_uptime = before['up']
        deadline = started + 50
        while time.monotonic() < deadline:
            time.sleep(1)
            current = state()
            assert current['up'] >= previous_uptime, 'Clock restarted during check'
            previous_uptime = current['up']
            assert current['version'] == args.current
            update = current['update']
            if not update['busy']:
                result = {'number': number + 1, 'seconds': round(time.monotonic()-started, 2),
                          'uptime_before': before['up'], 'uptime_after': current['up'],
                          'update': update}
                results.append(result)
                args.output.write_text(json.dumps(results, indent=2), encoding='utf-8')
                print(json.dumps(result), flush=True)
                assert update['latest'] == args.latest
                assert update['phase'] in ('current', 'available')
                assert update['http_status'] == 200 and update['tls_error'] == 0
                break
        else:
            raise TimeoutError('Update check did not finish')

if __name__ == '__main__':
    main()
