// To compile: g++ -lcurses -otestkeys testkeys.cpp
#include <curses.h>
#include <cstdio>
#include <iostream>

using namespace std;

int main(int argc, char* argv[]) {
  initscr();
  raw();
  nonl();
  keypad(stdscr, TRUE);
  noecho();
  mvprintw(1, 0, "Type keys to see the keycode, control-c to exit.");
  while (true) {
    int ch = getch();
    if (ch == 3) {
      break;
    }
    char s[81];
    sprintf(s, "[ch: %c] [value: %d]\n", ch, ch);
    mvprintw(3, 0, "You Entered: %s", s);
  }
  endwin();
  return 0;
}

