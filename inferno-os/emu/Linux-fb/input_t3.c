/*******************************************
*
* Palm T3 input handling for Inferno OS
*
* Copyright (c) 2008 Salva Peiró
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
********************************************/

#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "keyboard.h"
#include <unistd.h>
#include "fb.h"
#include "input.h"

static int xabs[5], yabs[5];

static int b = 0;

const char scrdev[] = "/dev/input/event3";
const char keydev[] = "/dev/input/event1";

static void 
t3stylus(struct input_event* ev, int count)
{
	int i, touch=0;
	int x, y, p, dbl = 0;
	static int lastb;
	static struct timeval lastt;
	
	for (i=0; i < count; i++){
		if(0) fprint(2, "%d/%d [%d] ", i, count, ev[i].code);
		switch(ev[i].type){
			case EV_ABS:
				switch(ev[i].code){
				case ABS_X:
					x = Xsize - Xsize*(ev[i].value-xabs[1])/(float)(xabs[2]-xabs[1]);
					break;
				case ABS_Y:
					y = Ysize - Ysize*(ev[i].value-yabs[1])/(float)(yabs[2]-yabs[1]); 
					break;
				case ABS_PRESSURE:
					p = ev[i].value;
					break;
				}
				break;
			case EV_KEY:
				if (ev[i].value){
					touch=1;
					if(b==lastb && ev[i].time.tv_sec == lastt.tv_sec &&
						(ev[i].time.tv_usec-lastt.tv_usec) < DblTime)
						dbl = 1;
					lastb = b;
					lastt = ev[i].time;
					if(dbl)
						b = b | 1<<8;
				}
				break;
			case EV_SYN:
				if (i==3 && p>0)		// motion
					mousetrack (b, x, y, 0);
//				else if (i>3 && p>0 && touch)	// press
//					mousetrack(b, x, y, 0);
				else if (i>3 && p==0 && !touch)	// release
					mousetrack(0, 0, 0, 1);
				return;
		}
	}
}

static void 
t3keys(struct input_event* ev, int count)
{
	int i, key;
	static int t3_buttons[] = {
		No,
		[KEY_ENTER] = '\n',
		[KEY_F3] = No, [KEY_F4] = No,
		[KEY_F7] = PwrOff,
		[KEY_F9] = LCtrl, [KEY_F10] = Esc,
		[KEY_F11] = LShift, [KEY_F12] = RShift,
		[KEY_UP] = Up, [KEY_LEFT] = Left, [KEY_RIGHT] = Right, [KEY_DOWN] = Down,
	};

	for (i=0; i < count; i++){
		if (ev[i].type != EV_KEY)
			continue;

		if (ev[i].value)
			b = 1;
		else { 
			key = t3_buttons [ev[i].code];
			if (key != No)
				apm_blank(FB_NO_BLANK);
			switch(key) {
			case No:
				break;
			case RShift:
				b = 1;
				break;
			case LCtrl:
				b = 2;
				break;
			case LShift:
				b = 4;
				break;
			case PwrOff:  
				apm_suspend();
				break;
			default:
				gkbdputc(gkbdq, key);
				break;
			}
		}
	}
	return;
}

static int
t3config(Handler* handler)
{
	int res = input_init_vp(handler);
	if(res)
		return res;
	// corrections to touchscreen dimension
	ioctl(handlers->fd, EVIOCGABS(ABS_X), xabs);
	ioctl(handlers->fd, EVIOCGABS(ABS_Y), yabs);
	xabs[1] = xabs[1] + 143;
	xabs[2] = xabs[2] - 108;
	yabs[2] = yabs[2] + 35;

	return 0;
}

struct Handler handlers[] = {{-1, "/dev/input/event3", "scrProc", t3config, t3stylus, input_deinit},
			     {-1, "/dev/input/event1", "keysProc", input_init, t3keys, input_deinit},
			     {-1, 0, 0, 0, 0}};

