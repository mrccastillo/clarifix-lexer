"""Black-box tests of the compiled C lexer; Python's standard library only.

Usage: python tests/test_lexer.py build/clarifix.exe
"""
import csv
import io
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else Path('build/clarifix.exe').resolve()
ROOT = Path(__file__).resolve().parents[1]


class LexerTests(unittest.TestCase):
    def scan(self, source, *options, suffix='.clf'):
        with tempfile.TemporaryDirectory(prefix='clarifix-') as directory:
            path = Path(directory) / ('input' + suffix)
            path.write_bytes(source.encode('utf-8') if isinstance(source, str) else source)
            process = subprocess.run([str(EXE), *options, str(path)], capture_output=True, timeout=10)
            stdout = process.stdout.decode('utf-8')
            stderr = process.stderr.decode('utf-8')
            rows = list(csv.DictReader(io.StringIO(stdout), delimiter='\t', quoting=csv.QUOTE_NONE))
            return process.returncode, rows, stderr

    def good(self, source, *options):
        status, rows, diagnostic = self.scan(source, *options)
        self.assertEqual(status, 0, diagnostic)
        self.assertNotIn('Error E', diagnostic)
        self.assertEqual(rows[-1]['TOKEN'], 'EOF')
        return rows

    def test_exact_basic_tokens_and_positions(self):
        rows = self.good('set age to 20.\nshow age.')
        self.assertEqual([(r['TOKEN'], r['LEXEME'], int(r['LINE']), int(r['COLUMN'])) for r in rows], [
            ('SET','set',1,1), ('IDENT','age',1,5), ('ASSIGN','to',1,9),
            ('INT','20',1,12), ('PERIOD','.',1,14), ('NEWLINE','',1,15),
            ('SHOW','show',2,1), ('IDENT','age',2,6), ('PERIOD','.',2,9),
            ('NEWLINE','',2,10), ('EOF','',2,10),
        ])
        self.assertEqual(rows[1]['SYMBOL_ID'], rows[7]['SYMBOL_ID'])

    def test_decimals_periods_and_negative_numbers(self):
        rows = self.good('set x = -19.99. show 20. show 0.5.')
        self.assertEqual([(r['TOKEN'],r['LEXEME']) for r in rows if r['TOKEN'] in {'MINUS','INT','DECIMAL','PERIOD'}],
                         [('MINUS','-'),('DECIMAL','19.99'),('PERIOD','.'),('INT','20'),('PERIOD','.'),('DECIMAL','0.5'),('PERIOD','.')])

    def test_case_and_identifier_boundaries(self):
        rows = self.good('set Set = 1. set _x1 = Set. show ' + 'a' * 32 + '.')
        self.assertIn(('IDENT','Set'), [(r['TOKEN'],r['LEXEME']) for r in rows])
        self.assertIn('_x1', [r['LEXEME'] for r in rows])

    def test_all_operators_use_longest_match(self):
        spelling = '+ - * / // % ^ ++ -- && || ! == != < > <= >= = += -= *= /= %= ^='
        rows = self.good(spelling)
        self.assertEqual([r['LEXEME'] for r in rows[:-2]], spelling.split())
        self.assertEqual([r['TOKEN'] for r in rows[:-2]],
                         'PLUS MINUS STAR SLASH FLOOR_DIV MOD POWER INCREMENT DECREMENT AND OR NOT EQ NE LT GT LE GE ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN'.split())

    def test_word_operators_and_boolean_literals(self):
        rows = self.good('show true and not false or x is not y.')
        self.assertEqual([r['TOKEN'] for r in rows],
                         'SHOW BOOL AND NOT BOOL OR IDENT IS NOT IDENT PERIOD NEWLINE EOF'.split())

    def test_noise_words_are_removed_or_normalized(self):
        rows = self.good('set the total to of 10. show the total. ask of x "?".\nif total > 0 then: # comment\n    show total.')
        self.assertFalse(any(r['TOKEN'] == 'NOISE' for r in rows))
        self.assertEqual([(r['TOKEN'],r['LEXEME']) for r in rows if r['LEXEME'] == 'to'], [('ASSIGN','to')])
        self.assertFalse(any(r['LEXEME'] in {'the','of','then'} for r in rows))

    def test_indexed_assignment_noise(self):
        rows = self.good('set a[indices[0]] to 4.')
        self.assertIn(('ASSIGN','to'), [(r['TOKEN'],r['LEXEME']) for r in rows])

    def test_comments_do_not_produce_tokens(self):
        rows = self.good('# @ $ smart quotes “\n/* ignored\nmore ignored */\nshow 1. # trailing')
        self.assertEqual([r['TOKEN'] for r in rows], 'SHOW INT PERIOD NEWLINE EOF'.split())
        self.assertEqual(rows[0]['LINE'], '4')

    def test_nested_layout_and_eof_dedents(self):
        rows = self.good('if true:\n    if false:\n        show 1.')
        self.assertEqual([r['TOKEN'] for r in rows].count('INDENT'), 2)
        self.assertEqual([r['TOKEN'] for r in rows][-4:], ['NEWLINE','DEDENT','DEDENT','EOF'])

    def test_comment_and_blank_lines_do_not_change_indentation(self):
        rows = self.good('if true:\n    show 1.\n # comment\n\t# comment\n\nshow 2.')
        self.assertEqual([r['TOKEN'] for r in rows].count('INDENT'), 1)
        self.assertEqual([r['TOKEN'] for r in rows].count('DEDENT'), 1)

    def test_multiline_grouping_suppresses_layout(self):
        rows = self.good('set a = [\n  1,\n  (2 +\n3)\n].')
        self.assertFalse(any(r['TOKEN'] in {'INDENT','DEDENT'} for r in rows))
        self.assertEqual([r['TOKEN'] for r in rows].count('NEWLINE'), 1)

    def test_braces_mode(self):
        rows = self.good('if (true) {\n\tshow 1.\n} else { show 2. }', '--braces')
        self.assertFalse(any(r['TOKEN'] in {'NEWLINE','INDENT','DEDENT'} for r in rows))
        self.assertEqual([r['TOKEN'] for r in rows].count('LBRACE'), 2)

    def test_literals_and_unicode(self):
        rows = self.good('set c = \'é\'. show "Hello 世界 @ # /* ”". show "\\n\\\"\\\'\\\\".')
        self.assertEqual([r['TOKEN'] for r in rows].count('CHAR'), 1)
        self.assertEqual([r['TOKEN'] for r in rows].count('STRING'), 2)
        self.assertIn('"Hello 世界 @ # /* ”"', [r['LEXEME'] for r in rows])

    def test_bom_and_windows_line_endings(self):
        rows = self.good(b'\xef\xbb\xbfset x = 1.\r\nshow x.\rshow 2.')
        self.assertEqual([r['LINE'] for r in rows if r['TOKEN'] == 'SHOW'], ['2','3'])

    def test_empty_and_comment_only_files(self):
        for source in ['', '\n \n', '# comment', '/* comment */']:
            with self.subTest(source=source):
                self.assertEqual([r['TOKEN'] for r in self.good(source)], ['EOF'])

    def test_diagnostic_codes_and_format(self):
        cases = [
            ('show "unclosed',102), ('show [1.',102), ('show (1].',102), ('show ).',102),
            ('show @.',103), ('show &.',103), ('show |.',103), ('show \\.',103),
            ('show ' + 'a' * 33 + '.',104), ('show to 1.',105), ('set to = 1.',105),
            ('set x to then 1.',105), ('if true then show 1.',105),
            ('/* unclosed',106), ('if true:\n\tshow 1.',107),
            ('if true:\n  show 1.',108), ('if true:\n        show 1.',108),
            ('show “hello”.',109), ('show ‘A’.',109), ('set 1name = 2.',110),
            ('show "bad\\q".',111), ("show ''.",112), ("show 'AB'.",112),
            (b'show "\xff".',103), (b'show \0.',103),
        ]
        for source, code in cases:
            with self.subTest(source=source):
                status, rows, diagnostic = self.scan(source)
                self.assertEqual(status, 1, diagnostic)
                self.assertIn(f'Error E{code}', diagnostic)
                for label in ['at line ', 'column ', '^', 'What happened:', 'Why:', 'How to fix:']:
                    self.assertIn(label, diagnostic)
                self.assertEqual(rows[-1]['TOKEN'], 'EOF')

    def test_unclosed_bracket_points_to_opening(self):
        status, _, diagnostic = self.scan('show (\n1.')
        self.assertEqual(status, 1)
        self.assertIn('line 1, column 6', diagnostic)

    def test_recovery_and_five_error_limit(self):
        status, rows, diagnostic = self.scan('show @.\nshow 42.\n')
        self.assertEqual(status, 1)
        self.assertIn(('INT','42'), [(r['TOKEN'],r['LEXEME']) for r in rows])
        status, _, diagnostic = self.scan('show @.\n' * 8)
        self.assertEqual(status, 1)
        self.assertEqual(len(re.findall(r'Error E\d+', diagnostic)), 5)
        self.assertIn('Fix these first, then run again', diagnostic)

    def test_large_symbol_table_and_repeated_identifier(self):
        rows = self.good(''.join(f'set item{i} = {i}.\n' for i in range(1200)) + 'show item999.')
        occurrences = [r for r in rows if r['LEXEME'] == 'item999']
        self.assertEqual(len(occurrences), 2)
        self.assertEqual(occurrences[0]['SYMBOL_ID'], occurrences[1]['SYMBOL_ID'])

    def test_symbol_table_and_token_files(self):
        with tempfile.TemporaryDirectory(prefix='clarifix-') as directory:
            path = Path(directory)
            source, tokens, symbols = [path / n for n in ['input.clf','tokens.txt','symbols.txt']]
            source.write_text('set x to 1. show x.', encoding='utf-8')
            args = [str(EXE), '--tokens', str(tokens), '--symbols', str(symbols), str(source)]
            p = subprocess.run(args, capture_output=True, timeout=10)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(p.stdout, b'')
            with symbols.open(encoding='utf-8', newline='') as f:
                table = list(csv.DictReader(f, delimiter='\t', quoting=csv.QUOTE_NONE))
            by_name = {r['LEXEME']:r for r in table}
            self.assertEqual(by_name['x']['OCCURRENCES'], '2')
            self.assertEqual(by_name['x']['FIRST_COLUMN'], '5')
            self.assertEqual(by_name['x']['ORIGIN'], 'IDENTIFIER')
            self.assertEqual(by_name['to']['TOKEN'], 'NOISE')
            self.assertEqual(by_name['to']['OCCURRENCES'], '1')
            self.assertEqual(by_name['function']['OCCURRENCES'], '0')
            self.assertIn('\tASSIGN\tto\t', tokens.read_text())
            original = source.read_bytes()
            p = subprocess.run([str(EXE), '--tokens', str(source), str(source)], capture_output=True, timeout=10)
            self.assertEqual(p.returncode, 2)
            self.assertEqual(source.read_bytes(), original)
            p = subprocess.run(args, capture_output=True, timeout=10)
            self.assertEqual(p.returncode, 2)  # Existing outputs are protected.

    def test_cli_and_io_failures(self):
        for args, expected in [([],2), (['--help'],0), (['--unknown'],2), (['--tokens'],2), (['missing.clf'],2)]:
            with self.subTest(args=args):
                p = subprocess.run([str(EXE), *args], capture_output=True, timeout=10)
                self.assertEqual(p.returncode, expected)
        status, _, diagnostic = self.scan('show 1.', suffix='.txt')
        self.assertEqual(status, 1)
        self.assertIn('E100', diagnostic)

    def test_provided_examples(self):
        for name, options, status in [('average',[],0), ('features',[],0), ('braces',['--braces'],0), ('errors',[],1), ('checklist',[],0)]:
            with self.subTest(name=name):
                p = subprocess.run([str(EXE), *options, str(ROOT/'examples'/f'{name}.clf')], capture_output=True, timeout=10)
                self.assertEqual(p.returncode, status, p.stderr)

    def test_fixed_word_boundaries_and_identifier_identity(self):
        fixed = ('set show ask if elif else while for in function return break continue '
                 'true false and or not is to the of then').split()
        variants = [name + suffix for name in fixed for suffix in ('x', '_total', '2')]
        variants += [name.capitalize() for name in fixed]
        rows = self.good(' '.join(variants + variants))
        ids = {}
        for row in rows:
            if not row['LEXEME']:
                continue
            self.assertEqual(row['TOKEN'], 'IDENT')
            old = ids.setdefault(row['LEXEME'], row['SYMBOL_ID'])
            self.assertEqual(old, row['SYMBOL_ID'])
        self.assertEqual(len(set(ids.values())), len(ids))

    def test_report_golden_and_parser_tokens_unchanged(self):
        source = ROOT / 'tests/fixtures/report.clf'
        baseline = subprocess.run([str(EXE), str(source)], capture_output=True, timeout=10)
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'report.txt'
            result = subprocess.run([str(EXE), '--report', str(report), str(source)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, baseline.stdout)
            expected = (ROOT / 'tests/fixtures/report.expected.tsv').read_text(encoding='utf-8')
            self.assertEqual(report.read_text(encoding='utf-8'), expected)

    def test_report_literals_comments_and_escaping(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source, report = directory / 'input.clf', directory / 'report.txt'
            source.write_bytes(b'/* first\nsecond\t\\\xff */\nshow 1. show 2.5. show \'A\'. show "hi\\n". show true.\n# end')
            result = subprocess.run([str(EXE), '--report', str(report), str(source)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            rows = list(csv.reader(io.StringIO(report.read_text(encoding='utf-8')), delimiter='\t', quoting=csv.QUOTE_NONE))
            self.assertTrue(all(len(row) == 2 for row in rows))
            self.assertEqual(rows[1], ['BLOCK_COMMENT', r'/* first\nsecond\t\\\xFF */'])
            for pair in [('INT_LITERAL','1'), ('DECIMAL_LITERAL','2.5'), ('CHAR_LITERAL',"'A'"),
                         ('STRING_LITERAL', r'"hi\\n"'), ('BOOL_LITERAL','true'), ('LINE_COMMENT','# end')]:
                self.assertIn(list(pair), rows)
            self.assertFalse(any(row[0] in {'EOF','NEWLINE','INDENT','DEDENT'} for row in rows))

    def test_report_empty_partial_and_io_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / 'input.clf'
            source.write_text('', encoding='utf-8')
            report = directory / 'empty.txt'
            result = subprocess.run([str(EXE), '--report', str(report), str(source)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(report.read_text(), 'TOKEN TYPE\tLEXEME\n')
            for output in [report, source, directory / 'missing/report.txt']:
                result = subprocess.run([str(EXE), '--report', str(output), str(source)], capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 2)
            self.assertEqual(source.read_bytes(), b'')
            source.write_text('show @.\nshow 42.', encoding='utf-8')
            report = directory / 'partial.txt'
            result = subprocess.run([str(EXE), '--report', str(report), str(source)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b'Report output is partial', result.stderr)
            self.assertIn('INT_LITERAL\t42', report.read_text())
            self.assertNotIn('@', report.read_text())
            self.assertEqual(subprocess.run([str(EXE), '--report'], capture_output=True, timeout=10).returncode, 2)

    def test_invalid_bytes_and_rendered_caret(self):
        for source in [b'show "\xff".', b'show\t@.', 'show "é" @.'.encode('utf-8'), b'show "\x00".']:
            with self.subTest(source=source):
                status, _, diagnostic = self.scan(source)
                self.assertEqual(status, 1)
                lines = diagnostic.splitlines()
                excerpt, marker = lines[1:3]
                target = '@' if b'@' in source else r'\xFF' if b'\xff' in source else r'\x00'
                self.assertEqual(marker.index('^'), excerpt.index(target))
                self.assertNotIn('\t', excerpt)
                self.assertNotIn('\0', excerpt)
        for source in ['if true:\n\tshow 1.', 'if true:\n  show 1.']:
            _, _, diagnostic = self.scan(source)
            self.assertIn('line 2, column 1', diagnostic)
            self.assertEqual(diagnostic.splitlines()[2], '  ^')

    def test_checklist_report_coverage(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'report.txt'
            result = subprocess.run([str(EXE), '--report', str(report), str(ROOT / 'examples/checklist.clf')], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            with report.open(encoding='utf-8', newline='') as stream:
                rows = list(csv.DictReader(stream, delimiter='\t', quoting=csv.QUOTE_NONE))
            kinds = {row['TOKEN TYPE'] for row in rows}
            required = set(('SET SHOW ASK IF ELIF ELSE WHILE FOR IN FUNCTION RETURN BREAK CONTINUE '
                            'INT_LITERAL DECIMAL_LITERAL CHAR_LITERAL STRING_LITERAL BOOL_LITERAL '
                            'NOISE_WORD LINE_COMMENT BLOCK_COMMENT IDENTIFIER '
                            'PLUS MINUS STAR SLASH FLOOR_DIV MOD POWER INCREMENT DECREMENT '
                            'AND OR NOT EQ NE LT GT LE GE ASSIGN PLUS_ASSIGN MINUS_ASSIGN '
                            'STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN '
                            'LPAREN RPAREN LBRACKET RBRACKET COMMA PERIOD COLON').split())
            self.assertTrue(required <= kinds, required - kinds)

    def test_seeded_malformed_inputs_do_not_crash_or_hang(self):
        rng = random.Random(303)
        alphabet = 'set show 0123_abc+*/=().[]{}:\n\t\'"\\#@&|'
        for _ in range(60):
            source = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(200)))
            status, rows, diagnostic = self.scan(source)
            self.assertIn(status, [0,1], diagnostic)
            self.assertEqual(rows[-1]['TOKEN'], 'EOF')


if __name__ == '__main__':
    if not EXE.is_file():
        sys.exit(f'Compile the C lexer first; executable not found: {EXE}')
    unittest.main(verbosity=2)
