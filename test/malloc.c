#include "libef.h"

int main() {
  int* buf = calloc(6, 4);
  int* buf2 = calloc(6, 4);
  buf[0] = 'h';
  buf[1] = 'e';
  buf[2] = 'l';
  buf[3] = 'l';
  buf[4] = 'o';
  buf[5] = 0;
  buf2[0] = 'w';
  buf2[1] = 'o';
  buf2[2] = 'r';
  buf2[3] = 'l';
  buf2[4] = 'd';
  buf2[5] = 0;
  print_str(buf);
  print_str(buf2);
  free(buf);
  free(buf2);
  return 0;
}
