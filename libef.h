int getchar(void);
int putchar(int c);
int puts(const char* p);
// We need to declare malloc as int* to reduce bitcasts */
int* calloc(int n, int s);
void free(void* p);
void exit(int s);

__attribute__((noinline)) static void print_int(int v) {
  if (v < 0) {
    putchar('-');
    v = -v;
  }
  int buf[16];
  int n = 0;
  do {
    buf[n] = v % 10;
    v /= 10;
    n++;
  } while (v);

  while (n--) {
    putchar(buf[n] + '0');
  }
}

static void print_str(int* p) {
  while (*p) {
    putchar(*p);
    p++;
  }
}
