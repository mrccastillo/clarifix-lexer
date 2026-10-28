# Group 4 submission checklist

Local release label: `lexer-compliance-1` (a release label, not a Git commit).
ZIP filename: `clarifix-lexer-compliance-1.zip`.
The ZIP's `MANIFEST.sha256` identifies every packaged file; the adjacent `.zip.sha256` identifies the complete archive.

## Local deliverables

- [x] Handwritten C11 lexer using character transitions and stable trie identities.
- [x] Two-column Symbol Table reporting with literal, comment, and noise-word occurrences.
- [x] Example sources, regression tests, automata documentation, and revised proposal source.
- [x] Printable revised proposal PDF generated and inspected.
- [x] All checks recorded in `docs/validation.md` and clean ZIP verified.

## Course hand-in

- [x] GitHub repository URL: https://github.com/mrccastillo/clarifix-lexer (private).
- [x] Published source snapshot: release tag `lexer-compliance-1`. Its commit identifies the source; the release ZIP manifest is verified against that commit.
- [ ] Record this revision in the course hand-in and retain the receipt.
- [ ] Calendar deadline and timezone: **not confirmed**. The supplied sheet states Thursday 11:59 PM only.
- [ ] Central repository/submission destination: **not supplied**. Use the instructor's course announcement.
- [ ] Submit the tested ZIP and actual GitHub repository link; retain the hand-in receipt.
- [ ] Print `docs/ClariFix-Revised-Proposal.pdf`.
- [ ] Prepare the group index card for checking day.

The partial report for `errors.clf` is deliberate. Review it with `reports/errors-diagnostics.txt`; do not treat it as successful lexical analysis. A successful lexer run produces tokens and reports, not program execution.
