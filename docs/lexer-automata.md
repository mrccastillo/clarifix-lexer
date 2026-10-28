# ClariFix lexical automata

Implementation: `lexer.c`. The recognizer is handwritten C11. No generated scanner, regular expressions, or whole-word comparison functions classify source lexemes.

## Vocabulary, character states, and symbol storage

The recognition graph has 55 fixed reserved/operator/delimiter spellings and two separate Boolean accepting paths. It recognizes 57 spellings in total, but none prepopulate the symbol table. Boolean constants are outside the reserved array and enter the symbol table only when encountered.

| Group | Count | Spellings |
| --- | ---: | --- |
| Keywords | 13 | set show ask if elif else while for in function return break continue |
| Boolean accepting paths | 2 | true false |
| Word operators | 4 | and or not is |
| Noise words | 4 | to the of then |
| Symbol operators | 25 | + - * / // % ^ ++ -- && \|\| ! == != < > <= >= = += -= *= /= %= ^= |
| Delimiters | 9 | ( ) [ ] { } , . : |

`transition(state, character)` follows sparse character edges. A node has its outgoing child/sibling indices, an optional fixed-spelling index, and a Boolean accepting marker. Fixed accepting actions classify token type and noise subtype without comparing a completed source word against a list.

| State | Input | Transition/action |
| --- | --- | --- |
| Start | ASCII letter or underscore | Follow/create a character edge; enter Word. |
| Word | ASCII letter, underscore, or digit | Follow/create edge and increment length. |
| Word | Other or EOF | At the entire word boundary, accept its fixed type, Boolean constant, or IDENT. Leave the boundary unconsumed. |
| Word | Length exceeds 32 | Stop growing the path, finish scanning the word, emit E104, recover the physical line. |

`setx`, `set_total`, and `set2` are identifiers rather than keyword prefixes; `Set` is case-distinct from `set`. Noise actions are 1 for `to`, 2 for `then`, and 3 for `the`/`of`. Source reports retain allowed noise; parser tokens normalize `to` to ASSIGN and omit other fillers.

### Hash table and equality

`CfResult.buckets` contains 211 initially empty bucket heads. `symtab_insert` walks lexeme characters to get an exact trie-terminal identity and computes djb2, starting at 5381 and updating `hash = hash * 33 + byte` with unsigned 64-bit wraparound. The bucket is `hash % 211`.

Only that bucket's chain is searched. An entry matches when both its terminal identity and token enum match. Otherwise it is appended to the symbol array and linked at the bucket head. Each symbol's `next` is a one-based index or zero. IDs are first-insertion order; collisions preserve distinct names. Trie identity permits integer equality checks without whole-word comparisons. The table key is still the exact case-sensitive `(lexeme, token type)` pair.

Only identifiers and accepted constants are inserted. Fixed words, operators, delimiters, comments, layout, and EOF use symbol ID zero. Constant values are normalized exact decimal text, decoded character/string text, or Boolean text, with type carried separately. A trie does not replace the required hash table: all symbol lookup and insertion traverses the bucket chain.

## Operators and delimiters

`symbol` follows trie edges from the current source character, recording the last accepting fixed terminal. When a transition fails, it consumes only through that last accepted position. A nonaccepting prefix without a later accepted state is invalid. Comment openers are dispatched before this operator recognizer.

| First character | Accepting state after one character | Accepting continuation |
| --- | --- | --- |
| + | PLUS | + => INCREMENT; = => PLUS_ASSIGN |
| - | MINUS | - => DECREMENT; = => MINUS_ASSIGN |
| * | STAR | = => STAR_ASSIGN |
| / | SLASH | / => FLOOR_DIV; = => SLASH_ASSIGN; * dispatches block comment |
| % | MOD | = => MOD_ASSIGN |
| ^ | POWER | = => POWER_ASSIGN |
| ! | NOT | = => NE |
| = | ASSIGN | = => EQ |
| < | LT | = => LE |
| > | GT | = => GE |
| & | Not accepting | & => AND |
| \| | Not accepting | \| => OR |
| ( ) [ ] { } , . : | Corresponding delimiter | None |

Thus `+++` becomes INCREMENT followed by PLUS, and `&` alone produces E103. Braces are recognized only in `--braces` mode; default mode emits E103 for them. Bracket stack validation runs after lexical recognition.

## Numbers

| State | Input | Action |
| --- | --- | --- |
| Start | Digit | Enter Integer and consume digits. |
| Integer | Digit | Stay Integer. |
| Integer | Dot followed by digit | Consume dot; enter Fraction. |
| Fraction | Digit | Stay Fraction. |
| Integer/Fraction | Letter or underscore | E110; recover line. |
| Integer/Fraction | Other or EOF | Accept INT/DECIMAL without consuming boundary. |

The dot lookahead can equivalently be represented as a tentative-dot state with rollback. `20.` is INT then PERIOD; `19.99.` is DECIMAL then PERIOD. Minus is a separate operator. Exponents and leading-dot decimals are unsupported.

## Quoted literals and UTF-8

| State | Input | Action |
| --- | --- | --- |
| Start | Straight single/double quote | Record quote kind; enter Content. |
| Content | Matching quote | Close; accept CHAR/STRING. CHAR must contain exactly one scalar/escape, otherwise E112. |
| Content | Backslash | Enter Escape. |
| Escape | n, backslash, single quote, double quote | Count one character; return to Content. |
| Escape | Other | E111; recover line. |
| Content | Valid UTF-8 scalar | Consume scalar, count one character; stay Content. |
| Content | Invalid UTF-8 or NUL | E103; recover line. |
| Content/Escape | Raw newline or EOF | E102 for unclosed literal. |

UTF-8 validation accepts 1-4 byte scalar encodings and rejects overlong sequences, surrogate values, and values above U+10FFFF. Curly quotes encountered as code boundaries receive E109.

## Comments

| State | Input | Action |
| --- | --- | --- |
| Start | # | Enter LineComment. |
| LineComment | Anything except newline/EOF | Consume; remain LineComment. |
| LineComment | Newline/EOF | Accept LINE_COMMENT; leave newline for layout logic. |
| Start | /* | Enter BlockComment. |
| BlockComment | * followed by / | Consume both, accept BLOCK_COMMENT. |
| BlockComment | Newline | Consume through layout helper; remain BlockComment. |
| BlockComment | Other | Consume; remain BlockComment. |
| BlockComment | EOF | E106 at the opening marker; no successful comment row. |

Block comments do not nest. Comment content is ignored for parser tokens and escaped for reporting, including arbitrary invalid bytes. An equivalent explicit Star state detects a potential block terminator; the code uses one-character lookahead instead.

## Layout, reporting, and diagnostics

Indentation levels and matching brackets require auxiliary stacks and are not claimed to be finite automata. Default mode emits synthetic NEWLINE/INDENT/DEDENT tokens, with four-space levels and suppressed layout inside parentheses/square brackets. Braces mode suppresses layout tokens. Recovery skips the damaged physical line and removes its openers from bracket tracking; at most five errors are reported.

Section A of the report is a distinct source-ordered vector. Section B exports the unique hash-table entries, section C the stored diagnostic fragments, and section D source-category counts. Ordinary accepted tokens append to both vectors. Allowed noise and comments append to the report separately. Synthetic layout and EOF tokens have no report row. Original offsets always reference the normalized source buffer.

Diagnostics escape invalid UTF-8 and control bytes as `\xHH`. Their source column remains one-based UTF-8 byte position; the caret accounts for escape expansion and UTF-8 scalars. Terminal rendering of combining marks and wide glyphs can vary; byte locations remain authoritative.

`strcmp` in `main.c` parses command-line arguments and checks the filename extension. It never classifies ClariFix source. `strlen`/`memcpy` in the lexer are storage/output utilities. The recognition, identity, and hash-chain lookup paths contain no `strcmp`, `strncmp`, `memcmp`, or whole-word comparison loops.
