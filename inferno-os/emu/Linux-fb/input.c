/***********************************************
*
* Generic input handling for Inferno OS
*
* Copyright (c) 2008 Salva Peiró.
* Copyright (c) 2008, 2011 Alexander Sychev.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
************************************************/

#include "dat.h"
#include "fns.h"
#include "input.h"
#include "fb.h"

enum
{
	NEVENTS = 5,
	EVENTSZ = sizeof(struct input_event),
};

static void 
proc(void* param)
{
	int count;
	Handler* handler = (Handler*)param;
	struct input_event ev[NEVENTS];
	for(;;) {
		count = read(handler->fd, ev, sizeof(ev));
		if(count >= EVENTSZ && !suspend)
			handler->func(ev, count / EVENTSZ);
	}
}

static void 
init(void)
{
	int i = 0;
	
	for(i = 0; handlers[i].func != 0; ++i){
		if(handlers[i].init(&handlers[i]) < 0)
			continue;

		kproc(handlers[i].procname, proc, &handlers[i], 0);
	}
	if(handlers[i].func != 0){
		fprint(2, "emu: handlers haven't started\n");
		for(i = 0; handlers[i].func !=0; ++i)
			handlers[i].deinit(&handlers[i]);
		
	}
}

int
input_init(Handler* handler)
{
	if(((handler->fd = open(handler->filename, O_RDONLY)) < 0)) {
		fprint(2, "can't open %s device: %s\n", handler->filename, strerror(errno));
		return -1;
	}
	return 0;
}

int
input_init_vp(Handler* handler)
{
	ispointervisible = 1; // have pointer only under acme?
	return input_init(handler);
}

void 
input_deinit(Handler* handler)
{
	if(handler->fd != -1)
		close(handler->fd);
}

void 
inputlink()
{
	init();
}

