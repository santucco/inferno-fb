/*************************************************
*
* Generic mouse input handling for Inferno OS
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
**************************************************/

static void 
mouse(struct input_event* ev, int count)
{
	int i;
	int dx = 0, dy = 0, dbl = 0;
	
	static int b;
	static int lastb;
	static struct timeval lastt;
	
	for (i=0; i < count; i++){
		b &= ~(1<<8);
		switch(ev[i].type){
			case EV_REL:
				switch(ev[i].code){
				case REL_X:
					dx = ev[i].value;
					break;
				case REL_Y:
					dy = ev[i].value;
					break;
				}
				mousetrack(b, dx, dy, 1);
				break;
			case EV_KEY:
				if(ev[i].value){
					switch(ev[i].code){
					case BTN_LEFT:
						b |= 1;
						break;
					case BTN_MIDDLE:
						b |= 2;
						break;
					case BTN_RIGHT:
						b |= 4;
						break;
					}
					if((b == lastb) && 
					   (ev[i].time.tv_sec == lastt.tv_sec) &&
					   ((ev[i].time.tv_usec - lastt.tv_usec) < DblTime))
						dbl = 1;
					lastb = b;
					lastt = ev[i].time;
					if(dbl)
						b = b | 1<<8;
				}
				else
					switch(ev[i].code){
					case BTN_LEFT:
						b &= ~1;
						break;
					case BTN_MIDDLE:
						b &= ~2;
						break;
					case BTN_RIGHT:
						b &= ~4;
						break;
					}
				mousetrack(b, dx, dy, 1);
				break;
		}
	}
}
