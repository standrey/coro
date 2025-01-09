#include <coroutine>
#include <iostream>
#include <string>
#include <print>
#include <utility>

using namespace std::string_literals;

struct Chat {

    struct promise_type {
    std::string _msgout{};
    std::string _msgin{};

    void unhandled_exception() noexcept {}
    Chat get_return_object() { return Chat{this}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always yield_value(std::string msg) noexcept {
        _msgout = std::move(msg);
        return {};
    }

    auto await_transform(std::string) noexcept {
        struct awaiter { 
            promise_type & pt;
            constexpr bool await_ready() const noexcept { return true; }
            std::string await_resume() const noexcept { return std::move(pt._msgin);}
            void await_suspend(std::coroutine_handle<>) const noexcept {}
        };
        
        return awaiter{*this};
    }

    void return_value(std::string msg) noexcept { _msgout = std::move(msg); } 
    std::suspend_always final_suspend() noexcept { return {}; }
    };

    using handle = std::coroutine_handle<promise_type>;
    handle corohdl{};

    explicit Chat(promise_type* p ) : corohdl{handle::from_promise(*p)} {}
    Chat(Chat && rhs) : corohdl{std::exchange(rhs.corohdl, nullptr)} {}
    ~Chat() { if (corohdl) corohdl.destroy(); }

    std::string listen() {
        if(!corohdl.done()) corohdl.resume();
        return std::move(corohdl.promise()._msgout);
    }

    void answer(std::string msg) {
        corohdl.promise()._msgin = msg;
        if(!corohdl.done()) corohdl.resume();
    }
};
    

Chat fun()
{
    co_yield "hello\n"s;
    std::cout << co_await std::string{};
    co_return "here\n"s;
}

void use()
{
    Chat chat = fun();
    std::cout << chat.listen();
    chat.answer("Where are you\n"s);
    std::cout << chat.listen();
}

int main()
{
    use();
    return 0;
}
