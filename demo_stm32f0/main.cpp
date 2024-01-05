/** 
  ******************************************************************************
  *  @file   main.cpp
  *  @brief  Toy example of magnesium actor framework.
  ******************************************************************************
  *  License: Public domain.
  *****************************************************************************/

#include "stm32f0xx.h"
#include "magnesium.hpp"

using namespace magnesium;

void operator delete(void*) {}  // all objects are persistent
const unsigned int EXAMPLE_VECTOR = 0;

static inline void led_on() {
    GPIOA->BSRR = GPIO_BSRR_BR_4;
}

static inline void led_off() {
    GPIOA->BSRR = GPIO_BSRR_BS_4;
}

static void panic() {
    __disable_irq();
    led_on();
    for(;;);
}

extern "C" void HardFault_Handler() {
    panic();
}

void* magnesium::future::promise_type::allocate(std::size_t n) {
    static uint8_t buffer[256];
    static std::size_t ptr = 0;
    const std::size_t old_ptr = ptr;
    ptr += n;
    
    if (ptr > sizeof(buffer)) {
        panic();
    }
    
    return buffer + old_ptr;
}

static struct example_msg : public message {
    unsigned int led_state;
} g_msgs[10];

static message_pool g_pool(g_msgs);
static queue<example_msg> g_queue;

class systick_actor : public actor {
public:
    systick_actor(unsigned int vect) noexcept : actor(vect) {}

    future run() override {
        for(;;) {
            //auto msg = co_await poll(g_queue);
            
            co_await sleep(50);
            led_off();
            co_await sleep(50);
            led_on();
        }
    }
};

scheduler scheduler::context;
timer timer::context;
static systick_actor g_actor(EXAMPLE_VECTOR);

extern "C" void WWDG_IRQHandler() {
    scheduler::schedule(EXAMPLE_VECTOR);
}

extern "C" void SysTick_Handler() {
    timer::tick();
}

extern "C" void hwinit() {
    //
    // Enable HSE and wait until it is ready.
    //
    RCC->CR |= RCC_CR_HSEON;

    while(!(RCC->CR & RCC_CR_HSERDY)) {
    }
    
    //
    // Setup flash prefetch.
    //
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

    //
    // Set PLL multiplier to 6, set HSE as PLL source and enable the PLL.
    //
    RCC->CFGR |= RCC_CFGR_PLLMUL6;
    RCC->CFGR |= RCC_CFGR_PLLSRC_1;
    RCC->CR |= RCC_CR_PLLON;

    while(!(RCC->CR & RCC_CR_PLLRDY)) {
    }
    
    //
    // Switch to PLL and wait until switch succeeds.
    //
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;

    while(!(RCC->CFGR & RCC_CFGR_SWS_PLL)) {
    }

    //
    // Disable HSI.
    //
    RCC->CR &= ~RCC_CR_HSION;

    //
    // Enable LED.
    //
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= GPIO_MODER_MODER4_0;
}

extern "C" int main() {
    NVIC_EnableIRQ(WWDG_IRQn);
    __enable_irq();

    g_actor.run();

    SysTick->LOAD  = 48000U - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    for (;;);
    return 0;
}
