#include <stddef.h>

#include "libef.h"

int g_prog_height;
int* g_line_len;
int** g_prog;

void readProg() {
  g_line_len = calloc(10000, 4);
  g_prog = (int**)calloc(10000, 4);
  int y = 0;
  int x = 0;
  for (;;) {
    int c = getchar();
    if (c == -1)
      break;
    if (c == '\n') {
      x = 0;
      y++;
      continue;
    }

    if (!g_prog[y])
      g_prog[y] = calloc(1000, 4);

    g_prog[y][x] = c;
    g_line_len[y] = ++x;
  }
  g_prog_height = y;
}

int g_x;
int g_y;
int g_dx;
int g_dy;
int g_px;
int g_py;

int peekChar(void) {
  if (g_x < g_line_len[g_y])
    return g_prog[g_y][g_x];
  else
    return ' ';
}

int getChar(void) {
  int r = peekChar();
  g_px = g_x;
  g_py = g_y;
  g_x += g_dx;
  g_y += g_dy;
  if (g_y < 0)
    g_y = g_prog_height - 1;
  else if (g_y >= g_prog_height)
    g_y = 0;
  if (g_dx) {
    if (g_x < 0)
      g_x = g_line_len[g_y] - 1;
    else if (g_x >= g_line_len[g_y])
      g_x = 0;
  }
  return r;
}

void ungetChar(int c) {
  g_x = g_px;
  g_y = g_py;
  if (peekChar() != c) {
    puts("weird ungetChar");
    exit(1);
  }
}

int g_close_char;

#include "lisp_common.c"

Atom* parse(void) {
  skipWS();
  int c = peekChar();
  if (c == '(' && g_dx != -1) {
    g_dx = 1;
    g_dy = 0;
    getChar();
    g_close_char = ')';
    return parseList();
  } else if (c == ')' && g_dx != 1) {
    g_dx = -1;
    g_dy = 0;
    getChar();
    g_close_char = '(';
    return parseList();
  } else if (c == '^' && g_dy != -1) {
    g_dx = 0;
    g_dy = 1;
    getChar();
    g_close_char = 'v';
    return parseList();
  } else if (c == 'v' && g_dy != 1) {
    g_dx = 0;
    g_dy = -1;
    getChar();
    g_close_char = '^';
    return parseList();
  } else if (c == '@') {
    exit(0);
  } else if (c == '-' || (c >= '0' && c <= '9')) {
    getChar();
    return parseInt(c);
  } else if (c == ';') {
    while (c != '\n') {
      c = getChar();
    }
    return parse();
  } else {
    getChar();
    return parseStr(c);
  }
  return NULL;
}

int main() {
  readProg();
  g_dx = 1;

  int buf[2];
  buf[0] = 't';
  buf[1] = '\0';
  g_t = createStr(buf);

  while (1) {
    Atom* expr = parse();
    //printExpr(expr);
    Atom* result = eval(expr, NULL);
    printExpr(result);
    putchar('\n');
  }
}
