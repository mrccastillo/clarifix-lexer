# ClariFix lexical automata

Implementation: `lexer.c`. The recognizer is handwritten C11. No generated scanner, regular expressions, or whole-word comparison functions classify source lexemes.

## Fixed vocabulary and identifier states

At initialization, `extend` inserts the 57 fixed spellings into a sparse character trie. Each node has a character edge, first-child index, sibling index, and optional accepting symbol ID. `transition(state, character)` inspects outgoing character edges. An accepting symbol carries its token type and, for noise words, an action code.

| Group | Count | Spellings |
| --- | ---: | --- |
| Keywords | 13 | set show ask if elif else while for in function return break continue |
| Boolean literals | 2 | true false |
| Word operators | 4 | and or not is |
| Noise words | 4 | to the of then |
| Symbol operators | 25 | + - * / // % ^ ++ -- && \|\| ! == != < > <= >= = += -= *= /= %= ^= |
| Delimiters | 9 | ( ) [ ] { } , . : |

Examples of fixed transitions: root --s--> s --e--> se --t--> SET, and root --s--> s --h--> sh --o--> sho --w--> SHOW. `set` is accepting only when the entire identifier-shaped word ends. A character class check requires the first character to be ASCII letter/underscore and subsequent characters to be ASCII letter/underscore/digit.

| State | Input | Transition/action |
| --- | --- | --- |
| Start | Letter or underscore | Follow/create the matching character edge; enter Word. |
| Word | Letter, underscore, or digit | Follow/create edge; increment length. |
| Word | Other or EOF | Accept terminal fixed type if present; otherwise assign IDENT. Do not consume the boundary. |
| Word | Length exceeds 32 | Stop growing trie, finish scanning word, emit E104, recover line. |

Fixed-word recognition uses the finite subgraph initialized from the vocabulary. Identifier interning extends that graph with character paths and stable terminal IDs. This dynamic identity store is separate from the finite fixed-word recognizer: no hash or string equality test determines identity. `setx`, `set_total`, and `set2` are identifiers; `Set` is case-distinct. A prefix of a keyword may itself be an identifier. Repeated names reuse their terminal ID.

Noise accepting actions are 1 for `to`, 2 for `then`, and 3 for `the`/`of`. They are assigned while constructing the fixed vocabulary, never by comparing an input word against candidate spellings. Context validation occurs after recognition. The report retains allowed noise lexemes; parser tokens normalize `to` to ASSIGN and omit fillers.

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

The report is a distinct source-ordered vector. Ordinary accepted tokens append to both vectors. Allowed noise and comments append to the report separately. Synthetic layout and EOF tokens have no report row. Original offsets always reference the normalized source buffer.

Diagnostics escape invalid UTF-8 and control bytes as `\xHH`. Their source column remains one-based UTF-8 byte position; the caret accounts for escape expansion and UTF-8 scalars. Terminal rendering of combining marks and wide glyphs can vary; byte locations remain authoritative.

`strcmp` in `main.c` parses command-line arguments and checks the filename extension. It never classifies ClariFix source. `strlen`/`memcpy` in the lexer are storage/output utilities. The recognition and identity paths contain no `strcmp`, `strncmp`, `memcmp`, or whole-word comparison loops.
