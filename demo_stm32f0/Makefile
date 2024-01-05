#
# Simple makefile for compiling all .c and .s files in the current folder.
# No dependency tracking, use make clean if a header is changed.
#

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-

%.o : %.cpp
	$(GCC_PREFIX)g++ -std=c++20 -fno-rtti -fno-exceptions -mcpu=cortex-m0 -Wall -O2 -DSTM32F030 -DMG_NVIC_PRIO_BITS=2 -mthumb -I . -I $(MG_DIR) -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m0 -mthumb -c -o $@ $<

all : $(OBJS) startup.o
	$(GCC_PREFIX)g++ -Wl,--gc-sections -mthumb -Wl,-T,gcc.ld -o demo.elf $(OBJS) startup.o

clean:
	rm -f *.o *.elf

