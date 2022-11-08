////////////////////////////////////////
// - keyboard

// 'A' CTRL+'A' META+'A' FUNC+7
//    0- 31  :  ^@ ^A...
//   32-126  :  ' ' .. 'Z{|}~'
//  127      :  BACKSPACE
// -- Hi bit set == META
//    1- 12  :  F1 .. F12
//   32- 64  : (special keys)
//   65- 96  :  M-A .. M-Z
// -- 256 (9th bit set) = TERM
//   TERM+'A': UP
//        'B': DOWN
//        'C': RIGHT
//        'D': LEFT
//   TERM+'Z': DEL
typedef enum keycode { RETURN='M'-64, TAB='I'-64, ESC=27, BACKSPACE=127,
  CTRL=-64, META=256, FUNC=META, ALT=META, TERM=1024,
  UP=TERM+'A', DOWN, RIGHT, LEFT, S_TAB=TERM+'Z', DEL=TERM+'3', SHIFT=1024,
  CTRLX=2048, /* not used */
  MOUSE_DOWN=0x01000000, MOUSE_UP=MOUSE_DOWN*2, MOUSE=MOUSE_DOWN+MOUSE_UP, SCROLL_DOWN=0x04000000, SCROLL_UP=0x08000000, SCROLL=SCROLL_DOWN+SCROLL_UP } keycode;

extern keycode _prevkey, _lastkey;
extern long _prevkeyms, lastkeyms;

int haskey();
keycode key();
keycode peekey();
int keywait(int ms);
keycode waitScrollEnd(keycode k);

char* keystring(int c);
void testkeys();

