# ClariFix lexical analyzer compliance specification

Status: local implementation and package validation complete; course hand-in pending

## Objective

Bring the handwritten C11 lexer, generated output, proposal, tests, and submission package into agreement with the supplied COSC 303 Lexical Analyzer Guidelines & Requirements.

This document records the implementation requirements and completion checklist. Local validation is recorded in `docs/validation.md`; external submission remains pending. The instructor's requirements take precedence over this specification if clarified later.

## Current baseline

- `main.c`, `lexer.c`, and `lexer.h` implement a handwritten lexer for `.clf` files.
- `build.ps1` builds with desktop GCC using C11 and strict warnings; `Makefile` supplies a second build path.
- All four examples run with expected exit statuses: three succeed, and `errors.clf` reports five deliberate lexical errors.
- The most recent native test run had 22 passing test methods and one failing method: the test reader could not decode an invalid UTF-8 byte copied into a diagnostic.
- Recognition currently uses whole-lexeme comparisons in the hash table, direct noise-word comparisons, and string-table matching for operators.
- Current exports are a five-column token listing and a seven-column internal symbol table. Neither matches the instructor's two-column sample.

## Scope and completion conditions

Complete these workstreams in order:

1. Implement character-based finite automata and remove word-level matching from lexical classification and identifier interning.
2. Add the instructor-format Symbol Table export while preserving useful internal data and developer exports.
3. Reconcile the language proposal, README, and examples with the final behavior.
4. Repair diagnostic encoding, run the regression suite, and verify all submission examples.
5. Assemble and inspect a ZIP and a GitHub-ready source tree, then complete the human submission steps.

Program execution, parsing, type checking, and runtime evaluation are outside this lexical-analyzer phase. Missing periods, missing statement colons, and undefined variables remain later-stage checks unless the instructor explicitly requires them here.

## Requirement coverage

| Instructor requirement | Required behavior and evidence |
| --- | --- |
| Unique source file type | Accept lowercase `.clf`; reject other extensions with E100. |
| Identifiers | Recognize `[A-Za-z_][A-Za-z0-9_]*`, case-sensitive, maximum 32 characters; fixed words never become identifiers. |
| Keywords and reserved words | Recognize the proposal's 13 keywords plus Boolean literals, word operators, and noise words with automaton accepting states. Document the sets explicitly. |
| At least four constant types | Demonstrate integer, decimal, character, string, and Boolean literals. Arrays consist of constituent tokens, not a sixth scalar literal token. |
| Noise words | Recognize `to`, `the`, `of`, and `then`; preserve their source spelling for reporting and normalize or omit them for parser-facing tokens as defined below. |
| Comments | Recognize `#` line comments and non-nesting `/* ... */` block comments; retain them in the submission report. |
| Seven arithmetic operators | Demonstrate at least seven; support `+ - * / % ^ ++ --` and the grammar's `//`. |
| Nine Boolean operators | Demonstrate `&&`, `||`, `!`, `==`, `!=`, `<`, `>`, `<=`, `>=`. This interprets the count as three logical plus six relational operators; document word aliases separately. |
| Delimiters and brackets | Recognize parentheses, square brackets, commas, periods, colons, and braces in explicit braces mode. Quotes delimit literals. |
| Invalid tokens | Emit located lexical diagnostics, recover where possible, stop after five errors, and return nonzero. |
| Symbol Table text file | Produce the two-column, source-ordered report specified below. |
| No premade lexer | Handwritten C only; no Flex, Lex, lexer framework, or third-party tokenizer. |
| No word-level string matching | Classify using character transitions and accepting states; do not compare a completed lexeme against candidate words. |
| Finite Automaton behavior | Supply states, transition behavior, accepting-state actions, and a diagram or transition document consistent with the implementation. |
| Compilation instructions | Verify documented Windows build and run commands on desktop GCC. |

The screenshot does not define whether relational operators count as Boolean operators. The nine spellings above are the implementation target; do not invent additional language operators to satisfy a different interpretation without updating the language specification.

## 1. Character-based recognition

### Word automaton

- Use an explicit handwritten deterministic transition structure, such as a trie of character edges, for all fixed word spellings.
- While consuming an identifier-shaped word, advance both the identifier scanner and the fixed-word state one character at a time.
- Classify as a fixed word only if its state is accepting at the end of the entire identifier-shaped word.
- Once a fixed-word transition fails, continue scanning as an identifier. Thus `ifx`, `set_total`, and `true2` remain identifiers; `Set` remains distinct from `set`.
- Accepting states carry token kind and any noise-word subtype. Noise handling must use that subtype, not `strcmp(name, "to")` or equivalent completed-word comparisons.
- Keep the original lexeme span and source location independently of classification.

### Identifier identity and internal symbols

- Replace string-comparison-based interning with a character trie whose terminal nodes carry stable symbol IDs.
- Repeated identifier spellings must reach the same terminal node and reuse the same symbol ID; prefixes and case variants must remain distinct.
- Do not use a hash alone as proof that two names are equal. Distinct names must remain distinct even when their hashes collide.
- Preserve occurrence counts, first locations, and stable one-based IDs where practical.
- If the existing hash table is retained to satisfy the proposal, key its entries by the integer identity assigned by the trie. Compare integer identities when resolving buckets. Update the proposal to explain the trie and the hash table's actual role.
- Alternatively, replace the hash table entirely with the trie and revise the proposal consistently. Choose and document one design before implementing; the instructor checklist does not itself require hashing.

### Other token families

- Replace operator string-table matching with character transitions and accepting states. Use longest match for `+`/`++`/`+=`, `/`/`//`/`/=`, and the remaining overlapping spellings.
- Model integer and decimal scanning with documented states. A decimal point requires a following digit: `19.99.` yields DECIMAL then PERIOD; `20.` yields INT then PERIOD.
- Document literal states for opening quote, content, escape, closing quote, and invalid or unclosed input. Preserve UTF-8 validation and character-literal cardinality checks.
- Document comment states for line comments, block content, potential closing `*`, and closed or unterminated block comments.
- Keep indentation and bracket stacks as auxiliary mechanisms. Do not describe these unbounded stacks as part of a pure finite automaton; the automata recognize lexemes, while separate logic handles layout and delimiter matching.

### Comparison restriction

Remove `strcmp`, `strncmp`, `memcmp`, `strstr`, regular-expression matching, and equivalent completed-word matching from lexical classification and interning. Replacing `strcmp` with a handwritten loop comparing complete candidate words does not satisfy this specification.

Memory copying and string-length operations for storage or output are not classification mechanisms. Comparisons of CLI options and filename extensions in `main.c` are outside source tokenization; document that distinction in the implementation review. If a literal ban on comparison functions across every source file is later confirmed, convert those CLI checks to character transitions as well.

### Deliverable and acceptance

- Add `docs/lexer-automata.md` describing states, character classes, accepting actions, and transitions, with diagrams or tables for each token family.
- All current valid examples retain their parser-facing token sequences unless an explicitly documented requirement changes them.
- Prefix words, case variants, repeated names, large identifier sets, and operator boundaries pass regression tests.
- A source review finds no whole-word matching in the lexical recognition or interning paths.

## 2. Instructor-format Symbol Table

### Report semantics

Use the instructor's term "Symbol Table" for the submission report. Its illustrated rows describe recognized lexemes in source order, which differs from a conventional deduplicated internal symbol table. Preserve that distinction in the README.

- Report every successfully recognized source lexeme in order, including repeated occurrences, literals, noise words, and comments.
- Include both line and block comments. The sample explicitly includes a line comment; including both recognized comment forms is the chosen reporting policy.
- Exclude synthetic EOF, NEWLINE, INDENT, and DEDENT rows because they have no source lexeme.
- Do not list unused preloaded words or other spellings absent from the source.
- Keep parser-facing tokens separate: comments remain absent, `to` becomes ASSIGN in an allowed assignment position, and allowed filler noise words remain omitted.
- An invalid fragment must produce a diagnostic rather than a fabricated valid report row. Successfully recognized fragments before and after recoverable errors may appear in the partial report.

### File format

Write UTF-8 text with exactly two tab-separated columns and this header:

```text
TOKEN TYPE	LEXEME
```

Required public names include IDENTIFIER, INT_LITERAL, DECIMAL_LITERAL, CHAR_LITERAL, STRING_LITERAL, BOOL_LITERAL, NOISE_WORD, LINE_COMMENT, and BLOCK_COMMENT. Existing keyword and operator names may be reused when unambiguous. Publish the complete mapping in the documentation.

Retain original spelling, including quotes around literals and delimiters around comments. Escape embedded tabs, newlines, carriage returns, backslashes, and invalid bytes consistently so each entry occupies one physical row. Document those escapes.

Example input:

```text
# Example
set the age to 20.
show age.
```

Expected report:

```text
TOKEN TYPE	LEXEME
LINE_COMMENT	# Example
SET	set
NOISE_WORD	the
IDENTIFIER	age
NOISE_WORD	to
INT_LITERAL	20
PERIOD	.
SHOW	show
IDENTIFIER	age
PERIOD	.
```

The instructor's `int`, `=`, and `;` example demonstrates report formatting, not mandatory ClariFix syntax. Do not add those language constructs solely to imitate the sample.

### CLI contract

- Add `--report FILE` to generate the instructor-format report.
- Preserve `--tokens FILE` for detailed parser-facing tokens and `--symbols FILE` for the internal symbol inventory, and clearly label their purposes in help and README.
- Provide `build.ps1 -Examples` or an equivalent documented command to build and generate a report for each supplied example, passing `--braces` for `braces.clf`.
- Require an explicit fresh output directory or fresh filenames for generated reports. Keep the existing protection against overwriting source files and previous outputs.
- Preserve exit codes: 0 for lexical success, 1 for lexical errors, 2 for usage, allocation, or I/O failure.
- A report from invalid input must be identified as partial in the diagnostic stream and submission documentation.

### Acceptance

- A golden-file test verifies the exact header, column order, public token names, source order, duplicates, comments, noise words, literals, and exclusion of synthetic tokens.
- Reporting does not change the parser-facing token sequence.
- Tests cover empty input, multiline comments, escaping, report-write failure, existing output protection, and partial results after an invalid token.

## 3. Proposal and documentation reconciliation

- Create a clearly named revised proposal from the existing Markdown draft. Preserve the original PDF as the historical source; do not silently present it as revised.
- Define four-space indentation as the default and `--braces` as an explicit alternate lexical mode. Explain which examples use each mode.
- Use ASCII `--` for decrement consistently, and retain both symbolic and word logical spellings where supported.
- Explain `to` normalization, filler omission, and the difference between parser-facing tokens and the submission report.
- Replace the unreconciled "52 fixed words" claim with an enumerated inventory and an accurately derived count. Explain the chosen interning design.
- Describe five scalar literal types and arrays as compound syntax rather than calling all six primitive scalar types.
- Remove the optional `int score = 90.` declaration example from the revised language definition unless declaration syntax is deliberately implemented and tested. Use `set score = 90.` for the current language.
- Correct malformed smart-quote examples intended to be valid source. Retain smart quotes only in explicitly invalid examples.
- Distinguish lexical checks from parser, semantic, and runtime responsibilities.
- Update README build instructions, CLI help, report examples, automata reference, and actual validation results. Remove the stale claim that no desktop compiler is available.
- Produce a printable revised PDF for submission using an available document conversion tool, and inspect it for missing tables, code, or clipped text. Record an unavailable conversion tool as an outstanding deliverable rather than claiming a PDF exists.

Acceptance: the revised proposal, README, CLI help, examples, token inventory, and implementation agree on these decisions; both the Markdown and printable PDF carry a clear revision label.

## 4. Diagnostics and verification

### Invalid UTF-8 diagnostics

The current E103 diagnostic copies an invalid byte into stderr, causing the Python harness's strict UTF-8 decoding to fail. Fix the diagnostic writer to render invalid source bytes as ASCII escapes such as `\xFF`, while preserving valid UTF-8 and preventing terminal control injection.

Preserve reported source line and byte-column locations. If escapes expand the displayed source excerpt, compute the caret's displayed position so it still points at the offending byte. Do not hide this defect merely by allowing the test harness to discard invalid bytes.

### Required checks

1. Build with `gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2` through the supported Windows script.
2. Run every existing test method successfully, including the deliberately invalid UTF-8 case.
3. Add focused regression coverage for the new automata boundaries, identifier identity, two-column report, diagnostic escaping, and display caret alignment.
4. Run `average.clf`, `features.clf`, and `braces.clf` in their documented modes; require exit 0 and no lexical errors.
5. Run `errors.clf`; require exit 1 and exactly the five intended lexical errors, with partial output clearly identified.
6. Generate and inspect submission reports showing every checklist category. Add a small coverage example if existing examples do not demonstrate a required category.
7. Extract the release ZIP into a fresh directory and follow only its README to build, run examples, generate reports, and execute tests.

Record the compiler version, commands, test summary, and example statuses in `docs/validation.md`. Record current results from an actual run; do not reuse obsolete token counts after behavior changes.

## 5. Submission preparation

### Package contents

- C sources and headers, build scripts, README, and license or attribution files if applicable.
- Example `.clf` files, regression tests, automata documentation, and validation record.
- Generated instructor-format report examples, including a clearly labeled partial invalid-input example.
- Revised proposal Markdown and printable PDF.
- A submission checklist with the repository URL, ZIP filename, and revision identifier once available.

Exclude temporary files, compiler downloads, caches, local logs, machine-specific compilation databases, credentials, and unrelated workspace files. A prebuilt executable is optional and must not replace source or build instructions.

### Repository and hand-in

- Inspect the complete repository file list and ZIP manifest before publishing or uploading anything.
- Prepare the repository locally and verify the ZIP independently. This specification does not itself authorize publication, repository visibility changes, or hand-in messages.
- Use the actual group repository URL when available; do not invent one. Confirm the submitted revision matches the tested ZIP.
- Submit the ZIP and GitHub repository link through the central repository specified by the instructor.
- The screenshot says Thursday 11:59 PM but gives no calendar date or timezone. Confirm the actual due date through the course announcement before recording a deadline.
- Prepare the group index card and print the revised proposal for checking day.

The group must verify course submission access, deadline, physical materials, and successful hand-in. Local implementation is complete only after code, documents, validation, and package checks pass; course submission is complete only after the hand-in steps are actually performed.

## Final acceptance checklist

- [x] Every checklist category has an example and verified report evidence.
- [x] Recognition and interning use character transitions without completed-word comparisons.
- [x] Automata documentation matches the code and distinguishes layout stacks from lexeme automata.
- [x] The submission report has exactly the required two columns and documented row semantics.
- [x] The revised proposal and README agree with implemented behavior.
- [x] The invalid UTF-8 diagnostic is valid UTF-8 and its caret remains accurate.
- [x] Strict GCC build and the complete regression suite pass.
- [x] All four examples have their expected exit statuses.
- [x] A clean extraction of the ZIP builds and runs using its instructions.
- [x] Revised printable proposal, report samples, and validation evidence are packaged.
- [ ] Repository URL, matching revision, actual deadline, and submission destination are verified.
- [ ] ZIP and repository link are handed in; index card and printed proposal are ready.
