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

#include "ansi.h"

// - higher level colors
typedef enum color{black, red, green, yellow, blue, magenta, cyan, white, none,
  // jsk's guesses
  purple= 93,
  orange= 214,
  brightorange= yellow+8,
} color;

int screen_rows= 24, screen_cols= 80;
// https://stackoverflow.com/questions/1022957/getting-terminal-width-in-c

int screen_resized= 0;

void screen_init(int sig) {
  struct winsize w;
  ioctl(0, TIOCGWINSZ, &w);
  screen_rows = w.ws_row;
  screen_cols = w.ws_col;
  screen_resized= 1;
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

////////////////////////////////////////
// - keyboard

// bytes buffered
static int _key_b= 0; 
keycode _peekedkey= -1;

int haskey() {
  if (_peekedkey!=-1) return 1;
  if (_key_b>0) return 1;

  struct timeval tv = { 0L, 0L };
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(0, &fds);
  int r= select(1, &fds, NULL, NULL, &tv);
  return r;
}

long _prevkeyms= 0, _lastkeyms= 0;
keycode _prevkey= -1, _lastkey= -1;

keycode _key();

// Wait and read a key stroke.
// Returns:
//   - ASCII
//   - UTF-8 bytes
//   - CTRL+'A' .. CTRL+'Z'
//   or if >= 256:
//     - META+'A'
//     - FUNC+1
//     - (CTRL/SHIFT/META)+UP/DOWN/LEFT/RIGHT
//   - TAB S_TAB RETURN DEL BACKSPACE
// Note: NOT thread-safe
// (user-faceing wrapping get/wait key
//  because _key has many returns)
keycode key() {
  _prevkeyms= _lastkeyms;
  _prevkey= _lastkey;
  if (_peekedkey!=-1) {
    keycode k= _peekedkey;
    _peekedkey= -1;
    return _lastkey= k;
  }

  // wait for one
  _lastkey= _key();
  _lastkeyms= mstime();
  return _lastkey;
}

// internal wait key or get from buf
keycode _key() {
  // TODO: whatis good size?
  // TODO: test how much we can use?
  // estimate: rows*10=???
  static char buf[2048]= {0};

  // get next key
  if (_key_b>0) {
    memcpy(buf, &buf[1], _key_b--);
  } else {
    // read as many as available, as we're blocking=>at least one char
    // terminals return several character for one key (^[A == M-A  arrows etc)
    bzero(buf, sizeof(buf));
    _key_b= read(0, &buf[0], sizeof(buf)) - 1;
  }
  int k= buf[0];
  _lastkeyms= mstime();

  // TODO: how come I get ^J on RETURN?
  // but not in Play/keys.c ???

  //fprintf(stderr, " {%d} ", k);

  buf[_key_b+1]= 0;

//TDOO: BACKSPACE seems broken!
// or at least in Play/testkeys.c...

//printf("\n [k=%d] \n", k);

  // simple character, or error
  if (_key_b<=0) return _key_b<0? _key_b+1 : k;

  // fixing multi-char key-encodings
  // (these are triggered in seq!)
  if (k==ESC) k=toupper(_key())+META;
  if (k==META+'[' && _key_b) k=_key()+TERM;
  if (k==TERM+'3' && _key_b) _key(), k= DEL;
  if (k==TERM+'<' && _key_b) k= MOUSE;
  if (!_key_b) return k;

  // mouse!
  // - https://stackoverflow.com/questions/5966903/how-to-get-mousemove-and-mouseclick-in-bash/58390575#58390575
  assert(sizeof(k)>=4);
  if (k==MOUSE) {
    int but, r, c, len= 0;
    char m;
    // this is only correct if everything is in the buffer... :-(
    int n= sscanf(&buf[1], "%d;%d;%d%n%c", &but, &c, &r, &len, &m);
    if (n>0) {
      if (n==3) c='m'; // We get ^@0 byte?
      if (n==4) len++; //ok

      //fprintf(stderr, "\n\n[n=%d ==>%d TOUCH.%s %d , %d \"%s\" ]", n, len, m=='M'?"down":"up", r, c, &buf[1]);
      // TODO: be limited to 0-256...?
      k= (m=='M'?MOUSE_DOWN:MOUSE_UP)
        + (but<<16) + (r<<8) + c;

      if (but==64) k+= SCROLL_DOWN;
      if (but==65) k+= SCROLL_UP;
    } else len= 0;
    // eat up the parsed strokes
    while(len-->0) _key();
    //printf(" {%08x} ", k);
    return k;
  }

  // CTRL/SHIFT/ALT arrow keys
  if (k==TERM+'1' && _key_b==3 && buf[1]==';') {
    _key();
    char mod= _key();
    k= UP+ _key()-'A';
    switch(mod) {
    case '2': k+= SHIFT; break;
    case '5': k+= CTRL; break;
    case '3': k+= META; break;
    }
  }

  // function keys (special encoding)
  if (k==META+'O') k=_key()-'P'+1+FUNC;
  if (k==TERM+'1'&& _key_b==2) k=_key()-'0'+FUNC, _key(), k= k==5+FUNC?k:k-1;
  // (this only handlines FUNC
  // TODO: how about BRACKETED PASTE?
  // ......^[[200~~/GIT/RetroFit^[[201~
  if (k==TERM+'2'&& _key_b==2) k=_key()-'0'+9+FUNC, _key(), k= k>10+FUNC?k-1:k;

  return k;
}

// Non-blocking peek of what key would return.
// Returns next keycode
//         -1 if no next
keycode peekey() {
  if (_peekedkey!=-1) return _peekedkey;
  if (!haskey()) return -1;
  // there is key waiting, get and store
  return _peekedkey= key();
}

// waits for a key to be pressed MAX milliseonds
// Returns passed ms.
int keywait(int ms) {
  long startms= mstime();
  long passed= 0;
  // 5ms usleep => 1.5% cpu usage
  while(!haskey() && (passed= mstime()-startms)<=ms)
    usleep(5*1000);
  return passed;
}

keycode waitScrollEnd(keycode k) {
  keycode nk;
  while((nk= key()) && (nk & 0xff00ffff)==(k & 0xff00ffff));
  return nk;
}

// Returns true if they key is repeated
//
// (used to recognize and ignore further
//  scrolls when detecting flicking)
// It'll eat up same keys.
// After 200ms, which means no longer
// "flicking" it means sustastained
// scrolling (or key pressed long time)
// emit further events every 200 ms only.
//
// Normal keyrepeats after an intial delay
// of 480ms, and then every 50ms!
int keyRepeated() {
  // eat up repeated keys
  int n= 0;
  while(haskey() && peekey()==_prevkey) {
    key();
    n++;
  }
  return mstime()-_prevkeyms<200;
}

// Returns a static string describing KEY
// Note: next call may change previous returned value, NOT thread-safe
char* keystring(int k) {
  static char s[32];
  memset(s, 0, sizeof(s));

  if (0) ;
  else if (k==TAB) return "TAB";
  else if (k==RETURN) return  "RETURN";
  else if (k==ESC) return "ESC";
  else if (k<32) sprintf(s, "^%c", k+64);
  else if (k==BACKSPACE) return "BACKSPACE";
  else if (k==DEL) return "DEL";
  else if (k<127) s[0]= k;
  // 127? == delete key?
  else if (k==S_TAB) return "S_TAB";

  else if (k==UP) return "UP";
  else if (k==DOWN) return "DOWN";
  else if (k==RIGHT) return "RIGHT";
  else if (k==LEFT) return "LEFT";

  // TODO: simplify
  else if (k==SHIFT+UP) return "S_UP";
  else if (k==SHIFT+DOWN) return "S_DOWN";
  else if (k==SHIFT+RIGHT) return "S_RIGHT";
  else if (k==SHIFT+LEFT) return "S_LEFT";

  else if (k==CTRL+UP) return "^UP";
  else if (k==CTRL+DOWN) return "^DOWN";
  else if (k==CTRL+RIGHT) return "^RIGHT";
  else if (k==CTRL+LEFT) return "^LEFT";

  else if (k==META+UP) return "M-UP";
  else if (k==META+DOWN) return "M-DOWN";
  else if (k==META+RIGHT) return "M-RIGHT";
  else if (k==META+LEFT) return "M-LEFT";
  // END:TODO:

  else if (k & SCROLL_UP) return "SCROLL_UP";
  else if (k & SCROLL_DOWN) return "SCROLL_DOWN";

  else if (k & MOUSE || k & SCROLL) {
    int b= (k>>16) & 0xff, r= (k>>8) & 0xff, c= k & 0xff;
    sprintf(s, "%s_%s-B%d-R%d-C%d", k&SCROLL?"SCROLL":"MOUSE",
      k&SCROLL?(k&SCROLL_UP?"UP":"DOWN"): k&MOUSE_UP?"UP":"DOWN", b, r, c);
  }
  else if (k>=FUNC && k<=FUNC+12) sprintf(s, "F-%d", k-FUNC);
  else if (k>=META+' ') sprintf(s, "M-%c", k-META);
  else sprintf(s, "\\u%06x", k);
  return s;
}

void testkeys() {
  jio();
  fprintf(stderr, "\nCTRL-C ends\n");
  long t= mstime();
  int fastones= 0;
  for(int k= 0; k!=CTRL+'C'; k= key()) {
    long ms= mstime()-t;
    //long ms=mstime()-_prevkeyms;
    fprintf(stderr, "\n%4ld ms (fast %d) ------%s\t", ms, fastones, keystring(k));
    if (ms<10) fastones++;
    if (ms>=500) fastones= 0;
    
    t= mstime();

    if (0) {
      long n=0, ms;
      while(((ms= keywait(60)))<1) {
	if (!n) fprintf(stderr, "\n");
	if (((k= key())) == CTRL+'C') break;
	n++;
	fprintf(stderr, "\r%ld ", n);
      }
      if (n) fprintf(stderr, " fast keys\n");
    }

    if (0 && keyRepeated()) {
      long n= 0, tms= 0;
      fprintf(stderr, "\n");
      while (((tms= keyRepeated()))<40) {
	usleep(5*1000);
	if (n++ > 100) break;
	fprintf(stderr, "\r%ld ", n);
      }
      fprintf(stderr, " repeats \n");
    }
    
    if (0)
    while(!haskey()) {
      putchar('.'); fflush(stdout);
      usleep(30*1000);
    }
  }
}
