# mini_cpp – C++-ähnlicher Interpreter

Dieses Projekt implementiert eine **kleine, C++-ähnliche Sprache** inklusive

- Lexer
- Parser (AST)
- Semantische Analyse
- Interpreter
- interaktive REPL

Unterstützt werden u.a.:

- primitive Typen (`int`, `bool`, `char`, `string`)
- Referenzen (`T&`)
- Funktionen mit Overloading
- Klassen mit einfacher Vererbung
- Konstruktoren
- statischer vs. dynamischer Dispatch (`virtual`)
- Object Slicing (bewusst wie in C++)

Das Projekt ist so aufgebaut, dass der Code (bis auf REPL-Features) **echtes C++-Subset** bleibt.

---

## Build / Kompilieren

### Voraussetzungen
- Linux / macOS
- C++17 kompatibler Compiler (`g++` oder `clang++`)
- CMake ≥ 3.16
- Ninja (empfohlen, aber optional)

### Build-Schritte

```bash
git clone <repo-url>
cd mini_cpp

rm -rf build
cmake -S . -B build -G Ninja
cmake --build build

### Build-Starten

./build/mini_cpp

### Test-Starten

./build/mini_cpp tests/neg/file.cpp
./build/mini_cpp tests/pos/file.cpp
