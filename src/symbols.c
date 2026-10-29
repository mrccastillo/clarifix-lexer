#include "internal.h"

#include <stdint.h>
#include <string.h>

static char *save_text(CfResult *r, const char *s, size_t n)
{
    cf_require_room(n, CF_TEXT_SIZE - r->text_used, "symbol text bytes");
    char *saved = r->text + r->text_used;
    memcpy(saved, s, n);
    saved[n] = '\0';
    r->text_used += n + 1;
    return saved;
}
static char *constant_value(CfResult *r, const char *s, size_t n, CfTokenType type)
{
    if (type == CF_IDENT) return NULL;
    if (type == CF_INT || type == CF_DECIMAL) {
        size_t first = 0, end = n;
        while (first + 1 < n && s[first] == '0' && s[first + 1] != '.') first++;
        if (type == CF_DECIMAL) {
            while (end > first && s[end - 1] == '0') end--;
            if (s[end - 1] == '.') end++; /* An integral decimal retains .0. */
        }
        return save_text(r, s + first, end - first);
    }
    if (type == CF_BOOL) return save_text(r, s, n);
    char *value = save_text(r, s, n);
    size_t dest = 0;
    for (size_t i = 1; i + 1 < n; i++) {
        char c = s[i];
        if (c == '\\') { c = s[++i]; if (c == 'n') c = '\n'; }
        value[dest++] = c;
    }
    value[dest] = '\0';
    return value;
}
size_t cf_symbol_insert(Lexer *l, const char *s, size_t n, CfTokenType type,
                            size_t line, size_t column)
{
    uint64_t hash = 5381;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        hash = hash * UINT64_C(33) + c;
    }
    CfResult *r = l->r;
    size_t bucket = (size_t)(hash % CF_SYMBOL_BUCKETS);
    for (size_t id = r->buckets[bucket]; id; id = r->symbols[id - 1].next) {
        CfSymbol *entry = &r->symbols[id - 1];
        if (entry->type == type && strlen(entry->lexeme) == n && memcmp(entry->lexeme, s, n) == 0) {
            entry->occurrences++;
            if (l->trace) fprintf(l->trace, "TRACE SYMBOL REUSE id=%zu name=%s\n", id, entry->lexeme);
            return id;
        }
    }
    cf_require_room(r->symbol_count, CF_MAX_SYMBOLS, "symbols");
    CfSymbol *symbol = &r->symbols[r->symbol_count++];
    memset(symbol, 0, sizeof(*symbol));
    symbol->lexeme = save_text(r, s, n);
    symbol->type = type;
    symbol->value = constant_value(r, s, n, type);
    symbol->length = cf_scalar_count(s, n);
    symbol->line = line; symbol->column = column; symbol->occurrences = 1;
    symbol->next = r->buckets[bucket];
    r->buckets[bucket] = r->symbol_count;
    if (l->trace) fprintf(l->trace, "TRACE SYMBOL INSERT id=%zu name=%s\n", r->symbol_count, symbol->lexeme);
    return r->symbol_count;
}
