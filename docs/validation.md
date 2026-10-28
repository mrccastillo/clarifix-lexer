# Validation record

Revision label: `lexer-compliance-1`.
Validated on 2026-10-28, Asia/Singapore, using Windows x86-64.

## Compiler and build

Desktop compiler: GCC 16.2.0, WinLibs MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3.

```powershell
.\build.ps1 -Test -Examples -OutputDirectory build\final-example-reports
```

The script compiled `main.c` and `lexer.c` with `-std=c11 -Wall -Wextra -Wpedantic -Werror -O2`. Compilation succeeded with no warnings. Python 3.12 ran all 29 black-box test methods successfully. The test harness retains strict UTF-8 decoding; no invalid bytes are silently discarded.

Tests include the existing regression cases plus fixed-word prefixes/case variants and stable identifier identities, exact golden report output, unchanged parser tokens when reporting, comment/literal escaping, empty and partial reports, protected output creation, invalid-byte/control diagnostics, rendered caret positions, and complete checklist coverage.

## Example results

| Source | Mode | Exit | Parser tokens | Report rows | Internal symbols | Errors |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| average.clf | Indentation | 0 | 118 | 96 | 69 | 0 |
| features.clf | Indentation | 0 | 187 | 157 | 69 | 0 |
| braces.clf | --braces | 0 | 34 | 34 | 59 | 0 |
| errors.clf | Indentation | 1 | 13 | 8 | 58 | 5 |
| checklist.clf | Indentation | 0 | 166 | 136 | 67 | 0 |

Reports contain exactly the `TOKEN TYPE` and `LEXEME` columns. Synthetic layout/EOF tokens are excluded. Comments and allowed noise words are retained in source order. Parser tokens remain separate. The internal inventory starts with 57 fixed spellings and adds identifiers.

The `errors.clf` result is intentionally partial. Its five diagnostics are E110, E104, E111, E112, and E103, in that order. Its matching diagnostic sample explicitly identifies the report as partial.

## Recognition and documentation review

- Reviewed trie transition, construction, accepting-action, identifier identity, and operator longest-match paths. No completed-word comparisons classify source or intern names; no scanner generator or third-party lexical framework is used.
- CLI string comparisons remain in `main.c` solely for options and the source extension.
- `docs/lexer-automata.md` documents character states, acceptance, bounded lookahead, and the separate layout/delimiter stacks.
- Revised proposal and README agree on the 57-entry inventory, five scalar literal types, noise normalization, default indentation and alternate braces, ASCII decrement, reports, and phase boundaries.
- Original root-level proposal Markdown/PDF are preserved. The revised proposal is separately labeled.

## Printable proposal

`docs/ClariFix-Revised-Proposal.pdf` was generated from the revised Markdown with `tools/render_proposal.py`, using ReportLab and Markdown in an isolated environment under `build/pdf-tools`. These optional tools are not needed for the C build or regression suite.

The PDF has six A4 pages. All pages were rasterized and inspected. Text bounds lie inside printable margins, tables and code are readable, logical OR displays as two bars, and text extraction found no replacement or missing-glyph markers. No clipped table or code line was found.

## Release verification

A source-only ZIP containing 42 payload files plus `MANIFEST.sha256` passed clean-extraction verification. Every payload hash was checked. The verifier rebuilt with the documented strict PowerShell command in a newly created temporary directory, passed all 29 tests, generated all five example reports, and confirmed byte-for-byte equality with the packaged report samples. No prebuilt executable was used.

```powershell
python tools\package_submission.py --output build\release
python tools\verify_submission.py build\release\clarifix-lexer-compliance-1.zip
```

Final deliverable: `build/release/clarifix-lexer-compliance-1.zip`, with an adjacent `.zip.sha256` checksum. The internal manifest is the authoritative file-level snapshot. GitHub publication target: https://github.com/mrccastillo/clarifix-lexer (private), branch `main`, release tag `lexer-compliance-1`. The published release identifies its exact commit; its ZIP manifest is checked against the committed blobs. Course submission has not been performed.

## Remaining course actions

GitHub publication does not establish a calendar deadline, course hand-in destination, receipt, or physical preparation. Those remain unchecked in `submission/CHECKLIST.md`. The lexer recognizes source; it does not execute ClariFix programs. Linux/macOS instructions are supplied but were not executed in this Windows environment.
