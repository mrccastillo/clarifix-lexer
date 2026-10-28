# Updated-paper decisions and traceability

Authoritative source: `ClariFix-Updated-Paper.pdf`, copied byte-for-byte from the user's updated paper. The copy and the user's file are not edited by this implementation.

| Paper location | Implementation |
| --- | --- |
| p.18: entry fields | Symbols expose ID, lexeme, token type, category, converted value, scalar lexeme length, first line/column, and occurrence count. |
| p.18: djb2, 211 buckets, chaining | `CfResult.buckets[211]` holds chain heads. `CfSymbol.next` is the one-based next ID. Hash arithmetic wraps at 64 bits, then takes `% 211`. |
| pp.18–19: case-sensitive `(lexeme,type)` key | A character trie gives an exact lexeme identity. A bucket compares that integer identity and token enum, so collisions cannot merge different names and no whole-word comparison is needed. |
| p.19: reserved words first | Accepting states classify reserved words before symbol insertion. Keywords, noise, operators, delimiters, and comments never become symbols. |
| p.19: Boolean constants | `true` and `false` have Boolean DFA accepting states outside the reserved array. Symbol entries are created only when those constants occur in the source. |
| p.19: operations | Fresh result initialization corresponds to `symtab_init`; hash-chain lookup/insertion to `symtab_lookup`/`symtab_insert`; parser/source vectors to `symtab_record_token`; diagnostic records to `symtab_record_invalid`; report export to `symtab_write_file`; `cf_free` frees every allocation. Function spellings differ from the illustrative operation names; responsibilities agree. |
| p.20: report | `--report FILE` writes the source filename, A token list, B unique symbols, C invalid records, and D category summary. Columns are fixed-width within each table and left-aligned, with `(none)` in empty sections. |
| p.20: embedded example | `examples/sample.clx` yields nine source tokens, two symbols (`total`, `10`), and the stated category counts and first locations. |

## Resolved ambiguities

The paper still contradicts itself in several places. These are explicit implementation decisions, not silent edits to the PDF:

- **Extension:** use `.clx` from the page-20 sample; retain `.clf` as a compatibility alias. Other extensions receive E100.
- **Layout:** default to four-space indentation, following pp.14–15 grammar and its correct program. `--braces` selects the free-field syntax described on p.8 and illustrated on p.10. No source-based mode guessing is performed.
- **Quotes:** follow the explicit straight-quote rule on pp.7–8. Smart quotes in the p.9 examples remain invalid; replace them with straight quotes when writing code.
- **Decrement:** use ASCII `--` as printed in the p.5 precedence table. The Unicode dash on p.3 is treated as a typography inconsistency.
- **Noise:** reports retain original noise tokens. Parser tokens normalize allowed `to` to ASSIGN and omit the other fillers, preserving a separator required by the assignment grammar. E105 is checked early by the lexer; p.13 assigns noise misuse to the parser while the error taxonomy lists it with lexical conditions.
- **Optional declaration:** `int` is not added as a reserved declaration keyword, because it is absent from the keyword list and core grammar. Use `set score = 90.` for valid declarations. Lexing `int score = 90.` alone does not validate that statement.
- **Values and lengths:** numbers use exact normalized base-10 text, so very large valid literals are not silently rounded or rejected. String and character values decode supported escapes; source lexeme length counts Unicode scalars, including source quotes and escape characters. First columns remain byte-based as before.
- **Invalid section:** it includes every reported lexical diagnostic, including malformed recognized constructs, with its offending fragment. Recovery skips the remainder of that physical line. At most five records are emitted; partial reports do not claim that skipped source was analyzed.
- **Summary:** category counts are occurrence counts in A; B is deduplicated. Synthetic layout/EOF tokens are excluded from the source-token total. Invalid fragments have a separate count and are excluded from successful-token totals.

Missing colons/periods, undefined variables, wrong types, and runtime evaluation remain later phases, as the paper specifies on p.13. Its deliberate-error program can therefore pass lexical analysis without producing the parser/semantic messages shown on p.16.

## No word-level matching

The manual hash table is for symbol storage, not a replacement for character automata. Character transitions determine token acceptance. Trie terminal identity permits exact collision resolution by integer comparison. Standard memory-copy, length, numeric-formatting, and CLI-option functions do not perform source word classification.
