"""Check declared owner call chains against compiler stack frames and a reserve."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--budget', type=Path, required=True)
args = parser.parse_args()
budget = json.loads(args.budget.read_text(encoding='utf-8'))
frames = {}
for path in args.build.rglob('*.su'):
    for line in path.read_text(errors='replace').splitlines():
        fields = line.split('\t')
        if len(fields) < 3:
            continue
        source = fields[0].rsplit(':', 3)[0]
        if not Path(source).is_file():
            continue
        name = fields[0].rsplit(':', 1)[-1].split('.')[0]
        frames[name] = max(frames.get(name, 0), int(fields[1]))
for name, chain in budget['chains'].items():
    missing = [fn for fn in chain if fn not in frames]
    if missing:
        raise SystemExit('Missing stack evidence: ' + ', '.join(missing))
    used = sum(frames[fn] for fn in chain)
    total = used + budget['reserve_bytes']
    if total > budget['stack_bytes']:
        raise SystemExit(f'{name}: {used} + reserve exceeds {budget["stack_bytes"]}')
    print(f'PASS {name}: known frames {used} + reserve {budget["reserve_bytes"]} <= {budget["stack_bytes"]}')
