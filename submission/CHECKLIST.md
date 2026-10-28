# Group 4 submission checklist

Current revision/tag: `paper-alignment-2`. ZIP: `clarifix-paper-alignment-2.zip`.
Repository: https://github.com/mrccastillo/clarifix-lexer (private).
The ZIP's `MANIFEST.sha256` identifies every payload file; its adjacent `.zip.sha256` identifies the complete archive. The earlier `lexer-compliance-1` release is historical.

## Local deliverables

- [x] Updated-paper specification and page-by-page ambiguity decisions.
- [x] Character automata plus djb2, 211 buckets, separate-chained symbol table.
- [x] Unique identifiers/constants, converted values, categories, lengths, and first locations.
- [x] Four-section report with invalid-token records and category summary.
- [x] Page-20 sample: nine source occurrences, exactly two symbols (`total`, `10`).
- [x] All 39 tests and six example runs pass.
- [x] Updated paper and alignment addendum archived in the original release and Git history.
- [x] Printable addendum rendered and inspected.
- [x] Final ZIP rebuilt from a clean extraction and report samples verified.
- [x] Updated source and matching release assets published under `paper-alignment-2`.

## Course hand-in

- [ ] Give the instructor/group access to the private repository.
- [ ] Record the `paper-alignment-2` commit in the course hand-in.
- [ ] Confirm the calendar deadline and timezone; the supplied guideline only says Thursday 11:59 PM.
- [ ] Confirm the central submission destination from the instructor's announcement.
- [ ] Submit the tested ZIP and actual repository link; retain the receipt.
- [ ] Download and print the updated paper and addendum from the [alignment release](https://github.com/mrccastillo/clarifix-lexer/releases/tag/paper-alignment-2).
- [ ] Prepare the group index card for checking day.

The invalid example's report is deliberately partial; review it with its diagnostics. Successful lexical analysis is not program execution.
