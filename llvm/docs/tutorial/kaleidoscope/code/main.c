#include <stdio.h>

extern double average(double x, double y);

int main() {
  printf("average(3.0, 4.0) = %f\n", average(3.0, 4.0));
  return 0;
}
