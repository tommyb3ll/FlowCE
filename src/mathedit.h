// mathedit.h - structure-aware editing keys for the 2D input line (KhiCAS CE console).
// The input line is one giac text buffer with a caret; mathinput.cc draws (a)/(b) as a stacked
// fraction, x^(2) as a superscript, f(a,b) as a call. These functions make the keys respect that
// structure. Pure functions of (buffer, caret, key): in place (memmove), no allocation, no static
// data, no giac, no drawing. Rules and examples: mathedit.cc.
#ifndef MATHEDIT_H
#define MATHEDIT_H

enum { ME_SQUARE = 1, ME_RECIP = 2 }; // the x² and x⁻¹ keys; other keys are their ASCII char

// A key typed at *caret in s (0-terminated, capacity cap bytes). Returns 1 if handled (s and
// *caret updated, possibly unchanged), 0 if the caller should insert the key's text itself.
// Keys handled: '/' '(' ')' '[' ']' '{' '}' ',' '=' '<' '>' ME_SQUARE ME_RECIP. Inside a string
// literal ("..."), never handled. Returns 1 without change when the result would not fit in cap.
// On 0, s is unchanged but *caret may have moved: ',' '=' '<' '>' ']' '}' typed in a denominator
// or an exponent first move the caret out of it, then the caller inserts the key at *caret.
int me_key(char * s, int cap, int * caret, int key);

// Backspace at *caret: 1 if handled structurally (s / *caret updated), 0 for the caller's own
// deletion (one character or glyph); on 0 nothing is changed.
int me_backspace(char * s, int * caret);

#endif
