/********************************************************************
*
* the screen implementation with using the Linux frame buffer support
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
*********************************************************************/

#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "fb.h"

#include <draw.h>
#include <memdraw.h>
#include <cursor.h>

Point mousexy(void);

static ulong displaychannel = 0;
static int displaydepth = 0;
static int isscreeninited = 0;
static uchar* screendata = 0;
static uchar* framebuffer = 0;
int ispointervisible = 0;

static void initcursor(uchar*, uchar*, int, int, int, int);
static void cleanpointer();
static void drawpointer(int, int);

static struct {
	char* clip;
	int   size;
} clipboard;

typedef struct ICursor ICursor;
struct ICursor {
	ulong	inuse;
	int     x;
	int     y;
	int	hotx;
	int	hoty;
	int	w;
	int	h;
	uchar*	set;
	uchar*	clr;
};
static ICursor icursor;

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static void
curslock(void)
{
	while(_tas(&icursor.inuse) != 0)
		osyield();
}

static void
cursunlock(void)
{
	icursor.inuse = 0;
}

void
drawcursor(Drawcursor* c)
{
	int h, bpl;
	Point mousepos = mousexy();
	if(suspend)
		return;
	drawqlock();	
	cleanpointer();
	if(c->data == nil)
		initcursor(0, 0, 0, 0, 0, 0);
	else {
		h = (c->maxy-c->miny)/2;	/* image, then mask */
		bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 1);
		initcursor(c->data, c->data + h*bpl, h, bpl*8, c->hotx, c->hoty);
	}
	setpointer(mousepos.x, mousepos.y);
	drawqunlock();
}

uchar* 
attachscreen(Rectangle *rectangle, ulong *channel, int *depth, int *width, int *softscreen)
{
	if(!isscreeninited) {
		if (!fb_init(&Xsize, &Ysize, &displaydepth, &displaychannel)) {
			fprint(2, "cannot init framebuffer\n");
			return 0;
		}
		framebuffer = fb_get();
		if(!framebuffer) {
			fprint(2, "cannot get framebuffer\n");
			return 0;
		}
		screendata = malloc(Xsize * Ysize *(displaydepth / 8));
		if(!screendata) {
			fprint(2, "cannot allocate screen buffer\n");
			return 0;
		}
		initcursor(0, 0, 0, 0, 0, 0);
	}
	Xsize &= ~0x3;	/* ensure multiple of 4 */
	rectangle->min.x = 0;
	rectangle->min.y = 0;
	rectangle->max.x = Xsize;
	rectangle->max.y = Ysize;

	*channel = displaychannel;
	*depth = displaydepth;

	*width = (Xsize / 4) * (*depth / 8);
	*softscreen = 1;
	if(!isscreeninited)
		isscreeninited = 1;
	return screendata;
}

void 
detachscreen()
{
	free(icursor.set);
	icursor.set = 0;
	free(icursor.clr);
	icursor.clr = 0;
	free(screendata);
	screendata = 0;
	fb_release(framebuffer);
	framebuffer = 0;
	fb_deinit();
        if(clipboard.clip) {
                free(clipboard.clip);
                clipboard.clip = 0;
                clipboard.size = 0;
        }
}

#define max(a,b)(((a)>(b))?(a):(b))
#define min(a,b)(((a)<(b))?(a):(b))

void 
flushmemscreen(Rectangle rectangle)
{
	if(suspend)
		return;
	if(!framebuffer || !screendata)
		return;
	int depth = displaydepth / 8;
	int bpl = Xsize * depth;
	int i;
	uchar* framebufpos = framebuffer;
	uchar* screendatapos = screendata;
	int width;

	if(rectangle.min.x < 0)
		rectangle.min.x = 0;
	if(rectangle.min.y < 0)
		rectangle.min.y = 0;
	if(rectangle.max.x > Xsize)
		rectangle.max.x = Xsize;
	if(rectangle.max.y > Ysize)
		rectangle.max.y = Ysize;
  
	if((rectangle.max.x < rectangle.min.x) || (rectangle.max.y < rectangle.min.y))
		return;

	framebufpos += rectangle.min.y * bpl + rectangle.min.x * depth;
	screendatapos += rectangle.min.y * bpl + rectangle.min.x * depth;
	width = (rectangle.max.x - rectangle.min.x) * depth;
	for(i = rectangle.min.y; i < rectangle.max.y; i++) {
		memcpy(framebufpos, screendatapos, width);
		framebufpos += bpl;
		screendatapos += bpl;
	}

	if(!ispointervisible)
		return;
	Point mousepos = mousexy();
	curslock();
	if((max(rectangle.min.x, mousepos.x + icursor.hotx) > min(rectangle.max.x, mousepos.x + icursor.hotx + icursor.w)) ||
	   (max(rectangle.min.y, mousepos.y + icursor.hoty) > min(rectangle.max.y, mousepos.y + icursor.hoty + icursor.h))) {
		cursunlock();
		return;
	}
	cursunlock();
	drawpointer(mousepos.x, mousepos.y);
}
uchar DefaultPointer [] = {
	0x00, 0x00, /* 0000000000000000 */
	0x7F, 0xFE, /* 0111111111111110 */
	0x7F, 0xFC, /* 0111111111111100 */
	0x7F, 0xF8, /* 0111111111111000 */
	0x7F, 0xF0, /* 0111111111110000 */
	0x7F, 0xE0, /* 0111111111100000 */
	0x7F, 0xC0, /* 0111111111000000 */
	0x7F, 0xE0, /* 0111111111100000 */
	0x7F, 0xF0, /* 0111111111110000 */
	0x7F, 0xF8, /* 0111111111111000 */
	0x7C, 0xFC, /* 0111110011111100 */
	0x78, 0x7E, /* 0111100001111110 */
	0x70, 0x3C, /* 0111000000111100 */
	0x60, 0x18, /* 0110000000011000 */
	0x40, 0x00, /* 0100000000000000 */
	0x00, 0x00, /* 0000000000000000 */
};

uchar DefaultPointerMask [] = {
	0xFF, 0xFF, /* 1111111111111111 */
	0xFF, 0xFF, /* 1111111111111111 */
	0xFF, 0xFE, /* 1111111111111110 */
	0xFF, 0xFC, /* 1111111111111100 */
	0xFF, 0xF8, /* 1111111111111000 */
	0xFF, 0xF0, /* 1111111111110000 */
	0xFF, 0xE0, /* 1111111111100000 */
	0xFF, 0xF0, /* 1111111111110000 */
	0xFF, 0xF8, /* 1111111111111000 */
	0xFF, 0xFC, /* 1111111111111100 */
	0xFF, 0xFE, /* 1111111111111110 */
	0xFC, 0xFF, /* 1111110011111111 */
	0xF8, 0x7E, /* 1111100001111110 */
	0xF0, 0x3C, /* 1111000000111100 */
	0xE0, 0x18, /* 1110000000011000 */
	0xC0, 0x00, /* 1100000000000000 */
};

static const int DefaultPointerHeight = 16; 
static const int DefaultPointerWidth = 16; 

static void 
initcursor(uchar* clr, uchar* set, int height, int width, int hotx, int hoty)
{
	uchar i, j, k;
	uchar *ps, *pc, *bc, *bs, cb, sb;
	int depth = displaydepth / 8;
	int bpl;

	if(!clr) {
		set = DefaultPointer;
		clr = DefaultPointerMask; 
		height = DefaultPointerWidth;
		width = DefaultPointerHeight;
		hotx = 0;
		hoty = 0;
	}
	bpl = width / 8;
	curslock();
	if(icursor.set)
		free(icursor.set);
	icursor.set = malloc(height * width * depth);
	if(!icursor.set) {
		fprint(2, "cannot allocate cursor");
		return;
	}
	memset(icursor.set, 0xFF, height * width * depth);

	if(icursor.clr)
		free(icursor.clr);
	icursor.clr = malloc(height * width * depth);
	if(!icursor.clr) {
		fprint(2, "cannot allocate cursor mask");
		return;
	}
	memset(icursor.clr, 0x0, height * width * depth);

	ps = icursor.set;
	pc = icursor.clr;
	bs = set;
	bc = clr;
	for(i = 0; i < height; i++) {
		for(j = 0; j < bpl; j++) {
			sb = revbyte(bs[j]);
			cb = revbyte(bs[j] | bc[j]);
			for(k = 0; k < 8; k++) {
				if(sb & (1<<k))
					memset(ps, 0x0, depth);
				if(cb & (1<<k))
					memset(pc, 0xFF, depth);
				ps += depth;
				pc += depth;
			}
		}
		bs += bpl;
		bc += bpl;
	}
 	icursor.h = height;
	icursor.w = width;
	icursor.hotx = hotx;
	icursor.hoty = hoty;
	cursunlock();
}

void 
setpointer(int x, int y)
{
	if(suspend)
		return;
	if(!ispointervisible)
		return;
	cleanpointer();
	if(x < 0)
		x = 0;
	else if(x > Xsize)
		x = Xsize;
	if(y < 0)
		y = 0;
	else if(y > Ysize)
		y = Ysize;
	curslock();
	icursor.x = x;
	icursor.y = y;
	cursunlock();
	drawpointer(icursor.x, icursor.y);
}

static void 
cleanpointer()
{
	if(!framebuffer || !screendata)
		return;
	int depth = displaydepth / 8;
	int bpl = Xsize * depth;
	int i;
	int x = icursor.x + icursor.hotx;
	if(x < 0)
		x = 0;
	int y =  icursor.y + icursor.hoty;
	if(y < 0)
		y = 0;
	uchar* framebufpos = framebuffer + y * bpl + x * depth;
	uchar* screendatapos = screendata + y * bpl + x * depth; 
	int width = icursor.w * depth;
	int height = ((y + icursor.h) < Ysize) ? icursor.h : Ysize - y;
	for(i = 0; i < height; i++) {
		memcpy(framebufpos, screendatapos, width);
		framebufpos += bpl;
		screendatapos += bpl;
	}
}

static void 
drawpointer(int x, int y)
{
	uchar i = 0, j, k, J = 0;
	uchar depth = displaydepth / 8;
 	x += icursor.hotx;
   	y += icursor.hoty;
	if(x < 0){
		J = -x * depth;
		x = 0;
	}
	if(y < 0){
		i = -y;
		y = 0;
	}
	uchar* curpos = framebuffer + y * Xsize * depth + x * depth;
	curslock();
	int width = ((x + icursor.w) < Xsize) ? icursor.w : Xsize - x;
	int height = ((y + icursor.h) < Ysize) ? icursor.h : Ysize - y;
	for(; i < height; i++, curpos += Xsize * depth)
		for(j = J, k = 0; j < width * depth; j++, k++)
			if(icursor.clr[i * depth * icursor.w + j])
				curpos[k] = icursor.set[i * depth * icursor.w + j];
	cursunlock();
}

char*
clipread(void)
{
	return clipboard.clip;
}

int
clipwrite(char *buf)
{
	int size = strlen(buf);
	if((size > clipboard.size) && realloc(clipboard.clip, size)) {
		clipboard.size = size;
		strncpy(clipboard.clip, buf, clipboard.size);
	}
	return 0;
}

int
segflush(void *va, ulong len)
{
	return 0;
}


