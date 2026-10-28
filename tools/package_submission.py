"""Create a source-only release ZIP with an explicit SHA-256 manifest.

Run after building and generating submission/reports.
No publication or upload is performed. Output directory must be new.
"""
import argparse
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ZIP_NAME = 'clarifix-paper-alignment-2.zip'
ROOT_FILES = ['README.md', 'SPEC-UPDATED-PAPER-ALIGNMENT.md', '.gitignore', 'Makefile',
              'build.ps1', 'main.c', 'lexer.c', 'lexer.h']
DIRECTORIES = ['examples', 'tests', 'tools', 'submission']


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
    args = parser.parse_args()
    destination = args.output.resolve()
    if destination.exists():
        parser.error('Output directory must not already exist.')
    files = payload()
    required = [ROOT / 'submission/CHECKLIST.md']
    required += [ROOT / f'submission/reports/{name}-symbol-table.txt'
                 for name in ('sample', 'average', 'features', 'braces', 'errors', 'checklist')]
    for path in files + required:
        if not path.is_file():
            parser.error(f'Required file missing: {path}')
    # Read before opening the destination so unexpected changes cannot create a partial manifest.
    contents = [(path.relative_to(ROOT).as_posix(), path.read_bytes()) for path in files]
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
