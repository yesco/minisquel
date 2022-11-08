////////////////////////////////////////
// - ansi screen

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>

// - higher level colors
typedef enum color{black, red, green, yellow, blue, magenta, cyan, white, none,
  // jsk's guesses
  purple= 93,
  orange= 214,
  brightorange= yellow+8,
} color;

int screen_rows= 24, screen_cols= 80;
// https://stackoverflow.com/questions/1022957/getting-terminal-width-in-c

void screen_init(int sig) {
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);
  screen_rows = w.ws_row;
  screen_cols = w.ws_col;
}

struct termios _orig_termios, _jio_termios;

void cursoron();

void _jio_exit() {
  tcsetattr(0, TCSANOW, &_orig_termios);

  // deinit mouse/jio
  fprintf(stderr, "\e[?1000;1003;1006;1015l");

  // sanity
  cursoron();
}

void jio() {
  // get screen size
  screen_init(0);
  signal(SIGWINCH, screen_init);

  // xterm mouse init
  fprintf(stderr, "\e[?1000;1003;1006;1015h");

  tcgetattr(0, &_orig_termios);

  // ctrl-c doesn't do it?
  atexit(_jio_exit);
  
  _jio_termios= _orig_termios;

return;
 
  // raw terminal in; catch ^C & ^Z
  cfmakeraw(&_jio_termios);
  // enable terminal out
  _jio_termios.c_oflag |= OPOST;

  tcsetattr(0, TCSANOW, &_jio_termios);

  // catch window resize interrupts
}

////////////////////////////////////////
// - ansi screen

// good ref
// - https://www.ibm.com/docs/en/aix/7.1?topic=x-xterm-command#xterm__mouse

void reset() { printf("\e[48;5;0m\e[38;5;7m\e[23m\e[24m\e[0m"); }

//void cls() { printf("\e[H[2J[3J"); }

void clear() { printf("\x1b[2J\x1b[H"); }
void clearend() { printf("\x1b[K"); }
void cleareos() { printf("\x1b[J"); }

// notice thi sis zero based! (not ansi)
void gotorc(int r, int c) {
  // negative values breaks the ESC seq giving garbage on the screen!
  assert(r>=0 && c>=0);
  printf("\x1b[%d;%dH", r+1, c+1);
}

void inverse(int on) { printf(on ? "\x1b[7m" : "\x1b[m"); }
void fgcolor(int c) { printf("\x1b[[3%dm", c); }
void bgcolor(int c) { printf("\x1b[[4%dm", c); }

void savescreen() { printf("\x1b[?47h"); }
void restorescreen() { printf("\x1b[?47l"); }
// cursor
void hide() { printf("\e[?25l"); }
void show() { printf("\e[?25h"); }
void save() { printf("\e7"); }
void restore() { printf("\e8"); }

void cursoroff() { printf("\x1b[?25l"); }
void cursoron() { printf("\x1b[?25h"); }


// can use insert characters/line from
// - http://vt100.net/docs/vt102-ug/chapter5.html
void insertmode(int on) { printf("\x1b[4%c", on ? 'h' : 'l'); }

void _color(int c) {
  if (c >= 0) {
    printf("5;%dm", c);
  } else {
    c= -c;
    int b= c&0xff, g= (c>>8)&0xff, r=(c>>16);
    printf("2;%d;%d;%dm", r, g, b);
  }
}

enum color _fg= black, _bg= white;

color fg(int c) { color r=_fg; printf("\e[38;"); _color(c); _fg= c; return r; }
color bg(int c) { color r=_bg; printf("\e[48;"); _color(c); _bg= c; return r; }

// most colors are per default bold!
//int bold(int c /* 0-7 */) { return c+8; }
int bold(int c /* 0-7 */) { return c+8; }
int rgb(int r, int g, int b /* 0-5 */) { return 0x10+ 36*r + 6*g + b; }
int gray(int c /* 0-7 */) { return 0xe8+  c; }
int RGB(int r, int g, int b /* 0-255 */) { return -(r*256+g)*256+b; }

void boldon() { printf("\e[1m"); }
void boldoff() { printf("\e[21m"); } // not working termux

void underline() { printf("\e[4m"); }
void end_underline() { printf("\e[24m"); }

// Reverse video!
//   Ok, this is going to be "funny". Since I've been using "true video" (white on black) the bold wasn't really bolding black letters, instead I typeset it in red color. (ugh!)
//   So, turns out the solution is to use reverse video. Then bold of black works! (but not of white...).
//   Also, to pull this off, since reverse video makes fg be background and bg be foreground color, we switch those if we're in reverse mode!
//  Second, there is no way to turn off bold, except turn everything off, so we need to recolor it (and for every line of our .ANSI-file. Use recolor();

// TODO:   modify the state anmake it saveable... as a struct.

int _reverse=1; // 1 and bold works for black!

// TODO: is reverse mode with colors reverse a decent mode for "dark/night" theme?

void reverse() { printf("\e[7m"); }

void recolor() {
  if (_reverse) reverse();
  bg(_bg); fg(_fg);
  // TODO: underline, bold, italics, 
  // means need to keep track of state...
}

// adjusted colors
color _C(int n) {
  if (_reverse) reverse();
  // dark blue
  return fg(n);

}

color _B(int n) {
  if (_reverse) reverse();
  return bg(n);
}

// adjusted colors
color C(int n) {
  // TODO: generalize/move to w.c?
  // link color
  if (n==27) {
    if (_reverse && _bg==black)
      n=rgb(0,0,5);
    else
      n=rgb(0,3,5);
  }
  return (_reverse? _B : _C)(n==blue? rgb(0,0,1) : n);
}

color B(int n) {
  color (*f)(int n)= (_reverse? _C : _B);
  // white => offwhite
  //if (n==white || n==15) return f(n); // no good as it hilite bold changes bg color
  // good... not so tiring
  if (n==white || n==15) return f(254);

  return f(n==blue? rgb(0,0,1) : n==white ? n+8 : n);
}

color readablefg() {
  // TODO: generalize for rgb and RGB, use hue?
  // TODO: bad name!
  int dark= _bg==black||_bg==blue||_bg==red||_bg==magenta;
  return C(dark? white : black);
}
