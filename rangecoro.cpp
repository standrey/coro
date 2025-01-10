#include <coroutine>
#include <iostream>
#include <string>
#include <print>
#include <utility>
#include <vector>
#include <algorithm>

using namespace std::string_literals;

struct Generator {

    struct promise_type {
        int value{};

        void unhandled_exception() noexcept {}
        Generator get_return_object() { return Generator{this}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int msg) noexcept {
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
    
    struct sentinel{};
    struct iterator {
        handle corohdl{};
        bool operator == (sentinel) const { return corohdl.done(); }
        iterator &  operator ++() { corohdl.resume(); return *this; }
        int operator*() const { return corohdl.promise().value; }
    };

    iterator begin() { return {corohdl}; }
    sentinel end() { return {}; }
};


template<typename T>
Generator interleave(T a, T b) {
    auto l = [](T & v) -> Generator {
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
    using IntVector = std::vector<int>;
    IntVector mainv{1,2,3,4,5,6,7,8};
    auto middle_iter{mainv.begin()};
    std::advance(middle_iter, mainv.size()/2);
    IntVector left_half(mainv.begin(), middle_iter);
    IntVector right_half(middle_iter, mainv.end());

    Generator g{interleave(left_half, right_half)};
    for(const auto & v : g) { std::cout<< v << '\n'; } ;
    return 0;
}
