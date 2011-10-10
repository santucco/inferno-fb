/*************************************************
*
* Usual PC input handling for Inferno OS
*
* Copyright (c) 2005, 2006, 2008, 2011 Alexander Sychev.
* All rights reserved.
* Use of this source code is governed by a BSD-style
* license that can be found in the LICENSE file.
*
**************************************************/

#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "input.h"

#include "input_kbd.c"
#include "input_mouse.c"

struct Handler handlers[] = {{-1, "/dev/input/event3", "ptrProc", input_init_vp, mouse, input_deinit},
			     {-1, "/dev/input/event2", "kbdProc", input_init, keyboard, input_deinit},
			     {-1, 0, 0, 0, 0}};
