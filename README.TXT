These sources contain the implementation of support linux framebuffer for the Inferno emu.
 
With this code the Inferno can be used on devices with linux without  X Window server, such as usual personal computers or notebooks or handhelds computers.
I use it on IBM ThinkPad T42 with Gentoo Linux and HP iPAQ 3970 with Familiar Linux 0.8.4 <www.handhelds.org>, Salva Peiró <saoret.one@gmail.com> made a port to Tungsten Palm T3.

In the PC-version of the emu with framebuffer there is a generic mouse and keyboard support via linux input events, so you should compile you linux kernel with support of input events (Device Drivers->Input device support->Event interface). 
I left the old sources of mouse and keyboard handling in the 'obsolete' directory. 
If you use old linux kernel without the input event support you should add fields 'mouse' and 'kbd' into the 'link' section of emu-fb, add a define OLD_KBD_SUPPORT to CFLAGS in the mkfile. 
the PC-version of the Inferno emu can be used as usual version of the emu.

To switch from the emu to another terminal and back you should use Alt-Fx (Ctrl-Alt-Fx for X Window) combination.

To build the ARM-port you need a toolchain for ARM processor - I use a Handhelds official toolchain from www.handhelds.org

In the ARM-version of the emu with framebuffer there are a generic touchscreen support and a generic power management support. 
For the power management you should bind a apm device ( bind #S /dev ). 
To suspend your device you should write a word 'suspend' to /dev/apm ( the suspend button works too ).
The battery status you can read from /dev/apm.

To emulate a 3-buttons mouse on your iPAQ you can use combinations from hard buttons and a stylus touch.
To emulate the middle button you can use the 'Calendar' button and the 'iPAQ Task Manager' button, to emulate the left button you can use the 'Contacts' button and the 'Inbox' button.
To press 'Esc' you can use the 'Record' button.


TODO:
 - floating point support for ARM-processors


Gool luck,
  Alexander Sychev <santucco@gmail.com>

