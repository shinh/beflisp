#include "libef.h"

__attribute__((noinline)) void* retNull() {
  return 0;
}

int main() {
  if (retNull()) {
    putchar('F');
    putchar('A');
    putchar('I');
    putchar('L');
  } else {
    putchar('O');
    putchar('K');
  }
  return 0;
}
