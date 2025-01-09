#include <coroutine>
#include <iostream> 
#include <optional>
#include <utility>

class Generator {
public:
    class promise_type {
    public:
        Generator get_return_object() {
            //std::cout << "get_return_object " << std::hex << this << std::endl;
            return Generator(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int value) noexcept {
            m_value = value;
            return {};
        }
        void unhandled_exception() {}
        void return_void() {}
        int m_value;
    };

public:
    std::optional<int> next() {
        //std::cout<< "next begin\n";
        if (!m_cohandle || m_cohandle.done()) {
            return std::nullopt;
        }
        m_cohandle.resume();
        if (m_cohandle.done()) {
            return std::nullopt;
        }
        //std::cout<< "next end " << m_cohandle.promise().m_value <<std::endl;
        return m_cohandle.promise().m_value;
    }

private:
    explicit Generator(const std::coroutine_handle<promise_type> cohandle) : m_cohandle(cohandle) {}
    std::coroutine_handle<promise_type> m_cohandle;

public:
    Generator(Generator && other) : m_cohandle{other.release_handle()} {
        //std::cout  << "move ctor\n";
    }

    Generator& operator=(Generator && other) {
        //std::cout << "move assign\n";

        if (this != &other) {
            if(m_cohandle) {
                m_cohandle.destroy();
            }
            m_cohandle = other.release_handle();
        }
        return *this;
    }

    ~Generator() {
        if (m_cohandle) {
            m_cohandle.destroy();
        }
    }

private:
    std::coroutine_handle<promise_type> release_handle() {
        return std::exchange(m_cohandle, nullptr);
    }
};

Generator source(int end) {
    for(int x = 2; x < end; ++x) {
        co_yield x;
    }
}

Generator filter(Generator g, int prime) {
    while(std::optional<int> optional_x = g.next()) {
        int x = optional_x.value();
        std::cout << "Original prime " << prime << " generated x " << x << std::endl;
        if ((x % prime) != 0) {
            co_yield x;
        }
    }
    std::cout << "filter for prime " << prime << " finished\n";
}

int main() {
    Generator g = source(20);
    while (std::optional<int> optional_prime = g.next()) {
        int prime = optional_prime.value();
        std::cout << "=================== prime " << prime << std::endl;
        g = filter(std::move(g), prime);

    }
return 0;
}


