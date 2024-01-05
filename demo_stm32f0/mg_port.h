/** 
  * @file  mg_port.h
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_NVIC_PRIO_BITS
#error Define MG_NVIC_PRIO_BITS as maximum number of supported preemption priorities for the target chip.
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX (1U << MG_NVIC_PRIO_BITS)
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

static inline unsigned int mg_port_clz(unsigned int x) {
    unsigned int i = 0;

    for (; i < 32; ++i) {
        if (x & (1U << (31 - i))) {
            break;
        }
    }
    
    return i;
}

#define mg_object_lock(p) { asm volatile ("cpsid i"); }
#define mg_object_unlock(p) { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) \
    ((((volatile unsigned char*)0xE000E400)[v]) >> (8 - MG_NVIC_PRIO_BITS))

#define ISPR_ADDR ((volatile unsigned int*) 0xE000E200)
#define pic_interrupt_request(v) ((*ISPR_ADDR) = 1U << (v))

#endif

