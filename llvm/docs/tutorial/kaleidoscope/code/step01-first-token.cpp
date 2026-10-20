#include <cstdio>

int main() {
  printf("Kaleidoscope Token Inspector (Step 1)\n");
  printf("Type characters and press Enter (Ctrl+D to exit):\n\n");

  int c;
  while ((c = getchar()) != EOF) {
    if (c != '\n' && c != '\r')
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(c), c);
  }

  printf("[EOF]\n");
  return 0;
}
