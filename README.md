Experimental version of the magnesium framework in C++
======================================================


DRAFT


Overview
--------

C++20 async/await features may be used to make the original (written in C) [magnesium framework](https://github.com/romanf-dev/magnesium) more robust and user-friendly.

Actors are now represented as resumable coroutines. This allows to verify message/queue types at compile-time.

Although the repository contains demo application for the STM32 Bluepill board, the magnesium.hpp file is cross-platform and have no dependencies on board-related headers.


API description
---------------


All message types must be derived classes of magnesium::message. Array of messages of given type must be specified as a message pool constructor parameter:


    struct foo_msg : public message {
        ...
    } g_msg_array[10];

    message_pool g_pool(g_msg_array);


Queues are template classes parametrized with the message type:

    queue<foo_msg> g_queue;


Device interrupt handlers allocate message and push them to a queue:

    auto allocated = g_pool.alloc();

    if (allocated) {
        auto& msg = *allocated;

        msg->... = ... // set the message payload

        g_queue.push(msg);
    }


Interrupt handlers designated to run actors must contain scheduling call:

    scheduler::schedule( ...current interrupt vector... );


Don't forget to declare scheduler structures at global scope:

    scheduler scheduler::context;


Since actors are coroutines the allocator is required. This is called once on first run of every actor's coroutine.

    void* magnesium::future::promise_type::allocate(std::size_t n) {...}


Actors must be derived from actor base class. It should override run virtual function:

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

