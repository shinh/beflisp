#include "libef.h"

typedef struct {
  int x;
  int y;
} S;

__attribute__((noinline)) int add(S* s) {
  return s->x + s->y;
}

int main() {
  S s;
  s.x = getchar();
  s.y = getchar();
  putchar(add(&s));
  return 0;
}
