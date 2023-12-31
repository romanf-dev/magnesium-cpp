/** 
  ******************************************************************************
  *  @file   main.cpp
  *  @brief  Toy example of magnesium actor framework.
  ******************************************************************************
  *  License: Public domain.
  *****************************************************************************/

#include "stm32f1xx.h"
#include "magnesium.hpp"

using namespace magnesium;

void operator delete(void*) {}  // all objects are persistent
const unsigned int EXAMPLE_VECTOR = 20;

static inline void led_on() {
    GPIOC->BSRR = GPIO_BSRR_BR13;
}

static inline void led_off() {
    GPIOC->BSRR = GPIO_BSRR_BS13;
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
            auto msg = co_await poll(g_queue);
        
            if (msg->led_state == 0)
                led_off();
            else
                led_on();
        }
    }
};

scheduler scheduler::context;
static systick_actor g_actor(EXAMPLE_VECTOR);

extern "C" void USB_LP_CAN1_RX0_IRQHandler() {
    scheduler::schedule(EXAMPLE_VECTOR);
}

extern "C" void SysTick_Handler() {
    static unsigned int led_state = 0;
    auto allocated = g_pool.alloc();
    
    led_state ^= 1;

    if (allocated) {
        auto& msg = *allocated;
        msg->led_state = led_state;
        g_queue.push(msg);
    }
}

extern "C" void hwinit() {
    RCC->CR |= RCC_CR_HSEON;            
    while(!(RCC->CR & RCC_CR_HSERDY))
        ;
    
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    RCC->CFGR |= RCC_CFGR_SW_HSE;
    RCC->CFGR |= RCC_CFGR_PLLMULL9;
    RCC->CFGR |= RCC_CFGR_PLLSRC;

    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY))
        ;
    
    RCC->CFGR = (RCC->CFGR | RCC_CFGR_SW_PLL) & ~RCC_CFGR_SW_HSE;
    while(!(RCC->CFGR & RCC_CFGR_SWS_PLL))
        ;

    RCC->CR &= ~RCC_CR_HSION;

    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH |= GPIO_CRH_CNF13_0 | GPIO_CRH_MODE13_1;
    GPIOC->BSRR = GPIO_BSRR_BS13;
}

extern "C" int main() {
    NVIC_SetPriorityGrouping(3);
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    __enable_irq();

    g_actor.run();

    SysTick->LOAD  = 72000U * 100 - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    for (;;);
    return 0;
}
