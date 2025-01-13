#include <coroutine>
#include <list>
#include <print>

class Scheduler {
private:
    std::list<std::coroutine_handle<>> tasks{};

public:
    auto tasks_count() const { return tasks.size(); }
    bool schedule() {
        auto t = tasks.front();
        tasks.pop_front();

        std::println("resume corohandle addr {:#010x}", reinterpret_cast<uintptr_t>(t.address()));
        if(!t.done()) t.resume();

        return !tasks.empty();
    }

    auto suspend() {
        struct awaiter: std::suspend_always {
            Scheduler & s;
            explicit awaiter(Scheduler&sched) : s{sched} {}
            void await_suspend(std::coroutine_handle<> coro) const noexcept { 
                s.tasks.push_back(coro); 
                std::println("suspend size {} corohandle addr {:#010x}", s.tasks.size(), reinterpret_cast<uintptr_t>(coro.address()));
            }
        };
        return awaiter{*this};
    }

    auto suspend(std::coroutine_handle<> coro) { tasks.push_back(coro); }
};

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
    };
};

Scheduler gScheduler;

struct suspend { 
    auto operator co_await() {
        struct awaiter : std::suspend_always {
            void await_suspend(std::coroutine_handle<> coro) const noexcept { gScheduler.suspend(coro); } 
        };
    
        return awaiter{};
    }
};


template <char TaskName>
Task task(Scheduler &s) {
    std::println("Start task {}", TaskName);
    co_await s.suspend();
    std::println("Middle task {}", TaskName);
    co_await s.suspend();
    std::println("End task {}", TaskName);
}


template <char TaskName>
Task task() {
    std::println("Start task {}", TaskName);
    co_await suspend{};
    std::println("Middle task {}", TaskName);
    co_await suspend{};
    std::println("End task {}", TaskName);
}

void use_sched_1() {
    Scheduler s;
    task<'1'>(s);
    task<'2'>(s);
    while (s.schedule());
}

void use_sched_2() {
    task<'1'>();
    task<'2'>();
    while (gScheduler.schedule());
}

int main() {
    use_sched_1();
    return 0;
}
