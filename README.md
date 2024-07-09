Magnesium is a simple header-only framework implementing actor model for deeply embedded systems. It uses interrupt controller hardware for scheduling and supports preemptive multitasking. This is experimental version of the framework in C++20.

C++ async/await features are used to make the original (written in C) [magnesium framework](https://github.com/romanf-dev/magnesium) more robust and user-friendly. Actors are now represented as resumable coroutines.
Although the repository contains demo application for the STM32 Bluepill board, the magnesium.hpp file is cross-platform and have no dependencies on board-related headers.


Features
--------

- Preemptive multitasking
- Hardware-assisted scheduling
- Unlimited number of actors and queues
- Message-passing communication
- Timer facility
- Hard real-time capability
- Only ARMv6-M and ARMv7-M are supported at now


API usage examples
------------------


All message types must be derived classes of magnesium::message. Array of messages of given type must be specified as a message pool constructor parameter:


    struct foo_msg : public message {
        ...
    } g_msg_array[10];

    message_pool g_pool(g_msg_array);


Queues are template classes parametrized with the message type:

    queue<foo_msg> g_queue;


Device interrupt handlers may communicate with actors by pushing messages into queues:

    auto allocated = g_pool.alloc();

    if (allocated) {
        auto& msg = *allocated;

        msg->... = ... // set the message payload

        g_queue.push(msg);
    }


Interrupt handlers designated to run actors must contain the scheduling call:

    scheduler::schedule( ...current interrupt vector... );


Don't forget to declare scheduler structures at global scope:

    scheduler scheduler::context;


Since actors are coroutines the allocator is required. This is called once on first run of every actor's coroutine.

    void* magnesium::future::promise_type::allocate(std::size_t n) {...}


Actors must be derived classes of the 'actor' class with overridden 'run' virtual function. Actor function must not return.

    struct foo_actor : public actor {

        foo_actor(unsigned int vect) noexcept : actor(vect) {}

        future run() override {
            for(;;) {
                auto msg = co_await poll(g_queue);
                ...
            }
        }
    };


co_await arguments are calls for 'poll' function with certain queue. Value of the whole co_await expression is owner<T> where T is the message type.

Actors should be defined with interrupt vector:

    foo_actor g_actor(EXAMPLE_VECTOR);

Main function must setup the interrupt controller and call run function of each actor to cause suspension on the await. The actor will be activated every time when queue it polls is nonempty.

If your board has a tick source, put tick call into the appropriate interrupt handler.

        timer::tick();

Actors may request delays using sleep call:

        co_await sleep(<ticks>);

