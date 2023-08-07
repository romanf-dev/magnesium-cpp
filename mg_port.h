/** 
  * @file  mg_port.h
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#define MG_PRIO_MAX 1

#define mg_object_lock(p) { asm volatile ("cpsid i"); }
#define mg_object_unlock(p) { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) 0

#define STIR_ADDR ((volatile unsigned int*) 0xE000EF00)
#define pic_interrupt_request(v) ((*STIR_ADDR) = v)

#endif

