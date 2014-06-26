#include <stddef.h>

#include "libef.h"

int g_buf = -1;

int getChar(void) {
  int r;
  if (g_buf >= 0) {
    r = g_buf;
    g_buf = -1;
  } else {
    r = getchar();
    if (r == -1)
      exit(0);
  }
  return r;
}

int peekChar(void) {
  if (g_buf >= 0)
    return g_buf;
  int c = getchar();
  g_buf = c;
  return c;
}

void ungetChar(int c) {
  g_buf = c;
}

const int g_close_char = ')';

#include "lisp_common.c"

Atom* parse(void) {
  skipWS();
  int c = getChar();
  if (c == '(') {
    return parseList();
  } else if (c == '-' || (c >= '0' && c <= '9')) {
    return parseInt(c);
  } else if (c == ';') {
    while (c != '\n') {
      c = getChar();
    }
    return parse();
  } else {
    return parseStr(c);
  }
}

int main() {
  int buf[2];
  buf[0] = 't';
  buf[1] = '\0';
  g_t = createStr(buf);

  while (1) {
    Atom* expr = parse();
    Atom* result = eval(expr, NULL);
    printExpr(result);
    putchar('\n');
  }
}
