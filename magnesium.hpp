/** 
  * @file  magnesium.hpp
  * @brief Interrupt-based preemptive multitasking.
  * License: Public domain. The code is provided as is without any warranty.
  */

#ifndef _MAGNESIUM_HPP_
#define _MAGNESIUM_HPP_

#include <optional>
#include <array>
#include "mg_port.h"

namespace magnesium {

template<class T> using option = std::optional<T>;

template<class T> class owner {
    T* ptr;
public:
    owner(T* pointer) noexcept : ptr(pointer) {}

    owner(owner&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    
    ~owner() {
        if (ptr != nullptr) {
            ptr->drop();
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

    template<class U> const U* as() const {
        return static_cast<const U*>(ptr);
    }

    owner(std::nullptr_t) = delete;
    owner(const owner&) = delete;
    owner& operator=(const owner& other) = delete;
    owner& operator=(owner&& other) = delete;
};

class queue_base;
class actor;
class message;

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

    inline bool is_empty() const {
        return (this->next == this);
    }

public:
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

class message : public node {
    queue_base* parent;
public:
    inline void set_parent(queue_base* q) {
        parent = q;
    }

    void drop() noexcept;
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

class queue_base {
    list items;
    int length = 0;
 
    inline option<owner<actor>> push_internal(owner<message>& msg);
protected:
    mutex lock;
    ~queue_base() = default;
public:
    option<owner<message>> pop(option<owner<actor>>&& subscriber);
    void push(owner<message>&& msg);
};

void message::drop() noexcept {
    parent->push(owner(this));
}

class actor : public node {
    option<owner<message>> mailbox;

    virtual queue_base& run(owner<message> msg) = 0;
protected:
    ~actor() = default;
public:
    const unsigned int vect;
    const unsigned int prio;

    actor(unsigned int vect) noexcept : mailbox(), vect(vect), prio(pic_vect2prio(vect)) {}

    inline void poll(queue_base& q) {
        q.pop(owner(this));
        // TODO: assert queue is empty
    }

    inline void set_message(owner<message>& msg) {
        mailbox.emplace(std::move(msg));
    }

    inline queue_base& call() {
        // TODO: assert mailbox nonempty
        actor* me = this;
        return me->run(std::move(*mailbox));
    }

    void drop() noexcept {
        // TODO: assert actors should never be dropped
    }
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

            for (;;) {
                auto& next_queue = active_actor->call();
                auto msg = next_queue.pop(std::move(item));
                
                if (msg) {
                    active_actor->set_message(*msg);
                }
                else {
                    break;
                }
            }
        }
    }
};

option<owner<actor>> queue_base::push_internal(owner<message>& msg) {
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

option<owner<message>> queue_base::pop(option<owner<actor>>&& subscriber) {
    locked_region region(lock);
    const int queue_length = length--;

    if (queue_length <= 0) {
        if (subscriber) {
            items.enqueue(*subscriber);
        }
    } 
    else {
        return items.dequeue<message>();
    }

    return std::nullopt;
}

void queue_base::push(owner<message>&& msg) {
    option<owner<actor>> subscriber = push_internal(msg);
    
    if (subscriber) {
        scheduler::activate(*subscriber);
    }
}
    
template<class T> class queue : public queue_base {
public:
    inline void push(owner<T>& msg) {
        queue_base::push(msg.release());
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
            message& inner_msg = static_cast<message&>(item);
            inner_msg.set_parent(this);
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
            option<owner<message>> item = this->pop({});
            
            if (item) {
                return owner<T>(static_cast<T*>((*item).release()));
            }
        }

        return msg;
    }
};

};

#endif

