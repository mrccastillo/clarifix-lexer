# Updated-paper alignment validation

Revision: `paper-alignment-2`. Date: 2026-10-28, Asia/Singapore.
Environment: Windows x86-64; Python 3.12; WinLibs GCC 16.2.0, MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3.

## Build and regression suite

```powershell
.\build.ps1 -Test -Examples -OutputDirectory build\paper-alignment-reports
```

The strict C11 build uses `-Wall -Wextra -Wpedantic -Werror -O2`. It compiled without warnings and all 38 regression test methods passed. The page-20 golden output fixture was also checked independently after adding the exact formatting expectation. No invalid UTF-8 is silently discarded by the test harness.

Coverage includes existing token/location/layout/error regressions and new checks for exact paper output, two-symbol sample inventory, collision chains, repeated/case-sensitive keys, no preloaded symbols, all constant values, Unicode scalar lengths, very large exact numeric values, invalid fragments, empty sections, recovery, protected report destinations, portable source basenames, `.clx`/`.clf`, and explicit braces mode.

## Six example runs

| Source | Mode | Exit | Parser tokens | Source report rows | Unique symbols | Errors |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| sample.clx | Indentation | 0 | 11 | 9 | 2 | 0 |
| average.clf | Indentation | 0 | 118 | 96 | 18 | 0 |
| features.clf | Indentation | 0 | 187 | 157 | 33 | 0 |
| braces.clf | --braces | 0 | 34 | 34 | 9 | 0 |
| errors.clf | Indentation | 1 | 13 | 8 | 1 | 5 |
| checklist.clf | Indentation | 0 | 166 | 136 | 23 | 0 |

The sample exactly matches the updated page-20 acceptance values: symbol 1 `total`, value `-`, length 5, first seen `2:5`; symbol 2 `10`, value `10`, length 2, first seen `2:14`. Its source categories are identifiers 2, keywords 2, noise words 1, constants 1, delimiters 2, comments 1, operators 0. Section C is empty.

The deliberate-error example records E110, E104, E111, E112, and E103 in section C and explicitly warns that output is partial. It does not insert malformed constants/names as successful symbols. `submission/reports/summary.json` records actual runs; source reports, parser tokens, attribute TSVs, simple tables, and diagnostics are packaged together.

## Implementation review

- The symbol table has 211 djb2 bucket heads and explicit one-based separate chains. Symbol key equality compares exact trie identity plus token type, not just a hash. Collisions retain distinct entries.
- The reserved/operator array has 55 recognition spellings. Two Boolean accepting paths are separate. No vocabulary prepopulates the symbol table, which begins empty for each call.
- Only identifiers and accepted constants enter the table. IDs follow first insertion. All declared attributes are exported, including decoded/canonical values and first location.
- Source classification and hash-chain equality contain no `strcmp`, `strncmp`, `memcmp`, regular-expression matching, or completed-word comparison loops. CLI comparisons are outside source tokenization.
- Numeric values use exact normalized decimal text rather than overflowing a machine integer/float. Unicode length is scalar-based; source columns remain byte-based.
- Contradictory syntax passages are resolved explicitly in `updated-paper-decisions.md`; no original PDF was edited.

## Paper and printable addendum

`docs/ClariFix-Updated-Paper.pdf` is byte-identical to the user's `Group 4 - PPL Draft Proposal (1).pdf` (SHA-256 checked). `docs/ClariFix-Revised-Proposal.md` and its PDF are now the separately labeled alignment addendum, replacing the earlier implementation proposal. Optional PDF-generation tools are outside the workspace under the system temporary directory.

The addendum is four A4 pages. Every page was rasterized and inspected; tables/code are readable, table headers repeat across page breaks, and no clipped text or replacement glyphs were found. PDF text bounds were checked against printable margins. Range punctuation was also verified after rendering.

## Source-only release

ZIP: `clarifix-paper-alignment-2.zip`. Target tag: `paper-alignment-2` in the existing private repository, https://github.com/mrccastillo/clarifix-lexer.

The package includes the authoritative updated paper, addendum, current specification, source, examples, tests, report samples, and this evidence. It excludes local tool installations, temporary logs, binaries, caches, and superseded draft PDFs. The updated paper is retained unchanged in `docs`; redundant root copies and obsolete drafts have been removed from the current tree and remain in Git history.

The published `paper-alignment-2` snapshot passed clean-extraction verification for all 62 payload files: every manifest hash was checked, the documented PowerShell script rebuilt the analyzer and passed all 38 tests, all six examples returned their expected statuses, and regenerated reports matched packaged samples byte-for-byte. No prebuilt executable was used. Workspace cleanup removes the superseded specification from newly generated packages, leaving 61 payload files; the published historical archive is unchanged.

The cleaned 61-file package also passed the same clean-extraction, 38-test, six-example, and report-comparison checks. Its temporary verification archive was removed after the check; the published release ZIP is retained locally.

```powershell
python tools\package_submission.py --output build\paper-release
python tools\verify_submission.py build\paper-release\clarifix-paper-alignment-2.zip
```

The release tag identifies the exact source commit. The publication check compares each manifest digest against the committed blob before uploading the ZIP, checksum, authoritative paper, and alignment addendum. The original release remains historical; no course hand-in is performed.

## Course actions

GitHub publication does not hand in the coursework. Confirm the calendar deadline and central submission destination, provide instructor access to the private repository, submit the ZIP/link, print the authoritative paper and alignment addendum, and prepare the index card. Linux/macOS commands are supplied but are not verified in this Windows environment. This phase recognizes source; it does not execute programs.
