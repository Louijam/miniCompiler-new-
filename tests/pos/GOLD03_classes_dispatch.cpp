#include "hsbi_runtime.h"

class Base {
public:
  int x;
  int g() { return x + 1; }           // nicht virtuell -> statischer Dispatch
  virtual int f() { return x; }       // virtuell -> dynamischer Dispatch
};

class Der : public Base {
public:
  int y;
  virtual int f() { return x + y; }   // überschreibt Base::f
};

int main() {
  Base b; b.x = 10;
  Der d; d.x = 3; d.y = 7;

  Base& rb = d;
  print_int(rb.f());   // 10  (Der::f via dynamic dispatch)
  print_int(rb.g());   // 4   (Base::g via static dispatch am statischen Typ Base)

  return 0;
}
/* EXPECT (Zeile für Zeile):
10
4
*/
