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
};

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
    };
};

template <char TaskName>
Task task(Scheduler &s) {
    std::println("Start task {}", TaskName);
    co_await s.suspend();
    std::println("Middle task {}", TaskName);
    co_await s.suspend();
    std::println("End task {}", TaskName);
}

int main() {
    Scheduler s;
    task<'1'>(s);
    task<'2'>(s);
    while (s.schedule());
    std::println("About to exit - tasks count {}", s.tasks_count());
    return 0;
}
