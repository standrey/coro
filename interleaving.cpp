#include <coroutine>
#include <iostream>
#include <string>
#include <print>
#include <utility>
#include <vector>

using namespace std::string_literals;

struct Generator {

    struct promise_type {
        char value{};

        void unhandled_exception() noexcept {}
        Generator get_return_object() { return Generator{this}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always yield_value(auto msg) noexcept {
            value = std::move(msg);
            return {};
        }
        std::suspend_always final_suspend() noexcept { return {}; }
    };

    using handle = std::coroutine_handle<promise_type>;
    handle corohdl{};
    explicit Generator(promise_type* p ) : corohdl{handle::from_promise(*p)} {}
    Generator(Generator && rhs) : corohdl{std::exchange(rhs.corohdl, nullptr)} {}
    ~Generator() { if (corohdl) corohdl.destroy(); }
    bool is_done() const { return corohdl.done(); }
    auto value() const { return std::move(corohdl.promise().value); }
    void resume() { if(!corohdl.done()) corohdl.resume(); }
};
    

Generator interleave(std::vector<char> a, std::vector<char> b) {
    auto l = [](std::vector<char> v) -> Generator {
        for (const auto & x : v) co_yield x;
    };

    auto agen = l(a);
    auto bgen = l(b);

    while (!agen.is_done() || !bgen.is_done()) {
        
        if (!agen.is_done()) {
            co_yield agen.value();
            agen.resume();
        }

        if (!bgen.is_done()) {
            co_yield bgen.value();
            bgen.resume();
        }
    }
}

int main()
{
    Generator g{interleave({1,3,5,7}, {2,4,6,8})};
    while (!g.is_done()) {
        std::cout << g.value() << "\n";
        g.resume();
    }
    return 0;
}
