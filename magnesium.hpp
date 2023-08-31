/** 
  * @file  magnesium.hpp
  * @brief Interrupt-based preemptive multitasking.
  * License: Public domain. The code is provided as is without any warranty.
  */

#ifndef _MAGNESIUM_HPP_
#define _MAGNESIUM_HPP_

#include <optional>
#include <array>
#include <coroutine>
#include "mg_port.h"

namespace magnesium {

template<class T> using option = std::optional<T>;

template<class T> class owner {
    T* ptr;
       
    void drop(T*);
public:

    owner(T* pointer) noexcept : ptr(pointer) {}

    owner(owner&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    
    ~owner() {
        if (ptr != nullptr) {
            drop(ptr);
        }
    }
    
    inline T* release() {
        T* const temp = ptr;
        ptr = nullptr;
        return temp;
    }

    T* operator->() const {
        return ptr; // TODO: assert ptr != nullptr
    }

    owner(std::nullptr_t) = delete;
    owner(const owner&) = delete;
    owner& operator=(const owner& other) = delete;
    owner& operator=(owner&& other) = delete;
};

class node {
    node* next;
    node* prev;
protected:
    ~node() = default;  // can't be destructed through 'delete node'
public:
    
    friend class list;
    friend class message;
    friend class actor;

    // intrusive container and its items are neither copyable non movable
    node() = default;
    node(const node&) = delete;
    node& operator=(const node& other) = delete;
    node& operator=(node&& other) = delete;
};

class list : public node {
public:

    inline bool is_empty() const {
        return (this->next == this);
    }

    list() noexcept {
        this->next = this->prev = this;
    }

    template<class T> inline void enqueue(owner<T>& object) {
        node* const link = static_cast<node*>(object.release());
        link->next = this;
        link->prev = this->prev;
        link->prev->next = link;
        this->prev = link;
    }

    template<class T> inline option<owner<T>> dequeue() {
        option<owner<T>> object = {};

        if (!is_empty()) {
            node* const link = this->next;
            link->prev->next = link->next;
            link->next->prev = link->prev;
            link->next = link->prev = nullptr;
            object.emplace(owner(static_cast<T*>(link)));
        } 

        return object;
    }   
};

class queue_base {};

struct message : public node {
    queue_base* parent;
};

class mutex {};

class locked_region {
    mutex& lock;
public:
    locked_region(mutex& object) noexcept : lock(object) {
        mg_object_lock(&lock);
    }

   ~locked_region() {
        mg_object_unlock(&lock);
    }
};

struct future {
    struct promise_type {
        static void* allocate(std::size_t n);        
        future get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        void* operator new(std::size_t n) { return allocate(n); }
    };
};

template<class T> class queue;

class actor : public node {
    message* mailbox = nullptr;
    std::coroutine_handle<> frame;
        
    virtual future run() = 0;
protected:
    ~actor() = default;
public:   
    const unsigned int vect;
    const unsigned int prio;

    actor(unsigned int vect) noexcept : vect(vect), prio(pic_vect2prio(vect)) {}

    template<class T> inline void set_message(owner<T>& msg) {
        mailbox = msg.release();
    }
    
    template<class T> inline owner<T> get_message() {
        message* temp = mailbox;
        mailbox = nullptr;
        return owner<T>(static_cast<T*>(temp));
    }    

    inline void set_handle(std::coroutine_handle<> handle) {
        frame = handle;
    }
        
    void call() {
        frame();
    }

    inline void start() {
        this->run();
    }  

    template<class T> inline auto pop(queue<T>& q);
};
   
class scheduler {
    mutex lock;
    std::array<list, MG_PRIO_MAX> runqueue;
    static scheduler context;

    static option<owner<actor>> extract(list& runq) {
        locked_region region(context.lock);
        return runq.dequeue<actor>();
    }
    
public:
    static void activate(owner<actor>& target) {
        locked_region region(context.lock);
        pic_interrupt_request(target->vect);
        context.runqueue[target->prio].enqueue(target);
    }

    static void schedule(unsigned int vect) {
        const unsigned int prio = pic_vect2prio(vect);

        while (option<owner<actor>> item = extract(context.runqueue[prio])) {
            auto& active_actor = *item;
            active_actor->call();
        }
    }
};

template<class T> class queue : public queue_base {
    list items;
    int length = 0;
    
    option<owner<actor>> push_internal(owner<T>& msg) {
        locked_region region(lock);
        const int queue_length = length++;

        if (queue_length >= 0) {
            items.enqueue(msg);
        } 
        else {
            option<owner<actor>> item = items.dequeue<actor>();
            owner<actor>& subscriber = *item;
            subscriber->set_message(msg);
            return item;
        }
        
        return std::nullopt;
    }
    
    option<owner<T>> pop_internal(actor& subscriber, std::coroutine_handle<> h) {
        locked_region region(lock);
        const int queue_length = length--;

        if (queue_length <= 0) {
            subscriber.set_handle(h);
            auto subscr_owner = owner(&subscriber);
            items.enqueue(subscr_owner);
        } 
        else {
            return items.dequeue<T>();
        }

        return std::nullopt;
    }    
        
protected:
    mutex lock;
    
    option<owner<T>> try_pop() {
        locked_region region(lock);

        if (length > 0) {
            --length;
            return items.dequeue<T>();
        } 
        
        return std::nullopt;
    }
        
public:

    inline void push(owner<T>& msg) {
        option<owner<actor>> subscriber = push_internal(msg);
        
        if (subscriber) {
            scheduler::activate(*subscriber);
        }
    }
        
    auto pop(actor& subscriber) {
        
        struct awaitable {
            actor& subscriber;
            queue<T>& source;
            
            awaitable(queue<T>& q, actor& a): subscriber(a), source(q) {}
            
            bool await_ready() const noexcept { 
                option<owner<T>> msg = source.try_pop();
                
                if (msg) {
                    subscriber.set_message(*msg);
                }
                
                return (bool)msg;
            }
            
            bool await_suspend(std::coroutine_handle<> h) {
                option<owner<T>> msg = source.pop_internal(subscriber, h);
                
                if (msg) {
                    subscriber.set_message(*msg);
                }

                return !(bool)msg;
            }
            
            owner<T> await_resume() {
                return subscriber.get_message<T>();
            }
        };
        
        return awaitable(*this, subscriber);
    }
};

template<class T> class message_pool : public queue<T> {
    T* const items_array;
    const std::size_t array_length;
    std::size_t offset;

    inline option<owner<T>> try_pick_from_array() {
        locked_region region(this->lock);

        if (offset < array_length) {
            T& item = items_array[offset++];
            item.parent = this;
            return owner(&item);
        }
        
        return std::nullopt;
    }

public:
     template<unsigned int N> message_pool(T (&arr)[N]) : 
        items_array(&arr[0]), 
        array_length(sizeof(arr) / sizeof(arr[0])), 
        offset(0) {}

     option<owner<T>> alloc() {
        auto msg = try_pick_from_array();
        
        if (!msg) {
            return this->try_pop();            
        }

        return msg;
    }
};

template<class T> inline auto actor::pop(queue<T>& q) {
    return q.pop(*this);
}

template<class T> void owner<T>::drop(T* msg) {
    queue<T>* q = static_cast<queue<T>*>(msg->parent);
    owner<T> o = owner(msg);
    q->push(o);
}

template<> void owner<actor>::drop(actor* a) {
    //TODO: assert actor never dropped
}

};

#endif
