#
# Simple makefile for compiling all .c and .s files in the current folder.
# No dependency tracking, use make clean if a header is changed.
#

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-

%.o : %.cpp
	$(GCC_PREFIX)g++ -std=c++17 -fno-rtti -fno-exceptions -mcpu=cortex-m3 -Wall -O2 -DSTM32F103xB -mthumb -I . -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m3 -mthumb -c -o $@ $<

all : $(OBJS) startup_stm32f103c8tx.o
	$(GCC_PREFIX)g++ -Wl,--gc-sections -mthumb -Wl,-T,gcc.ld -o demo.elf $(OBJS) startup_stm32f103c8tx.o

clean:
	rm -f *.o *.elf

