void f(int x) { }

int main() {
    f();       // Fehler: zu wenige Argumente
    f(1, 2);   // Fehler: zu viele Argumente

    return 0;
}
