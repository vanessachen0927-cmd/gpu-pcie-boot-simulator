#!/usr/bin/env python3
import pathlib
import subprocess
import sys
binary = pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'build/gpu_boot')
for args, expected in [([], 0), (['--link-down'], 2), (['--bad-firmware'], 2)]:
    p = subprocess.run([str(binary), *args], capture_output=True, text=True)
    assert p.returncode == expected, (args, p.stdout, p.stderr)
    print('PASS', args or ['normal'], p.stdout.splitlines()[0])
