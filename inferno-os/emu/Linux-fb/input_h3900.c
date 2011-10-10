/***************************************************
*
* Compaq/HP iPAQ h3970 input handling for Inferno OS
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
****************************************************/

#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "keyboard.h"
#include <unistd.h>


static struct {
	int screenfd;
	int keyfd;
	int button;
} touchscreen;

typedef struct {
	uchar pressed;
	uchar reserved1;
	Rune  x;
	Rune  y;
	Rune  reserved2; 
} Packet __attribute__ ((packed));

static void 
touchscreen_stylus(Packet* packet, int count)
{
	int i;
	for(i = 0; i < count; i++) {
		if(packet [ i ].pressed) 
			mousetrack(touchscreen.button, packet [ i ].x, packet [ i ].y, 0); 
		else
			mousetrack(0, 0, 0, 1); 
	}
	return;
}

static void 
touchscreen_keys(uchar* packet, int size)
{
	int i;
	static int buttons[] = {0, Esc, LCtrl, LShift, RShift, RCtrl, Right, Down, Up, Left, Middle, PwrOff};
	for(i = 0; i < size; i++) {
		int released = packet[i] & 0x80;
		int key = buttons [packet[i] & 0x0F];
		if(released)
			touchscreen.button = 1;
		else {
			switch(key) {
			case LCtrl:
			case RCtrl:
				touchscreen.button = 2;
				break;
			case LShift:
			case RShift:
				touchscreen.button = 4;
				break;
			case PwrOff:  
				apm_suspend();
				break;
			default:
				touchscreen.button = 1;
				gkbdputc(gkbdq, key);
				break;
			}
		}
	}
	return;
}

static void 
touchscreenProc(void* dummy)
{
	for(;;) {
		Packet packets[10];
		int count = read(touchscreen.screenfd, packets, sizeof(packets));
		if(count > 0)
			touchscreen_stylus(packets, count / sizeof(Packet));      
	}
}


static void 
keysProc(void* dummy)
{
	for(;;) {
		uchar buffer[32];  
		int count = read(touchscreen.keyfd, buffer, sizeof(buffer));
		if(count > 0)
			touchscreen_keys(buffer, count);
	}
}

static void 
touchscreeninit(void)
{
	if(((touchscreen.screenfd = open("/dev/touchscreen/0", O_RDONLY)) < 0)) {
		fprint(2, "can't open touchscreen device: %s\n", strerror(errno));
		return;
	}
    
	if((touchscreen.keyfd = open("/dev/touchscreen/key", O_RDONLY)) < 0) {
		fprint(2, "can't open touchescreen keys device: %s\n", strerror(errno));
		close(touchscreen.screenfd);
		return;
	}

	touchscreen.button = 1;

	kproc("touchscreenProc", touchscreenProc, nil, 0);
	
	kproc("keysProc", keysProc, nil, 0);

}

void 
h3900_tslink(void)
{
	touchscreeninit();
}

