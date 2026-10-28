"""Generate reports, parser tokens, and diagnostics into a new directory."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ('sample', 'average', 'features', 'braces', 'errors', 'checklist')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/clarifix.exe')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    destination = args.output.resolve()
    if destination.exists():
        parser.error('Output directory must not already exist.')
    destination.mkdir(parents=True)
    summaries = []
    for name in EXAMPLES:
        report = destination / f'{name}-symbol-table.txt'
        tokens = destination / f'{name}-tokens.txt'
        table = destination / f'{name}-table.tsv'
        symbols = destination / f'{name}-symbols.tsv'
        command = [str(args.executable.resolve()), '--report', str(report), '--tokens', str(tokens),
                   '--table', str(table), '--symbols', str(symbols)]
        if name == 'braces':
            command.append('--braces')
        command.append(str(ROOT / f'examples/{name}.clx'))
        result = subprocess.run(command, capture_output=True, timeout=10)
        (destination / f'{name}-diagnostics.txt').write_bytes(result.stderr)
        expected = 1 if name == 'errors' else 0
        if result.returncode != expected or result.stdout:
            raise RuntimeError(f'{name}: unexpected exit {result.returncode} or stdout: {result.stderr!r}')
        diagnostic = result.stderr.decode('utf-8')
        match = re.search(r'Lexical analysis: (\d+) token\(s\), (\d+) symbol\(s\), (\d+) error\(s\)', diagnostic)
        if not match:
            raise ValueError(f'{name}: missing summary')
        text = report.read_text(encoding='utf-8')
        if not all('[' + letter + ']' in text for letter in 'ABCD'):
            raise ValueError(f'{name}: missing report section')
        with table.open(encoding='utf-8', newline='') as stream:
            rows = list(csv.reader(stream, delimiter='\t', quoting=csv.QUOTE_NONE))
        if rows[0] != ['TOKEN TYPE', 'LEXEME'] or not all(len(row) == 2 for row in rows):
            raise ValueError(f'{name}: invalid report format')
        token_count, symbol_count, errors = map(int, match.groups())
        if errors != (5 if name == 'errors' else 0):
            raise ValueError(f'{name}: unexpected error count')
        summaries.append(dict(source=f'examples/{name}.clx', braces=name == 'braces',
                              exit_status=result.returncode, tokens=token_count,
                              symbols=symbol_count, report_rows=len(rows) - 1, errors=errors))
        print(f'{name}: exit {result.returncode}; {token_count} tokens; {len(rows) - 1} report rows; {errors} errors')
    (destination / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
