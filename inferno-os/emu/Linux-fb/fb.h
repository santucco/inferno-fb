/*************************************************
*
* Linux frame buffer device support for Inferno OS
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
**************************************************/

#if !defined (__FB_H_included__)
#define __FB_H_included__

enum{
	FB_NO_BLANK,
	FB_BLANK,
};

extern uint suspend;

int fb_init(int* xres, int* yres, int* bpp, unsigned long* channel);
unsigned char* fb_get();
void fb_release(unsigned char* buffer);
int fb_blank(int mode);
void fb_deinit() ;

#endif // __FB_H_included__
