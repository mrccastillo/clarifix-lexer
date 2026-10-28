"""Create a source-only release ZIP with an explicit SHA-256 manifest.

Run after building. Example reports are generated in a temporary directory.
No publication or upload is performed. Output directory must be new.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ZIP_NAME = 'clarifix-paper-alignment-2.zip'
ROOT_FILES = ['README.md', 'SPEC-UPDATED-PAPER-ALIGNMENT.md', '.gitignore', 'Makefile',
              'build.ps1', 'main.c', 'lexer.c', 'lexer.h', 'submission/CHECKLIST.md']
DIRECTORIES = ['examples', 'tests', 'tools']


def payload():
    files = [ROOT / name for name in ROOT_FILES]
    for name in DIRECTORIES:
        files.extend(path for path in (ROOT / name).rglob('*')
                     if path.is_file() and '__pycache__' not in path.parts
                     and path.suffix not in {'.pyc', '.exe', '.obj', '.o', '.zip'}
                     and not path.is_symlink())
    return sorted(files, key=lambda path: path.relative_to(ROOT).as_posix())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path,
                        default=ROOT / ('build/clarifix.exe' if os.name == 'nt' else 'clarifix'))
    args = parser.parse_args()
    destination = args.output.resolve()
    if destination.exists():
        parser.error('Output directory must not already exist.')
    files = payload()
    for path in files:
        if not path.is_file():
            parser.error(f'Required file missing: {path}')
    if not args.executable.is_file():
        parser.error('Build the analyzer first, or pass --executable PATH.')
    # Read before opening the destination so unexpected changes cannot create a partial manifest.
    contents = [(path.relative_to(ROOT).as_posix(), path.read_bytes()) for path in files]
    # Reports belong in a hand-in ZIP, not in the working source tree.
    with tempfile.TemporaryDirectory(prefix='clarifix-package-') as temporary:
        reports = Path(temporary) / 'reports'
        subprocess.run([sys.executable, str(ROOT / 'tools/generate_samples.py'),
                        '--executable', str(args.executable.resolve()), '--output', str(reports)],
                       cwd=ROOT, check=True, timeout=60)
        contents.extend(('submission/reports/' + path.name, path.read_bytes())
                        for path in reports.iterdir() if path.is_file())
    contents.sort(key=lambda item: item[0])
    manifest = ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name, data in contents)
    destination.mkdir(parents=True)
    archive = destination / ZIP_NAME
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as package:
        for name, data in contents:
            package.writestr('ClariFix/' + name, data)
        package.writestr('ClariFix/MANIFEST.sha256', manifest.encode('utf-8'))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (destination / (ZIP_NAME + '.sha256')).write_text(f'{digest}  {ZIP_NAME}\n', encoding='utf-8')
    print(f'{archive}\n{len(contents)} payload files; SHA-256 {digest}')


if __name__ == '__main__':
    main()
