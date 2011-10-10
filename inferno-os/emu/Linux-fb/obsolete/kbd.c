/**********************************************
*
* PC keyboard support for Inferno OS
* based on sources from Inferno distribution
* Author: Alexander Sychev <santucco@gmail.com>
*
***********************************************/

#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include        "keyboard.h"
#include        <sys/kd.h>
#include        <termios.h>

/*
 * The codes at 0x79 and 0x81 are produed by the PFU Happy Hacking keyboard.
 * A 'standard' keyboard doesn't produce anything above 0x58.
 */
Rune kbtab[] = 
        {
                [0x00]	No,	0x1b,	'1',	'2',	'3',	'4',	'5',	'6',
                [0x08]	'7',	'8',	'9',	'0',	'-',	'=',	'\b',	'\t',
                [0x10]	'q',	'w',	'e',	'r',	't',	'y',	'u',	'i',
                [0x18]	'o',	'p',	'[',	']',	'\n',	LCtrl,	'a',	's',
                [0x20]	'd',	'f',	'g',	'h',	'j',	'k',	'l',	';',
                [0x28]	'\'',	'`',	Shift,	'\\',	'z',	'x',	'c',	'v',
                [0x30]	'b',	'n',	'm',	',',	'.',	'/',	Shift,	'*',
                [0x38]	Latin,	' ',	RCtrl,	KF|1,	KF|2,	KF|3,	KF|4,	KF|5,
                [0x40]	KF|6,	KF|7,	KF|8,	KF|9,	KF|10,	Num,	Scroll,	'7',
                [0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
                [0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	KF|11,
                [0x58]	KF|12,	No,	No,	No,	No,	No,	No,	No,
                [0x60]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x68]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x70]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x78]	No,	View,	No,	Up,	No,	No,	No,	No,
        };

Rune kbtabshift[] =
        {
                [0x00]	No,	0x1b,	'!',	'@',	'#',	'$',	'%',	'^',
                [0x08]	'&',	'*',	'(',	')',	'_',	'+',	'\b',	'\t',
                [0x10]	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',	'I',
                [0x18]	'O',	'P',	'{',	'}',	'\n',	LCtrl,	'A',	'S',
                [0x20]	'D',	'F',	'G',	'H',	'J',	'K',	'L',	':',
                [0x28]	'"',	'~',	Shift,	'|',	'Z',	'X',	'C',	'V',
                [0x30]	'B',	'N',	'M',	'<',	'>',	'?',	Shift,	'*',
                [0x38]	Latin,	' ',	RCtrl,	KF|1,	KF|2,	KF|3,	KF|4,	KF|5,
                [0x40]	KF|6,	KF|7,	KF|8,	KF|9,	KF|10,	Num,	Scroll,	'7',
                [0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
                [0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	KF|11,
                [0x58]	KF|12,	No,	No,	No,	No,	No,	No,	No,
                [0x60]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x68]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x70]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x78]	No,	Up,	No,	Up,	No,	No,	No,	No,
        };

Rune kbtabesc1[] =
        {
                [0x00]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x08]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x10]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x18]	No,	No,	No,	No,	'\n',	LCtrl,	No,	No,
                [0x20]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x28]	No,	No,	Shift,	No,	No,	No,	No,	No,
                [0x30]	No,	No,	No,	No,	No,	'/',	No,	Print,
                [0x38]	Latin,	No,	No,	No,	No,	No,	No,	No,
                [0x40]	No,	No,	No,	No,	No,	No,	Break,	Home,
                [0x48]	Up,	Pgup,	No,	Left,	No,	Right,	No,	End,
                [0x50]	Down,	Pgdown,	Ins,	Del,	No,	No,	No,	No,
                [0x58]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x60]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x68]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x70]	No,	No,	No,	No,	No,	No,	No,	No,
                [0x78]	No,	Up,	No,	No,	No,	No,	No,	No,
        };


int mouseshifted;
extern int tty;

/*
 *  keyboard procedure
 */
static void 
keyboardproc (void* dummy)
{
        int s, c, i;
        static int esc1, esc2;
        static int alt, caps, ctl, num, shift;
        static int collecting, nk;
        static Rune kc[5];
        int keyup;
        int count;
        uchar buffer[100] = {0};
        while (((count = read(tty, buffer, sizeof(buffer))) >= 0) || 
                (errno == EINTR) )
                for (i = 0; i < count; i++) {
                        c = buffer [i];
                        /*
                         *  e0's is the first of a 2 character sequence
                         */
                        if(c == 0xe0) {
                                esc1 = 1;
                                continue;
                        }else if(c == 0xe1) {
                                esc2 = 2;
                                continue;
                        }

                        keyup = c&0x80;
                        c &= 0x7f;
                        if(c > sizeof kbtab) {
                                c |= keyup;
                                if(c != 0xFF)	/* these come fairly often: CAPSLOCK U Y */
                                        print("unknown key %ux\n", c);
                                continue;
                        }

                        if(esc1) {
                                c = kbtabesc1[c];
                                esc1 = 0;
                        } else if(esc2) {
                                esc2--;
                                continue;
                        } else if(shift)
                                c = kbtabshift[c];
                        else
                                c = kbtab[c];

                        if(caps && c<='z' && c>='a')
                                c += 'A' - 'a';

                        /*
                         *  keyup only important for shifts
                         */
                        if(keyup) {
                                switch(c) {
                                case Latin:
                                        alt = 0;
                                        break;
                                case Shift:
                                        shift = 0;
                                        mouseshifted = 0;
                                        break;
                                case LCtrl:
                                case RCtrl:
                                        ctl = 0;
                                        break;
                                }
                                continue;
                        }

                        /*
                         *  normal character
                         */
                        if(!(c & Spec)) {
                                if(ctl)
                                        c &= 0x1f;
                                if(!collecting) {
                                        gkbdputc(gkbdq, c);
                                        continue;
                                }
                                kc[nk++] = c;
                                c = latin1(kc, nk);
                                if(c < -1)	/* need more keystrokes */
                                        continue;
                                if(c != -1)	/* valid sequence */
                                        gkbdputc(gkbdq, c);
                                else	/* dump characters */
                                        for(i=0; i<nk; i++)
                                                gkbdputc(gkbdq, kc[i]);
                                nk = 0;
                                collecting = 0;
                                continue;
                        } else {
                                switch(c) {
                                case Caps:
                                        caps ^= 1;
                                        continue;
                                case Num:
                                        num ^= 1;
                                        continue;
                                case Shift:
                                        shift = 1;
                                        mouseshifted = 1;
                                        continue;
                                case Latin:
                                        alt = 1;
                                        /*
                                         * VMware uses Ctl-Alt as the key combination
                                         * to make the VM give up keyboard and mouse focus.
                                         * This has the unfortunate side effect that when you
                                         * come back into focus, Plan 9 thinks you want to type
                                         * a compose sequence (you just typed alt). 
                                         *
                                         * As a clumsy hack around this, we look for ctl-alt
                                         * and don't treat it as the start of a compose sequence.
                                         */
                                        if(!ctl) {
                                                collecting = 1;
                                                nk = 0;
                                        }
                                        continue;
                                case LCtrl:
                                case RCtrl:
                                        collecting = 0;
                                        nk = 0;
                                        ctl = 1;
                                        continue;
                                case Del:
                                        if(ctl&&alt) {
						fb_deinit();
                                                cleanexit(0);
					}
                                }
                        }
                        gkbdputc(gkbdq, c);
                }
}

void 
kbdinit()
{
	if(ioctl(tty, KDSKBMODE, K_RAW) < 0) {
                fprint(2, "can't set keyboard mode: %s\n", stderr(errno));
                return;
        }

        struct termios term;

        tcgetattr(tty, &term);
        term.c_cc[VTIME] = 0;
        term.c_cc[VMIN] = 1;
        term.c_lflag &= ~(ICANON|ECHO|ISIG);
        term.c_iflag = 0;
        tcsetattr(tty, TCSAFLUSH, &term);
        if(kproc("keyboardproc", keyboardproc, 0, 0) < 0)
                fprint(2, "can't start keyboard procedure");
}

void 
kbdlink()
{}
