#include "hsbi_runtime.h"

class Base {
public:
  int x;
  virtual int f() { return x; }
};

class Der : public Base {
public:
  int y;
  virtual int f() { return x + y; }
};

int main() {
  Der d; d.x = 2; d.y = 5;

  Base b2 = d;        // Slicing: b2 enthält nur Base-Anteil (x=2)
  print_int(b2.f());  // 2

  return 0;
}
/* EXPECT (Zeile für Zeile):
2
*/
