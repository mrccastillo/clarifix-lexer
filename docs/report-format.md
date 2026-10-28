# Instructor Symbol Table format

Generate the submission report with:

```powershell
.\build\clarifix.exe --report build\average-symbol-table.txt examples\average.clf
```

The destination must not exist. The file is UTF-8, tab-separated, and has exactly two columns: `TOKEN TYPE` and `LEXEME`. It lists successful source lexemes in occurrence order, retaining duplicates, comments, and allowed noise words. It excludes synthetic layout tokens, EOF, invalid fragments, and unused preloaded vocabulary.

| Internal token | Report token |
| --- | --- |
| IDENT | IDENTIFIER |
| INT | INT_LITERAL |
| DECIMAL | DECIMAL_LITERAL |
| CHAR | CHAR_LITERAL |
| STRING | STRING_LITERAL |
| BOOL | BOOL_LITERAL |
| NOISE | NOISE_WORD |
| LINE_COMMENT / BLOCK_COMMENT | LINE_COMMENT / BLOCK_COMMENT |
| SET SHOW ASK IF ELIF ELSE WHILE FOR IN FUNCTION RETURN BREAK CONTINUE | Same name |
| PLUS MINUS STAR SLASH FLOOR_DIV MOD POWER INCREMENT DECREMENT | Same name |
| AND OR NOT IS EQ NE LT GT LE GE | Same name |
| ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN | Same name |
| LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE COMMA PERIOD COLON | Same name |
| NEWLINE INDENT DEDENT EOF | Omitted |

Original spelling is retained, including quote characters, comment markers, and aliases. `to` is reported as NOISE_WORD even though its parser token becomes ASSIGN. Both `and` and `&&` report AND with distinct lexemes.

Backslash becomes `\\`, tab becomes `\t`, LF becomes `\n`, and CR becomes `\r`. Other ASCII control bytes, DEL, and invalid UTF-8 bytes become uppercase `\xHH`. Valid UTF-8 is preserved. As a result, every row occupies one physical line and two columns. These escapes describe source text, not decoded runtime values.

The instructor sample includes comments and occurrence-ordered literals; this report follows that interpretation. It is distinct from the internal, deduplicated symbol inventory exported by `--symbols`. Detailed parser tokens remain available through `--tokens` or stdout. No output flag changes parser-facing tokens.

On lexical error, exit status is 1 and stderr labels both token and report output as partial. Completed lexemes before the error and after recoverable lines may appear. No error marker is inserted into the two-column file because it would be mistaken for a recognized source lexeme. Use the matching diagnostic file when reviewing a partial report.
