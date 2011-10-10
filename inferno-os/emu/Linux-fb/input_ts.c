/***************************************************
*
* Generic touchscreen input handling for Inferno OS
* (still in development)
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
****************************************************/


static void 
touchscreen(struct input_event* ev, int count)
{
	int i;
	int x, y, p, dbl = 0;
	static int b, lastb;
	static struct timeval lastt;
	
	for (i=0; i < count; i++){
		b &= ~(1<<8);
		switch(ev[i].type){
			case EV_ABS:
				switch(ev[i].code){
				case ABS_X:
					x = ev[i].value;
					break;
				case ABS_Y:
					y = ev[i].value;
					break;
				case ABS_PRESSURE:
					p = ev[i].value;
					break;
				}
				break;
			case EV_KEY:
				if(!ev[i].value)
					break;
				b = code2button(ev[i].code);
				touch = 1;
				if((b == lastb) && 
				   (ev[i].time.tv_sec == lastt.tv_sec) &&
				   ((ev[i].time.tv_usec - lastt.tv_usec) < DblTime))
					dbl = 1;
				lastb = b;
				lastt = ev[i].time;
				if(dbl)
					b |=  1<<8;
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
