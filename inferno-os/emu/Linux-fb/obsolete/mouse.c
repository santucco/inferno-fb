/**************************************************
*
* PC mouse support for Inferno OS
* based on sources form Inferno distribution
*
* Author: Alexander Sychev <santucco@gmail.com>
*
***************************************************/

#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include <termios.h>
#include <unistd.h>

/*
 *  mouse types
 */
enum {
	Mouseother,
	Mouseserial,
	MousePS2,
	Mousebus,
	Mouseintelli,
	Mousemsbus,
};


extern int mouseshifted;
extern int ispointervisible;

static struct {
        int type;
        int fd;
} mouse;

/*
 *  ps/2 mouse message is three bytes
 *
 *	byte 0 -	0 0 SDY SDX 1 M R L
 *	byte 1 -	DX
 *	byte 2 -	DY
 *
 *  shift & left button is the same as middle button
 */

static int 
ps2_write(uchar* buf, int bufsize)
{
        int i;
        int errcount = 0;
        for(i = 0; i < bufsize; i++) {
                uchar sym;
                write(mouse.fd, &buf[i], sizeof(uchar));
                read(mouse.fd, &sym, sizeof(sym));
                if(sym != 0xFA) 
                        errcount++;
        }
        tcflush(mouse.fd, TCIFLUSH);
        return errcount;
}

static void 
setintellimouse(void)
{
        static uchar samplerates[] = {0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50};
        ps2_write(samplerates, sizeof(samplerates));	/* set sample */
}

static int 
intellimousedetect(void)
{
        uchar id = 0;
        setintellimouse();
        /* check whether the mouse is now in extended mode */
        static uchar getid = 0xF2;
        if(!ps2_write(&getid, sizeof(getid)))
                read(mouse.fd, &id, sizeof(id));
        if(id != 3) {
                /*
                 * set back to standard sample rate(100 per sec)
                 */
                static uchar samplerates[] = {0xF3, 0x64};
                ps2_write(samplerates, sizeof(samplerates));
                return 0;
        }
        return 1;
}

static void 
ps2mouseputc(uchar* packet, int packetsize)
{
        static uchar b[] = {0, 1, 4, 5, 2, 3, 6, 7, 0, 1, 2, 5, 2, 3, 6, 7};
        int buttons = 0, dx, dy;
        int protosize = (mouse.type == Mouseintelli) ? 4 : 3;
        int i;
        for(i = 0; i < packetsize; i += protosize) {
                if((packet[i] & 0xc8) != 0x08)
                        continue;
                buttons = b[(packet[i] & 7) |(mouseshifted ? 8 : 0)];
                dx =(packet[i] & 0x10) ? packet[i + 1] - 256 :  packet[i + 1];
                dy =(packet[i] & 0x20) ? -(packet[i + 2] - 256) : -(packet[i + 2]);

        }
        return;
}

static void 
mouseproc(void* dummy)
{
        uchar buf[4 * 3  * 4 * 3];
        USED(dummy);
        for(;;) {
                int count = read(mouse.fd, buf, sizeof(buf));
                if(count > 0) 
                        ps2mouseputc(buf, count);
        }
}

/*
 *  set up a ps2 mouse
 */
static void 
ps2mouse(void)
{
        if(mouse.type != Mouseother)
                return;

        if(((mouse.fd = open("/dev/psaux", O_RDWR)) < 0) &&  
           ((mouse.fd = open("/dev/input/mice", O_RDWR)) < 0)) {
                fprint(2, "can't open mouse device: %s\n", strerror(errno));
                return;
        }
       
        if(intellimousedetect())
                mouse.type = Mouseintelli;   
        else 
                mouse.type = MousePS2;
        /* make mouse streaming, enabled */
        static uchar aModes[] = {0xEA, 0xF4};
        ps2_write(aModes, sizeof(aModes));

        ispointervisible = 1;
        if(kproc("mouseproc", mouseproc, nil, 0) < 0) {
                fprint(2, "emu: can't start mouse procedure");
                close(mouse.fd);
        }
}

void 
mouselink(void)
{
        ps2mouse();
}
