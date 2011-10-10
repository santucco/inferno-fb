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


#if !defined(__INPUT_H_included__)
#define __INPUT_H_included__
#include <linux/input.h>

enum
{
	DblTime	= 300000		/* double click time in micro seconds */
};

/* pointer/keyboard specific functions */
typedef struct Handler Handler;
struct Handler{
	int fd;
	char* filename;
	char* procname;
	int (*init)(Handler* handler);
	void (*func)(struct input_event*, int);
	void (*deinit)(Handler*);
};

int input_init(Handler*);
int input_init_vp(Handler*); /* initialization with turning pointer visible */ 
void input_deinit(Handler*);

extern struct Handler handlers[];
extern int ispointervisible;

#endif /*__INPUT_H_included__*/
