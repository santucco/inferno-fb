/******************************************************
*
* Linux frame buffer device support for Inferno OS
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev. 
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
*******************************************************/
#include "dat.h"
#include "fns.h"
#include <draw.h>

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/vt.h>
#include <sys/kd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "fb.h"

static int fbfd = -1;
static int fbsize = 0;
static struct fb_var_screeninfo origvsi;
static struct fb_var_screeninfo curvsi;
static struct fb_cmap origcmap;
static struct fb_cmap curcmap;
static int tty0 = -1;
int tty = -1;
static int old_stdin = -1;
static unsigned short prevtty = -1;
static unsigned short activetty = -1;
static unsigned short oldfb = -1;
static int oldtermmode = KD_TEXT;
static struct vt_mode oldvtmode;
struct termios oldtermattr;
static int vt_init();
static void vt_deinit();

static int enum_modes(int xres, int yres);
static int get_params(int* x, int* y, int* bpp, unsigned long* channel);
static int set_cmap();

uint suspend = 0;

int fb_init(int* xres, int* yres, int* bpp, unsigned long* channel)
{
	unsigned char* res = 0;
	int size = 0;
        struct fb_fix_screeninfo fsi;

	char* fbdev = getenv("FRAMEBUFFER");

	if( !fbdev || !*fbdev )
		fbdev = "/dev/fb0";
	
        fbfd = open(fbdev, O_RDWR);

        if(fbfd < 0){
                fprint(2, "framebuffer: cannot open a file %s: %s\n", fbdev, strerror(errno));
                return 0;
        }


        if(ioctl(fbfd, FBIOGET_FSCREENINFO, &fsi)){
                fprint(2, "framebuffer: cannot get fixed screen info: %s\n", strerror(errno));
                return -1;
        }

        fbsize = fsi.smem_len;

        if(ioctl(fbfd, FBIOBLANK, 0)){
                close(fbfd);
                fprint(2, "framebuffer: cannot blank the framebuffer:  %s\n", strerror(errno));
                return 0;
        }

        if(ioctl(fbfd, FBIOGET_VSCREENINFO, &origvsi)){
                close(fbfd);
                fprint(2, "framebuffer: cannot get the variable screen info: %s\n", strerror(errno));
                return 0;
        }

        curvsi = origvsi;
	curvsi.activate = FB_ACTIVATE_NOW;
	if(ioctl(fbfd, FBIOPUT_VSCREENINFO, &curvsi)){
		fprint(2, "framebuffer: cannot put the new variable screen info: %s\n", strerror(errno));
		return 0;
	}

        if(vt_init()){
                close(fbfd);
                return 0;
        }
#if defined(OLD_KBD_SUPPORT) 	
	kbdinit(); 
#endif

	if(!get_params(xres, yres, bpp, channel))  {
		fb_deinit();
		return 0;
	}
	memset(&origcmap, 0, sizeof(origcmap));
	memset(&curcmap, 0, sizeof(curcmap));
	if(!set_cmap()){
		fb_deinit();
		return 0;
	}

	return 1;
}

unsigned char* fb_get()
{
        unsigned char* res = mmap(0, fbsize, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
        if(res == MAP_FAILED){
                fprint(2, "framebuffer: cannot map the framebuffer:  %s\n", strerror(errno));
                return 0;
        }

        return res;
}

void 
fb_release(unsigned char* buf)
{
        if(munmap(buf,fbsize))
                fprint(2, "framebuffer: cannot release the framebuffer:  %s\n", strerror(errno));
}

int 
fb_blank(int mode)
{
	int fbmode;
	switch(mode){
	case FB_NO_BLANK: fbmode = FB_BLANK_UNBLANK; break;
	case FB_BLANK: fbmode = FB_BLANK_POWERDOWN; break;
	default: return -1;
	}
	return ioctl(fbfd, FBIOBLANK, fbmode);
}

void 
fb_deinit() 
{
        if(fbfd < 0)
                return;
        vt_deinit();
	if(origcmap.len)
		ioctl(fbfd, FBIOPUTCMAP, &origcmap);
	if(origcmap.red)
		free(origcmap.red);
	if(origcmap.green)
		free(origcmap.green);
	if(origcmap.blue)
		free(origcmap.blue);
	if(curcmap.red)
		free(curcmap.red);
	if(curcmap.green)
		free(curcmap.green);
	if(curcmap.blue)
		free(curcmap.blue);
        ioctl(fbfd, FBIOPUT_VSCREENINFO, &origvsi);
        close(fbfd);
}

static char* 
get_line(char** curpos, char* bufend)
{
        char* res = *curpos;
        while(*curpos < bufend){
                if(**curpos == '\n'){
                        **curpos = 0;
                        *curpos += 1;
                        return res;
                }
                if(**curpos == 0){
                        *curpos += 1;
                        return res;
                }
                *curpos += 1;
        }
        return 0;
}

static char* 
parse_mode(char* beginofmode, char* bufend)
{
        char* beginofline = 0;
        int tmp = 0;
        char value[10] = {0};
        int geometryfound = 0;
        int timingsfound = 0;
	int rgbafound = 0;
        do {
                beginofline = get_line(&beginofmode, bufend);
                if(!beginofline)
                        return beginofmode;

                if(sscanf(beginofline, 
                          " geometry %d %d %d %d %d", 
                          &curvsi.xres, 
                          &curvsi.yres, 
                          &tmp, 
                          &tmp, 
                          &curvsi.bits_per_pixel) == 5){
                        geometryfound = 1;
                        curvsi.sync = 0;
                        curvsi.xoffset = 0;
                        curvsi.yoffset = 0;
                        curvsi.xres_virtual = curvsi.xres;
                        curvsi.yres_virtual = curvsi.yres;
                        continue;
                }
                if(sscanf(beginofline,
                          " timings %d %d %d %d %d %d %d",
                          &curvsi.pixclock,
                          &curvsi.left_margin,
                          &curvsi.right_margin,
                          &curvsi.upper_margin,
                          &curvsi.lower_margin,
                          &curvsi.hsync_len,
                          &curvsi.vsync_len) == 7){
                        timingsfound = 1;
                        continue;
                }
                if((sscanf(beginofline,
                           " hsync %6s",
                           value) == 1) &&
                   !strcmp(value, "high")){
                        curvsi.sync |= FB_SYNC_HOR_HIGH_ACT;
                        continue;
                }
                if((sscanf(beginofline,
                           " vsync %6s",
                           value) == 1) &&
                   !strcmp(value, "high")){
                        curvsi.sync |= FB_SYNC_VERT_HIGH_ACT;
                        continue;
                }
                if((sscanf(beginofline,
                           " csync %6s",
                           value) == 1) &&
                   !strcmp(value, "high")){
                        curvsi.sync |= FB_SYNC_COMP_HIGH_ACT ;
                        continue;
                }
                if((sscanf(beginofline,
                           " extsync %6s",
                           value) == 1) &&
                   !strcmp(value, "true")){
                        curvsi.sync |= FB_SYNC_EXT;
                        continue;
                }
                if((sscanf(beginofline,
                           " laced %6s",
                           value) == 1) &&
                   !strcmp(value, "true")){
                        curvsi.sync |= FB_VMODE_INTERLACED;
                        continue;
                }
                if((sscanf(beginofline,
                           " double %6s",
                           value) == 1) &&
                   !strcmp(value, "true")){
                        curvsi.sync |= FB_VMODE_DOUBLE;
                        continue;
                }
                if(sscanf(beginofline,
			  " rgba %d/%d,%d/%d,%d/%d,%d/%d",
			  &curvsi.red.length,
			  &curvsi.red.offset,
			  &curvsi.green.length,
			  &curvsi.green.offset,
			  &curvsi.blue.length,
			  &curvsi.blue.offset,
			  &curvsi.transp.length,
			  &curvsi.transp.offset) == 8){
			rgbafound = 1;
                        continue;
                }

        }
        while(!strstr(beginofline, "endmode"));
	if(!rgbafound) {
		curvsi.transp.length = curvsi.transp.offset = 0;
		curvsi.red.msb_right = curvsi.green.msb_right = 0;
		curvsi.blue.msb_right = curvsi.transp.msb_right = 0;
		switch(curvsi.bits_per_pixel){
		case 8:
			curvsi.red.offset = 0;
			curvsi.red.length = 8;
			curvsi.green.offset = 0;
			curvsi.green.length = 8;
			curvsi.blue.offset = 0;
			curvsi.blue.length = 8;
		case 16:
			curvsi.red.offset = 11;
			curvsi.red.length = 5;
			curvsi.green.offset = 5;
			curvsi.green.length = 6;
			curvsi.blue.offset = 0;
			curvsi.blue.length = 5;
			break;
			
		case 24:
			curvsi.red.offset = 16;
			curvsi.red.length = 8;
			curvsi.green.offset = 8;
			curvsi.green.length = 8;
			curvsi.blue.offset = 0;
			curvsi.blue.length = 8;
			break;
			
		case 32:
			curvsi.red.offset = 16;
			curvsi.red.length = 8;
			curvsi.green.offset = 8;
			curvsi.green.length = 8;
			curvsi.blue.offset = 0;
			curvsi.blue.length = 8;
			break;
		}
		
	}
        if(geometryfound && timingsfound)
                return 0;
        return beginofmode;
}

static int 
enum_modes(int xres, int yres)
{
        int fd = -1;
        struct stat status;
        char* modesfile = MAP_FAILED;
        char* beginofline = 0;
        char* curpos = 0;
        char* bufend = 0;
	int bestx = 0;
	int besty = 0;
	int bestbpp = 0;;
	char* bestmode;
	char* beginofmode = 0;
        fd = open("/etc/fb.modes", O_RDONLY);
        if(fd < 0){
                fprint(2, "framebuffer: cannot open the file /etc/fb.modes: %s\n", strerror(errno));
                return 0;
        }

        if(fstat(fd, &status) < 0){
                fprint(2, "framebuffer: cannot stat the file /etc/fb.modes: %s\n", strerror(errno));
                close(fd);
                return 0;
        }

        modesfile = mmap(0, status.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        if(modesfile == MAP_FAILED){
                fprint(2, "framebuffer: cannot map /etc/fb.modes: %s\n", strerror(errno));
                close(fd);
                return 0;
        }
        modesfile[status.st_size] = 0;
        curpos = modesfile;
        bufend = modesfile + status.st_size;
	bestmode = bufend;
        do {
                char mode[50] = {0};
                beginofline = get_line(&curpos, bufend);
                if(!beginofline)
                        break;
                if(sscanf(beginofline, "mode \"%50[^\"]\"", mode) == 1){
			beginofmode = beginofline;
                        do {
                                int res = -1;
                                int x = 0;
                                int y = 0;
                                int bpp = 0;
                                int tmp = 0;
                                beginofline = get_line(&curpos, bufend);
                                if(!beginofline)
					break;
                                res = sscanf(beginofline, 
                                             " geometry %d %d %d %d %d", 
                                             &x, 
                                             &y, 
                                             &tmp, 
                                             &tmp, 
                                             &bpp);
                                if(res == 5){
					if((((x >= bestx) && (x <= xres)) || 
					    ((y >= besty) && (y <= yres))) &&
					   (bpp>=bestbpp)) {
						bestmode = beginofmode;
						bestx = x;
						besty = y;
						bestbpp = bpp;
					}
				}
	                                
			}
			while(!strstr(beginofline, "endmode"));
		}
	}
	while(beginofline);
	curpos = parse_mode(bestmode, bufend);
	munmap(modesfile, status.st_size);
	close(fd);
	
	if(curpos) {
		fprint(2, "framebuffer: mode %dx%d hasn't found in /etc/fb.modes\n", xres, yres);
		return 0;
	}
	return 1;
}


static int 
get_params(int* x, int* y, int* bpp, unsigned long* channel)
{
	if(enum_modes(*x, *y)) {
		if(ioctl(fbfd, FBIOPUT_VSCREENINFO, &curvsi)){
			fprint(2, "framebuffer: cannot put the new variable screen info: %s\n", strerror(errno));
			return 0;
		}
		if(ioctl(fbfd, FBIOGET_VSCREENINFO, &curvsi)){
			close(fbfd);
			fprint(2, "framebuffer: cannot get the variable screen info: %s\n", strerror(errno));
			return 0;
		}
	}
	else
		fprint(2,"framebuffer: cannot enumerate video modes - current framebuffer settings will be used\n");

	*x=curvsi.xres; 
	*y=curvsi.yres;
	*bpp=curvsi.bits_per_pixel;
	
	switch (curvsi.bits_per_pixel) {
	case 8:
		*channel=CMAP8;
		break;
	case 16:
	case 24:
		if(curvsi.transp.length)
			*channel=CHAN4(CAlpha,curvsi.transp.length,
				       CRed,curvsi.red.length,
				       CGreen,curvsi.green.length,
				       CBlue,curvsi.blue.length);
		
		else
			*channel=CHAN3(CRed,curvsi.red.length,
				       CGreen,curvsi.green.length,
				       CBlue,curvsi.blue.length);
		break;
	case 32:
		if(curvsi.transp.length)
			*channel=CHAN4(CAlpha,curvsi.transp.length,
				       CRed,curvsi.red.length,
				       CGreen,curvsi.green.length,
				       CBlue,curvsi.blue.length);
		else
			*channel=CHAN4(CIgnore,curvsi.bits_per_pixel - curvsi.red.length - curvsi.green.length - curvsi.blue.length,
				       CRed,curvsi.red.length,
				       CGreen,curvsi.green.length,
				       CBlue,curvsi.blue.length);
		break;
			
	}
	return 1;
}

static int 
set_cmap()
{
        struct fb_fix_screeninfo fsi;
	int i, red_size, green_size, blue_size;
	int r, g, b, cr, cg, cb, v, num, den, idx;

        if(ioctl(fbfd, FBIOGET_FSCREENINFO, &fsi)){
                fprint(2, "framebuffer: cannot get fixed screen info: %s\n", strerror(errno));
                return 0;
	
	}
	origcmap.start = 0;
	origcmap.len = 256;
	origcmap.red = malloc(256 * sizeof(unsigned short));
	origcmap.green = malloc(256 * sizeof(unsigned short));
	origcmap.blue = malloc(256 * sizeof(unsigned short));
	origcmap.transp = 0;
	memset(origcmap.red, 0, 256 * sizeof(unsigned short));
	memset(origcmap.green, 0, 256 * sizeof(unsigned short));
	memset(origcmap.blue, 0, 256 * sizeof(unsigned short));	
	if(ioctl(fbfd, FBIOGETCMAP, &origcmap)){
		fprint(2, "framebuffer: cannot get color map: %s\n", strerror(errno));
		origcmap.len = 0;
	}

	switch(fsi.visual) {
	case FB_VISUAL_PSEUDOCOLOR:
		curcmap.start = 0;
		curcmap.len = 256;
		curcmap.red = malloc(256 * sizeof(unsigned short));
		curcmap.green = malloc(256 * sizeof(unsigned short));
		curcmap.blue = malloc(256 * sizeof(unsigned short));
		curcmap.transp = 0;
		memset(curcmap.red, 0, 256 * sizeof(unsigned short));
		memset(curcmap.green, 0, 256 * sizeof(unsigned short));
		memset(curcmap.blue, 0, 256 * sizeof(unsigned short));
		for(r=0; r!=4; r++) {
			for(g = 0; g != 4; g++) {
				for(b = 0; b!=4; b++) {
					for(v = 0; v!=4; v++) {
						den=r;
						if(g > den)
							den=g;
						if(b > den)
							den=b;
						/* divide check -- pick grey shades */
						if(den==0)
							cr=cg=cb=v*17;
						else {
							num=17*(4*den+v);
							cr=r*num/den;
							cg=g*num/den;
							cb=b*num/den;
						}
						idx = r*64 + v*16 + ((g*4 + b + v - r) & 15);
						/* was idx = 255 - idx; */
						curcmap.red[idx] = cr*0x0101;
						curcmap.green[idx] = cg*0x0101;
						curcmap.blue[idx] = cb*0x0101;
					}
				}
			}
		}
		break;
	case FB_VISUAL_TRUECOLOR:
		return 1;
	case FB_VISUAL_DIRECTCOLOR:
		red_size = 1 << curvsi.red.length;
		green_size = 1 << curvsi.green.length;
		blue_size = 1 << curvsi.blue.length;	
		curcmap.start = 0;
		curcmap.len = red_size;
		if(curcmap.len < green_size)
			curcmap.len = green_size;		
		if(curcmap.len < blue_size)
			curcmap.len = blue_size;		
		curcmap.red = malloc(red_size * sizeof(unsigned short));
		curcmap.green = malloc(green_size * sizeof(unsigned short));
		curcmap.blue = malloc(blue_size * sizeof(unsigned short));
		curcmap.transp = 0;
		memset(curcmap.red, 0, red_size * sizeof(unsigned short));
		memset(curcmap.green, 0, green_size * sizeof(unsigned short));
		memset(curcmap.blue, 0, blue_size * sizeof(unsigned short));
		for(i=0; i < red_size; i++)
			curcmap.red[i] = i*256/red_size*0x0101;
		for(i=0; i < green_size; i++)
			curcmap.green[i] = i*256/green_size*0x0101;
		for(i=0; i < blue_size; i++)
			curcmap.blue[i] = i*256/blue_size*0x0101;
		break;
	default:
		fprint(2, "framebuffer: not supported visual type of the framebuffer\n" );
		return 0;
	}
        if(ioctl(fbfd, FBIOPUTCMAP, &curcmap)){
                fprint(2, "framebuffer: cannot put color map: %s\n", strerror(errno));
                return 0;
        }
	return 1;
}

static int 
get_fb4vt(int vt)
{
        struct fb_con2fbmap map;

        map.console = vt;

        if(ioctl(fbfd, FBIOGET_CON2FBMAP, &map )){
                fprint(2, "framebuffer: cannot get framebuffer console: %s\n", strerror(errno));
                return -1;
        }
  
        return map.framebuffer;
}

static int 
set_fb4vt(int virtualterm, int fb)
{
        struct fb_con2fbmap map;

        if(fb >= 0)
                map.framebuffer = fb;
        else {
                struct stat         aStat;
                if(fstat(fbfd, &aStat)){
                        fprint(2, "framebuffer: cannot get status of the framebuffer: %s\n", strerror(errno));    
                        return -1;
                }
                map.framebuffer =(aStat.st_rdev & 0xFF) >> 5;
        }

        map.console = virtualterm;
  
        if(ioctl(fbfd, FBIOPUT_CON2FBMAP, &map)){
                fprint(2, "framebuffer: cannot set a virtual terminal for the framebufer: %s\n", strerror(errno)); 
                return -1;
        }
        return 0;
}

static void
handler(int signo)
{
	USED(signo);
}

static void 
suspendproc(void* param)
{
	struct sigaction act;
	sigset_t mask;

	struct vt_mode newvtmode;

	newvtmode.mode   = VT_PROCESS;
	newvtmode.waitv  = 0;
	newvtmode.relsig = SIGIO;
	newvtmode.acqsig = SIGIO;
	if(ioctl(tty, VT_SETMODE, &newvtmode)){
                fprint(2, "framebuffer: cannot set mode for the terminal: %s\n", strerror(errno));    
		vt_deinit();
                return;
        }

	memset(&act, 0 , sizeof(act));
	act.sa_handler = handler;
	sigaction(SIGIO, &act, nil);
	
	while(1){
		sigprocmask(SIG_SETMASK, NULL, &mask);
		sigdelset(&mask, SIGIO);
		sigsuspend(&mask);
		if(!suspend){
			suspend = 1;
			if(ioctl(tty, VT_RELDISP, 1) < 0){
				fprint(2, "framebuffer: can't switch from terminal: %s\n");
				return;
			}
		}
		else{
			if(ioctl(tty, VT_RELDISP, 2) < 0){
				fprint(2, "framebuffer: can't switch to terminal: %s\n", strerror(errno));
				return;
			}
			
			if (ioctl(tty, KDSETMODE, KD_GRAPHICS ) < 0){
				fprint(2, "framebuffer: cannot set the terminal to the graphics mode: %s\n", strerror(errno));    
				return;
			}
			curvsi.activate = FB_ACTIVATE_NOW;
			if(ioctl(fbfd, FBIOPUT_VSCREENINFO, &curvsi)){
				fprint(2, "framebuffer: cannot put the new variable screen info: %s\n", strerror(errno));
				return;
		    	}		
			
			if(ioctl(fbfd, FBIOPUTCMAP, &curcmap)){
				fprint(2, "framebuffer: cannot put color map: %s\n", strerror(errno));
				return;
			}
			suspend = 0;
			Rectangle rect;
			rect.min.x = 0;
			rect.min.y = 0;
			rect.max.x = Xsize;
			rect.max.y = Ysize;
			flushmemscreen(rect);
		}
	}
}

static int 
vt_init()
{
        struct vt_stat vtstat;
        char ttyname[20] = {0};
	struct termios t;
        if((((tty0 = open("/dev/tty0", O_WRONLY)) < 0)) &&
           (((tty0 = open("/dev/vc/0", O_RDWR)) < 0))){
                fprint(2, "framebuffer: cannot open a terminal: %s\n", strerror(errno));    
                return -1;
        }
        if(ioctl(tty0, VT_GETSTATE, &vtstat) < 0){
                fprint(2, "framebuffer: cannot get state of the terminal: %s\n", strerror(errno));    
                close(tty0);
                return -1;
        }
  
        prevtty = vtstat.v_active;

        if((ioctl(tty0, VT_OPENQRY, &activetty) == -1) || 
           (activetty ==(unsigned short)-1)){
                fprint(2, "framebuffer: cannot open new terminal: %s\n", strerror(errno));    
                close(tty0);
                return -1;
        }
  
        oldfb = get_fb4vt(activetty);
        set_fb4vt(activetty, -1);
  
        if(ioctl(tty0, VT_ACTIVATE, activetty)){
                fprint(2, "framebuffer: cannot activate terminal: %s\n", strerror(errno));    
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }

        if(ioctl(tty0, VT_WAITACTIVE, activetty)){
                fprint(2, "framebuffer: cannot activate terminal: %s\n", strerror(errno));    
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }
  
        if((snprintf(ttyname, sizeof(ttyname), "/dev/tty%d", activetty) < 0) ||
           (((tty = open(ttyname, O_RDWR)) < 0) && 
            (errno != ENOENT)) &&
           (snprintf(ttyname, sizeof(ttyname),  "/dev/vc/%d", activetty) < 0) ||
           ((tty = open(ttyname, O_RDWR)) < 0)){
                fprint(2, "framebuffer: cannot activate terminal: %s\n", strerror(errno));    
                ioctl(tty0, VT_ACTIVATE, prevtty);
                ioctl(tty0, VT_WAITACTIVE, prevtty);
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }

        if(ioctl(tty, KDGETMODE, &oldtermmode)){
                fprint(2, "framebuffer: cannot get a terminal mode: %s\n", strerror(errno));    
                ioctl(tty0, VT_ACTIVATE, prevtty);
                ioctl(tty0, VT_WAITACTIVE, prevtty);
                close(tty);
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }
  
        if(ioctl(tty, KDSETMODE, KD_GRAPHICS)){
                fprint(2, "framebuffer: cannot set the terminal to the graphics mode: %s\n", strerror(errno));    
                ioctl(tty0, VT_ACTIVATE, prevtty);
                ioctl(tty0, VT_WAITACTIVE, prevtty);
                close(tty);
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }
  
        ioctl(tty0, TIOCNOTTY, 0);
        ioctl(tty, TIOCSCTTY, 0);
  
        const char setcursoroff [] = "\033[?1;0;0c";
        const char setblankoff [] = "\033[9;0]";

        write(tty, setcursoroff, sizeof(setcursoroff));
        write(tty, setblankoff, sizeof(setblankoff));


        if(ioctl(tty, VT_GETMODE, &oldvtmode)){
                fprint(2, "framebuffer: cannot get mode for the terminal: %s\n", strerror(errno));    
                ioctl(tty0, VT_ACTIVATE, prevtty);
                ioctl(tty0, VT_WAITACTIVE, prevtty);
                close(tty);
                ioctl(tty0, VT_DISALLOCATE, activetty);
                close(tty0);
                return -1;
        }


	kproc("suspendProc", suspendproc, 0, 0);

	tcgetattr(tty, &oldtermattr);
	t = oldtermattr;
	t.c_lflag &= ~(ICANON|ECHO|ISIG);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(tty, TCSANOW, &t);

        return 0;
}  

static void 
vt_deinit()
{
	tcsetattr(tty, TCSANOW, &oldtermattr);
        const char setcursoron [] = "\033[?0;0;0c";
        const char setblankon [] = "\033[9;10]";
     
        write(tty, setcursoron, sizeof(setcursoron));
        write(tty, setblankon, sizeof(setblankon));

        ioctl(tty, VT_SETMODE, &oldvtmode);
     
        ioctl(tty, KDSETMODE, oldtermmode);
        ioctl(tty0, VT_ACTIVATE, prevtty);
        ioctl(tty0, VT_WAITACTIVE, prevtty);
        set_fb4vt(activetty, oldfb);
        close(tty);
        ioctl(tty0, VT_ACTIVATE, prevtty);
        ioctl(tty0, VT_WAITACTIVE, prevtty);
        ioctl(tty0, VT_DISALLOCATE, activetty);
        close(tty0);
}
