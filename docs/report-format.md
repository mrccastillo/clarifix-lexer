# Updated-paper Symbol Table report

`--report FILE` implements pages 18–20 of `ClariFix-Updated-Paper.pdf`. Use `.clx` source files; `.clf` is accepted for compatibility. The basename of the source is printed so the output does not depend on a machine's checkout directory. Destinations must not already exist.

## Four sections

| Section | Contents |
| --- | --- |
| A: TOKEN LIST | No., Line, Col, Lexeme, Token Type, Category. Every successful source occurrence, including comments and noise. No synthetic EOF/NEWLINE/INDENT/DEDENT. |
| B: SYMBOL TABLE | ID, Lexeme, Token Type, Value, Length, First Seen. Unique identifiers and constants only, in first-insertion order. |
| C: INVALID / UNRECOGNIZED TOKENS | Line, Col, Lexeme, Error, Reason. The offending fragment for each reported lexical diagnostic. |
| D: SUMMARY | Source-occurrence counts for identifiers, keywords, noise, constants, delimiters, comments, operators; invalid count; total successful source tokens. |

Each data table uses spaces, fixed column start positions within that table, and left alignment. Widths expand to fit content; lexemes are never silently clipped. Empty A/B/C data sections print `(none)`. First Seen is `line:column`.

## Names and categories

| Internal token | Public name | Category |
| --- | --- | --- |
| IDENT | IDENTIFIER | Identifier |
| INT | CONST_INT | Constant |
| DECIMAL | CONST_DECIMAL | Constant |
| CHAR | CONST_CHAR | Constant |
| STRING | CONST_STRING | Constant |
| BOOL | CONST_BOOLEAN | Constant |
| NOISE | NOISE | Noise word |
| LINE_COMMENT | SINGLE_COMMENT | Comment |
| BLOCK_COMMENT | MULTI_COMMENT | Comment |
| PERIOD | STATEMENT_TERMINATOR | Delimiter |
| SET SHOW ASK IF ELIF ELSE WHILE FOR IN FUNCTION RETURN BREAK CONTINUE | Same names | Keyword |
| PLUS MINUS STAR SLASH FLOOR_DIV MOD POWER INCREMENT DECREMENT | Same names | Operator |
| AND OR NOT IS EQ NE LT GT LE GE | Same names | Operator |
| ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN | Same names | Operator |
| LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE COMMA COLON | Same names | Delimiter |
| NEWLINE INDENT DEDENT EOF | Omitted | Not source occurrences |

## Symbol attributes

The hash table starts empty and contains only identifiers/constants, keyed by original case-sensitive lexeme plus token type. IDs begin at 1. Repeated occurrences share an ID, first location, and value; occurrence counts increment. No unused reserved vocabulary is exported. Boolean literals are constants inserted on demand.

`--symbols FILE` exposes `ID LEXEME TOKEN CATEGORY VALUE LENGTH FIRST_LINE FIRST_COLUMN OCCURRENCES` as TSV. Identifiers use `-` for value. Strings/characters store decoded UTF-8 values; quotes are added around those values in the human report. In the TSV, decoded values are escaped without surrounding presentation quotes, so an empty string has an empty VALUE cell.

Integer values strip leading zeros, retaining one zero. Decimal values also strip redundant trailing fractional zeros, retaining `.0` when integral. Exact normalized base-10 text preserves arbitrarily large valid literals without overflow/rounding. Different original spellings remain different symbol keys even if values normalize identically: `00010` and `10` have separate IDs and value `10`.

Length counts Unicode scalars in the original source lexeme, including surrounding quotes and source escape characters. For example `'é'` has length 3 although its UTF-8 byte length is 4. First line/column are one-based; columns count bytes in normalized UTF-8 source.

## Escaping, invalid records, and compatibility

Original spelling remains in source lexeme columns. Backslash, tab, LF, and CR become `\\`, `\t`, `\n`, and `\r`; other ASCII controls, DEL, and invalid bytes become uppercase `\xHH`. Valid UTF-8 is preserved. Embedded newlines are escaped so a source occurrence occupies one output row.

Invalid section C includes malformed constants, illegal characters, bad indentation, misplaced noise, and unclosed constructs. It records the offending fragment rather than labeling the entire recovery-skipped line as individual tokens. Lexical analysis stops after five errors. Section D counts invalid records separately and excludes them and synthetic layout tokens from Total tokens. On error, stderr explicitly identifies output as partial. Section C is not a claim that all skipped input was analyzed.

`--table FILE` retains the original two-column `TOKEN TYPE`/`LEXEME` format using the current public names. `--tokens FILE` retains internal parser-token names and locations; default stdout remains that stream. `to` appears as NOISE in source reporting and ASSIGN in parser tokens. No report option changes lexical recognition.

The page-20 fixture is `examples/sample.clx`; its generated report is `submission/reports/sample-symbol-table.txt`. The expected nine occurrences and two symbols are asserted independently in the regression suite.
