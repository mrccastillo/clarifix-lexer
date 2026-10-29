#include "dfa.h"

/* Hardcoded character transitions. Read one case at a time when tracing.
   Example: case W_I: 'f' goes to W_IF; 'n' to W_IN; 's' to W_IS.
   A different letter/digit goes to the ordinary identifier state. */
WordState word_next(WordState state, unsigned char c)
{
    switch (state) {
    case W_START:
        if (c == 'a') return W_A;
        if (c == 'b') return W_B;
        if (c == 'c') return W_C;
        if (c == 'e') return W_E;
        if (c == 'f') return W_F;
        if (c == 'i') return W_I;
        if (c == 'n') return W_N;
        if (c == 'o') return W_O;
        if (c == 'r') return W_R;
        if (c == 's') return W_S;
        if (c == 't') return W_T;
        if (c == 'w') return W_W;
        return W_IDENT;
    case W_A:
        if (c == 'n') return W_AN;
        if (c == 's') return W_AS;
        return W_IDENT;
    case W_AN:
        if (c == 'd') return W_AND;
        return W_IDENT;
    case W_AND:
        return W_IDENT;
    case W_AS:
        if (c == 'k') return W_ASK;
        return W_IDENT;
    case W_ASK:
        return W_IDENT;
    case W_B:
        if (c == 'r') return W_BR;
        return W_IDENT;
    case W_BR:
        if (c == 'e') return W_BRE;
        return W_IDENT;
    case W_BRE:
        if (c == 'a') return W_BREA;
        return W_IDENT;
    case W_BREA:
        if (c == 'k') return W_BREAK;
        return W_IDENT;
    case W_BREAK:
        return W_IDENT;
    case W_C:
        if (c == 'o') return W_CO;
        return W_IDENT;
    case W_CO:
        if (c == 'n') return W_CON;
        return W_IDENT;
    case W_CON:
        if (c == 't') return W_CONT;
        return W_IDENT;
    case W_CONT:
        if (c == 'i') return W_CONTI;
        return W_IDENT;
    case W_CONTI:
        if (c == 'n') return W_CONTIN;
        return W_IDENT;
    case W_CONTIN:
        if (c == 'u') return W_CONTINU;
        return W_IDENT;
    case W_CONTINU:
        if (c == 'e') return W_CONTINUE;
        return W_IDENT;
    case W_CONTINUE:
        return W_IDENT;
    case W_E:
        if (c == 'l') return W_EL;
        return W_IDENT;
    case W_EL:
        if (c == 'i') return W_ELI;
        if (c == 's') return W_ELS;
        return W_IDENT;
    case W_ELI:
        if (c == 'f') return W_ELIF;
        return W_IDENT;
    case W_ELIF:
        return W_IDENT;
    case W_ELS:
        if (c == 'e') return W_ELSE;
        return W_IDENT;
    case W_ELSE:
        return W_IDENT;
    case W_F:
        if (c == 'a') return W_FA;
        if (c == 'o') return W_FO;
        if (c == 'u') return W_FU;
        return W_IDENT;
    case W_FA:
        if (c == 'l') return W_FAL;
        return W_IDENT;
    case W_FAL:
        if (c == 's') return W_FALS;
        return W_IDENT;
    case W_FALS:
        if (c == 'e') return W_FALSE;
        return W_IDENT;
    case W_FALSE:
        return W_IDENT;
    case W_FO:
        if (c == 'r') return W_FOR;
        return W_IDENT;
    case W_FOR:
        return W_IDENT;
    case W_FU:
        if (c == 'n') return W_FUN;
        return W_IDENT;
    case W_FUN:
        if (c == 'c') return W_FUNC;
        return W_IDENT;
    case W_FUNC:
        if (c == 't') return W_FUNCT;
        return W_IDENT;
    case W_FUNCT:
        if (c == 'i') return W_FUNCTI;
        return W_IDENT;
    case W_FUNCTI:
        if (c == 'o') return W_FUNCTIO;
        return W_IDENT;
    case W_FUNCTIO:
        if (c == 'n') return W_FUNCTION;
        return W_IDENT;
    case W_FUNCTION:
        return W_IDENT;
    case W_I:
        if (c == 'f') return W_IF;
        if (c == 'n') return W_IN;
        if (c == 's') return W_IS;
        return W_IDENT;
    case W_IF:
        return W_IDENT;
    case W_IN:
        return W_IDENT;
    case W_IS:
        return W_IDENT;
    case W_N:
        if (c == 'o') return W_NO;
        return W_IDENT;
    case W_NO:
        if (c == 't') return W_NOT;
        return W_IDENT;
    case W_NOT:
        return W_IDENT;
    case W_O:
        if (c == 'f') return W_OF;
        if (c == 'r') return W_OR;
        return W_IDENT;
    case W_OF:
        return W_IDENT;
    case W_OR:
        return W_IDENT;
    case W_R:
        if (c == 'e') return W_RE;
        return W_IDENT;
    case W_RE:
        if (c == 't') return W_RET;
        return W_IDENT;
    case W_RET:
        if (c == 'u') return W_RETU;
        return W_IDENT;
    case W_RETU:
        if (c == 'r') return W_RETUR;
        return W_IDENT;
    case W_RETUR:
        if (c == 'n') return W_RETURN;
        return W_IDENT;
    case W_RETURN:
        return W_IDENT;
    case W_S:
        if (c == 'e') return W_SE;
        if (c == 'h') return W_SH;
        return W_IDENT;
    case W_SE:
        if (c == 't') return W_SET;
        return W_IDENT;
    case W_SET:
        return W_IDENT;
    case W_SH:
        if (c == 'o') return W_SHO;
        return W_IDENT;
    case W_SHO:
        if (c == 'w') return W_SHOW;
        return W_IDENT;
    case W_SHOW:
        return W_IDENT;
    case W_T:
        if (c == 'h') return W_TH;
        if (c == 'o') return W_TO;
        if (c == 'r') return W_TR;
        return W_IDENT;
    case W_TH:
        if (c == 'e') return W_THE;
        return W_IDENT;
    case W_THE:
        if (c == 'n') return W_THEN;
        return W_IDENT;
    case W_THEN:
        return W_IDENT;
    case W_TO:
        return W_IDENT;
    case W_TR:
        if (c == 'u') return W_TRU;
        return W_IDENT;
    case W_TRU:
        if (c == 'e') return W_TRUE;
        return W_IDENT;
    case W_TRUE:
        return W_IDENT;
    case W_W:
        if (c == 'h') return W_WH;
        return W_IDENT;
    case W_WH:
        if (c == 'i') return W_WHI;
        return W_IDENT;
    case W_WHI:
        if (c == 'l') return W_WHIL;
        return W_IDENT;
    case W_WHIL:
        if (c == 'e') return W_WHILE;
        return W_IDENT;
    case W_WHILE:
        return W_IDENT;
    case W_IDENT:
        return W_IDENT;
    }
    return W_IDENT;
}

/* A complete prefix becomes a keyword only at a word boundary.
   Incomplete prefixes ("i", "fun") are still valid identifiers. */
CfTokenType word_token(WordState state)
{
    switch (state) {
    case W_SET: return CF_SET;
    case W_SHOW: return CF_SHOW;
    case W_ASK: return CF_ASK;
    case W_IF: return CF_IF;
    case W_ELIF: return CF_ELIF;
    case W_ELSE: return CF_ELSE;
    case W_WHILE: return CF_WHILE;
    case W_FOR: return CF_FOR;
    case W_IN: return CF_IN;
    case W_FUNCTION: return CF_FUNCTION;
    case W_RETURN: return CF_RETURN;
    case W_BREAK: return CF_BREAK;
    case W_CONTINUE: return CF_CONTINUE;
    case W_AND: return CF_AND;
    case W_OR: return CF_OR;
    case W_NOT: return CF_NOT;
    case W_IS: return CF_IS;
    case W_TO: return CF_NOISE;
    case W_THE: return CF_NOISE;
    case W_OF: return CF_NOISE;
    case W_THEN: return CF_NOISE;
    case W_TRUE: return CF_BOOL;
    case W_FALSE: return CF_BOOL;
    default: return CF_IDENT;
    }
}

const char *word_state_name(WordState state)
{
    switch (state) {
    case W_START: return "START";
    case W_A: return "A";
    case W_AN: return "AN";
    case W_AND: return "AND";
    case W_AS: return "AS";
    case W_ASK: return "ASK";
    case W_B: return "B";
    case W_BR: return "BR";
    case W_BRE: return "BRE";
    case W_BREA: return "BREA";
    case W_BREAK: return "BREAK";
    case W_C: return "C";
    case W_CO: return "CO";
    case W_CON: return "CON";
    case W_CONT: return "CONT";
    case W_CONTI: return "CONTI";
    case W_CONTIN: return "CONTIN";
    case W_CONTINU: return "CONTINU";
    case W_CONTINUE: return "CONTINUE";
    case W_E: return "E";
    case W_EL: return "EL";
    case W_ELI: return "ELI";
    case W_ELIF: return "ELIF";
    case W_ELS: return "ELS";
    case W_ELSE: return "ELSE";
    case W_F: return "F";
    case W_FA: return "FA";
    case W_FAL: return "FAL";
    case W_FALS: return "FALS";
    case W_FALSE: return "FALSE";
    case W_FO: return "FO";
    case W_FOR: return "FOR";
    case W_FU: return "FU";
    case W_FUN: return "FUN";
    case W_FUNC: return "FUNC";
    case W_FUNCT: return "FUNCT";
    case W_FUNCTI: return "FUNCTI";
    case W_FUNCTIO: return "FUNCTIO";
    case W_FUNCTION: return "FUNCTION";
    case W_I: return "I";
    case W_IF: return "IF";
    case W_IN: return "IN";
    case W_IS: return "IS";
    case W_N: return "N";
    case W_NO: return "NO";
    case W_NOT: return "NOT";
    case W_O: return "O";
    case W_OF: return "OF";
    case W_OR: return "OR";
    case W_R: return "R";
    case W_RE: return "RE";
    case W_RET: return "RET";
    case W_RETU: return "RETU";
    case W_RETUR: return "RETUR";
    case W_RETURN: return "RETURN";
    case W_S: return "S";
    case W_SE: return "SE";
    case W_SET: return "SET";
    case W_SH: return "SH";
    case W_SHO: return "SHO";
    case W_SHOW: return "SHOW";
    case W_T: return "T";
    case W_TH: return "TH";
    case W_THE: return "THE";
    case W_THEN: return "THEN";
    case W_TO: return "TO";
    case W_TR: return "TR";
    case W_TRU: return "TRU";
    case W_TRUE: return "TRUE";
    case W_W: return "W";
    case W_WH: return "WH";
    case W_WHI: return "WHI";
    case W_WHIL: return "WHIL";
    case W_WHILE: return "WHILE";
    case W_IDENT: return "IDENT";
    }
    return "UNKNOWN";
}
