/*************************************************
*
* Generic keyboard input handling for Inferno OS
* based on sources form Inferno distribution
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
*
**************************************************/

#include "keyboard.h"

char *gkscanid = "emu_pc";

/*
 * The codes at 0x79 and 0x81 are produed by the PFU Happy Hacking keyboard.
 * A 'standard' keyboard doesn't produce anything above 0x58.
 */
Rune kbtab[] = {
	[0x00]	No,	Esc,	'1',	'2',	'3',	'4',	'5',	'6',
	[0x08]	'7',	'8',	'9',	'0',	'-',	'=',	'\b',	'\t',
	[0x10]	'q',	'w',	'e',	'r',	't',	'y',	'u',	'i',
	[0x18]	'o',	'p',	'[',	']',	'\n',	LCtrl,	'a',	's',
	[0x20]	'd',	'f',	'g',	'h',	'j',	'k',	'l',	';',
	[0x28]	'\'',	'`',	LShift,	'\\',	'z',	'x',	'c',	'v',
	[0x30]	'b',	'n',	'm',	',',	'.',	'/',	RShift,	'*',
	[0x38]	LAlt,	' ',	Caps,	KF|1,	KF|2,	KF|3,	KF|4,	KF|5,
	[0x40]	KF|6,	KF|7,	KF|8,	KF|9,	KF|10,	Num,	Scroll,	'7',
	[0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
	[0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	KF|11,
	[0x58]	KF|12,	No,	No,	No,	No,	No,	No,	No,
	[0x60]	'\n',	RCtrl,	'/',	No,	RAlt,	No,	Home,	Up,
	[0x68]	Pgup,	Left,	Right,	End,	Down,	Pgdown,	Ins,	Del,
	[0x70]	No,	No,	No,	No,	No,	No,	No,	No,
	[0x78]	No,	No,	No,	No,	No,	No,	No,	No,
};

Rune kbtabshift[] = {
	[0x00]	No,	Esc,	'!',	'@',	'#',	'$',	'%',	'^',
	[0x08]	'&',	'*',	'(',	')',	'_',	'+',	'\b',	'\t',
	[0x10]	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',	'I',
	[0x18]	'O',	'P',	'{',	'}',	'\n',	LCtrl,	'A',	'S',
	[0x20]	'D',	'F',	'G',	'H',	'J',	'K',	'L',	':',
	[0x28]	'"',	'~',	LShift,	'|',	'Z',	'X',	'C',	'V',
	[0x30]	'B',	'N',	'M',	'<',	'>',	'?',	RShift,	'*',
	[0x38]	LAlt,	' ',	Caps,	KF|1,	KF|2,	KF|3,	KF|4,	KF|5,
	[0x40]	KF|6,	KF|7,	KF|8,	KF|9,	KF|10,	Num,	Scroll,	'7',
	[0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
	[0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	KF|11,
	[0x58]	KF|12,	No,	No,	No,	No,	No,	No,	No,
	[0x60]	'\n',	RCtrl,	'/',	No,	RAlt,	No,	Home,	Up,
	[0x68]	Pgup,	Left,	Right,	End,	Down,	Pgdown,	Ins,	Del,
	[0x70]	No,	No,	No,	No,	No,	No,	No,	No,
	[0x78]	No,	No,	No,	No,	No,	No,	No,	No,
};


static void 
keyboard(struct input_event* ev, int count)
{
	int i, keydown;
	static int alt, caps, ctl, num, shift;
	Rune c;
	for (i=0; i < count; i++){
		if (ev[i].type != EV_KEY)
			continue;

		keydown = ev[i].value ? 1 : 0;
		if(gkscanq != nil){
			uchar ch = ev[i].code;
			if(!keydown)
				ch |= 0x80;
			qproduce(gkscanq, &ch, 1);
			return;
		}

		c = ev[i].code;
		switch(c) {
		case KEY_LEFTALT:
		case KEY_RIGHTALT:
			alt = keydown;
			continue;
		case KEY_LEFTSHIFT:
		case KEY_RIGHTSHIFT:
			shift = keydown;
			continue;
		case KEY_LEFTCTRL:
		case KEY_RIGHTCTRL:
			ctl = keydown;
			continue;
		case KEY_CAPSLOCK:
			if(keydown)
				caps ^= 1;
			continue;
		case KEY_NUMLOCK:
			if(keydown)
				num ^= 1;
			continue;
		case KEY_DELETE:
			if(ctl&&alt)
				cleanexit(0);
		default:
			if(!keydown)
				continue;
			if(c > sizeof(kbtab))
				continue;
			if(shift)
                                c = kbtabshift[c];
                        else
                                c = kbtab[c];
			if(caps){
				if(c<='z' && c>='a')
					c += 'A' - 'a';
				else if(c<='Z' && c>='A')
					c -= 'A' - 'a';
			}
			/*Alt-Fx are terminal switching sequences*/
			if(alt && (c & KF)){
				alt = c = 0;
				continue;
			}
		}

		if(ctl)
			c &= 0x9f;
		gkbdputc(gkbdq, c);
	}
}
