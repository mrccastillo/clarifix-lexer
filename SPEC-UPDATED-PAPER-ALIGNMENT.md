# Updated-paper alignment specification

Source of truth: the updated Group 4 paper originally supplied as `Group 4 - PPL Draft Proposal (1).pdf`, especially pages 18-20 and the embedded page-20 sample. The unchanged PDF remains in Git history and the `paper-alignment-2` GitHub release. Its local documentation copies were removed at the user's request.

## Required behavior

1. Preserve handwritten character automata for lexical recognition. No whole-word string matching or premade lexer is introduced.
2. Store unique identifiers and constants in a manually implemented 211-bucket djb2 hash table with separate chaining. The key is the case-sensitive pair `(original lexeme, token type)`; IDs start at 1 in first-insertion order. Trie terminal identity plus token type resolves equality in a bucket without string comparison. The trie assists identity; it does not replace the hash table's bucket lookup.
3. Keep reserved keywords/noise words separate from user symbols. Symbolic operators and delimiters are also excluded from the symbol table. Boolean accepting states recognize `true`/`false` as constants without preloading symbol entries or placing them in the reserved-word array.
4. Store ID, lexeme, token type, category, converted value, Unicode-scalar lexeme length, first line/column, and occurrence count. Numeric values use an exact normalized base-10 representation rather than silently overflowing a machine integer or float. Strip redundant integer leading zeros and decimal trailing fractional zeros; retain `.0` for an integral decimal. Decode supported escapes in string/character values. Boolean values are `true`/`false`. Original lexemes remain distinct even when their converted values agree.
5. Generate one fixed-width, left-aligned report with source filename and these sections:
   - `[A] TOKEN LIST`: every successfully recognized source lexeme, including reserved words, noise, comments, operators and delimiters; no synthetic layout or EOF rows.
   - `[B] SYMBOL TABLE`: unique identifiers and constants, with ID, lexeme, public token type, value, length, and first seen as `line:column`.
   - `[C] INVALID / UNRECOGNIZED TOKENS`: source fragment, location, error code, and reason for every reported lexical diagnostic. A recognized-but-invalid lexical construct (such as a malformed constant or indentation) also belongs here. Line-recovery skips are not falsely labeled as individually recognized lexemes.
   - `[D] SUMMARY`: occurrence counts per category, invalid count, and total successfully recognized source tokens. A repeated identifier contributes two token occurrences and one unique symbol.
6. Print `(none)` in empty data sections. Preserve full lexemes using escaped control bytes and dynamic column widths. Keep partial-output warnings on lexical errors. Invalid fragments are excluded from successful-token totals and appear in section C.
7. Use the paper's public names: `IDENTIFIER`, `CONST_INT`, `CONST_DECIMAL`, `CONST_CHAR`, `CONST_STRING`, `CONST_BOOLEAN`, `NOISE`, `SINGLE_COMMENT`, `MULTI_COMMENT`, and `STATEMENT_TERMINATOR`. Keep existing internal enums in detailed parser-token exports to avoid unnecessary API churn. Other keyword/operator/delimiter names remain unchanged.

## CLI and ambiguity decisions

- `.clx` is the canonical extension from the updated sample. Continue accepting `.clf` as a compatibility alias for the existing examples; reject unrelated extensions with E100.
- `--report FILE` writes the four-section paper report. `--symbols FILE` writes an attribute-rich, identifiers/constants-only TSV for inspection. `--table FILE` retains a simple two-column token-type/lexeme report for the instructor's original checklist. `--tokens FILE` retains detailed parser tokens. Default stdout stays the detailed token stream for compatibility.
- Preserve four-space indentation by default, consistent with the paper's formal grammar and correct program. Keep explicit `--braces` for its free-field examples. Do not guess a mode from source contents.
- Use straight literal quotes and ASCII `--` for decrement, consistent with the explicit quote rule and precedence table. Treat conflicting smart-quote/Unicode-dash examples as typography errors, documented in an alignment addendum rather than changing the user's PDF.
- Keep `to` as NOISE in source reports and normalize it to ASSIGN for parser tokens. Drop allowed filler words from parser tokens. Retain existing E105 lexical validation; the paper inconsistently assigns noise misuse to the parser, so document the early check.
- Optional `int` declarations remain unsupported language syntax: `int` is an ordinary identifier, because it is absent from the reserved vocabulary and formal grammar. Use `set score = 90.` in valid examples.
- Parsing, semantic analysis, runtime execution, missing-colon/period checks, and name/type errors remain outside this phase.

## Acceptance and regression checks

- The exact page-20 source `# avg calc`, `set total to 10.`, `show total.` in `sample.clx` yields nine source-token rows and exactly two symbols: ID 1 `total`, ID 2 `10`, first locations `2:5` and `2:14`. Summary: identifiers 2, keywords 2, noise words 1, constants 1, delimiters 2, comments 1. Section C prints `(none)`.
- Empty input yields empty A/B/C sections and zero summary counts.
- Keywords, operators, delimiters, comments, and noise never occupy symbol rows. Repeated identifiers/constants reuse IDs; case variants and distinct `(lexeme,type)` keys remain distinct.
- Test collision chains with multiple different lexemes hashing to one of the 211 buckets, repeated lookups, and a large table.
- Test all five constant families, leading zeros, exact very large numbers, escaped/Unicode values, scalar lexeme lengths, and first-seen metadata.
- Test invalid fragment capture, recovery, five-error cutoff, invalid UTF-8, and partial reports. No invalid construct is inserted as a successful constant or identifier.
- Preserve all existing lexical-family, location, layout, CLI, and output-protection regressions. Update tests whose old symbol inventory/report expectations are deliberately superseded.
- Test `.clx` and `.clf`, default indentation and explicit braces, and report filename stability across directories.
- Rebuild with strict C11 warnings; regenerate report samples; verify the ZIP by a clean extraction, full tests, examples, manifest checks, and comparison with packaged reports.

## Documentation and publication

The initial `paper-alignment-2` release includes the authoritative paper, alignment addendum, and validation records. Following the user's documentation-cleanup request, the current tree and newly generated packages exclude those documentation files and their PDF renderer. Keep source, this specification, tests, examples, and report samples. Historical release assets remain unchanged. No course hand-in or visibility change is implied.

## Completion

- [x] Hash table, attributes, constants, and reserved-word separation implemented.
- [x] Four report sections, invalid records, summary, and public names implemented.
- [x] Page-20 golden fixture and collision/value/CLI tests pass.
- [x] Paper and alignment documentation packaged; report samples regenerated.
- [x] Clean ZIP validation and GitHub release completed.
