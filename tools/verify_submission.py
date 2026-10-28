"""Verify release hashes and rebuild/test/regenerate reports from a fresh extraction."""
import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile
import zipfile


def run(command, directory):
    result = subprocess.run(command, cwd=directory, capture_output=True, timeout=120)
    print(result.stdout.decode('utf-8', errors='replace'), end='')
    print(result.stderr.decode('utf-8', errors='replace'), end='')
    if result.returncode:
        raise RuntimeError(f'Command failed with exit {result.returncode}: {command}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as package:
        names = package.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate archive paths')
        for name in names:
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
                raise ValueError('Unsafe archive path: ' + name)
            if not name.startswith('ClariFix/'):
                raise ValueError('Unexpected archive root: ' + name)
        manifest = package.read('ClariFix/MANIFEST.sha256').decode('utf-8')
        expected = {}
        for line in manifest.splitlines():
            digest, name = line.split('  ', 1)
            if name in expected:
                raise ValueError('Duplicate manifest entry: ' + name)
            expected[name] = digest
        actual_names = {name.removeprefix('ClariFix/') for name in names
                        if name != 'ClariFix/MANIFEST.sha256'}
        if set(expected) != actual_names:
            raise ValueError('Archive contents differ from manifest')
        for name, digest in expected.items():
            if hashlib.sha256(package.read('ClariFix/' + name)).hexdigest() != digest:
                raise ValueError('Hash mismatch: ' + name)
        print(f'Manifest verified: {len(expected)} payload files')
        # Only this newly created temporary directory is extracted and later removed.
        with tempfile.TemporaryDirectory(prefix='clarifix-release-') as temporary:
            target = Path(temporary).resolve()
            for name in names:
                if not (target / name).resolve().is_relative_to(target):
                    raise ValueError('Extraction escaped temporary directory')
            package.extractall(target)
            root = target / 'ClariFix'
            if os.name == 'nt':
                run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                     str(root / 'build.ps1'), '-Test', '-Examples', '-OutputDirectory',
                     'build/fresh-reports'], root)
                executable = root / 'build/clarifix.exe'
            else:
                run(['make', 'test'], root)
                executable = root / 'clarifix'
            for name in ('sample', 'average', 'features', 'braces', 'errors', 'checklist'):
                output = root / f'build/verify-{name}.txt'
                output.parent.mkdir(exist_ok=True)
                command = [str(executable), '--report', str(output)]
                if name == 'braces':
                    command.append('--braces')
                extension = 'clx' if name == 'sample' else 'clf'
                command.append(str(root / f'examples/{name}.{extension}'))
                result = subprocess.run(command, capture_output=True, timeout=10)
                expected_status = 1 if name == 'errors' else 0
                if result.returncode != expected_status:
                    raise ValueError(f'{name}: unexpected exit {result.returncode}')
                if output.read_bytes() != (root / f'submission/reports/{name}-symbol-table.txt').read_bytes():
                    raise ValueError(f'{name}: regenerated report differs from packaged sample')
                print(f'{name}: expected exit {expected_status}; report matches')
    print('PASS: manifest, clean build, complete tests, all examples, and packaged report samples')


if __name__ == '__main__':
    main()
