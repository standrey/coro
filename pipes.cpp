#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cassert>
#include <coroutine>
#include <iostream>

enum class IOp { READ, WRITE };

static constexpr int MAX_EXCLUSIVE_FD = 32;

using AsyncIOResult = std::pair<ssize_t, int>;

class Awaitable;

class Scheduler {
public: 
    Awaitable async_io(int fd, void * ptr, size_t len, IOp iop);
    Awaitable async_write(int fd, const void * ptr, size_t len);
    Awaitable async_read(int fd, void * ptr, size_t len);

    int pump_events();
    Awaitable * get_awaitables(int fd) const { return m_awaitables[fd]; }
    void push_awaitables(int fd, Awaitable * value) { m_awaitables[fd] = value; }
private:
    Awaitable* m_awaitables[MAX_EXCLUSIVE_FD] = {0};
};

class Awaitable {
public:
    bool await_ready() {
        std::cout << "await_ready\n";
        do { 
            errno = 0;
            ssize_t n = (m_iop == IOp::READ) ? read(m_fd, m_ptr, m_len) : write(m_fd, m_ptr, m_len);
            m_result = std::make_pair((n >= 0) ? n : 0, errno);
        } while (m_result.second == EINTR);
        return m_result.second != EAGAIN;
    }

    void await_suspend(std::coroutine_handle<> h) {
        std::cout << "await_suspend\n";
        m_cohandle = h;
        assert(m_scheduler->get_awaitables(m_fd) == nullptr);
        m_scheduler->push_awaitables(m_fd, this);
    }

    AsyncIOResult await_resume() {
        std::cout << "await_resume\n";
        return m_result; 
    }

    std::coroutine_handle<> retry() {
        return await_ready() ? m_cohandle : nullptr;
    }

public:
    Scheduler* m_scheduler;
    const int m_fd;
    void * m_ptr;
    const size_t m_len;
    const IOp m_iop;

    AsyncIOResult m_result;

    std::coroutine_handle<> m_cohandle;
};

Awaitable Scheduler::async_io(int fd, void * ptr, size_t len, IOp iop) {
    return Awaitable{
    .m_scheduler = this,
    .m_fd = fd,
    .m_ptr = ptr,
    .m_len = len,
    .m_iop = iop,
    .m_result = {},
    .m_cohandle = nullptr,
    };
}



Awaitable Scheduler::async_read(int fd, void *ptr, size_t len) {
    return async_io(fd, ptr, len, IOp::READ);
}

Awaitable Scheduler::async_write(int fd, const void * ptr, size_t len) {
    return async_io(fd, const_cast<void*>(ptr), len, IOp::WRITE);
}

int Scheduler::pump_events() {
    pollfd polls[MAX_EXCLUSIVE_FD];
    int num_p = 0;
    for (int fd = 0; fd < MAX_EXCLUSIVE_FD; ++fd) {
        if (m_awaitables[fd] == nullptr) {
            continue;
        }
        polls[num_p].fd = fd;
        polls[num_p].events = (m_awaitables[fd]->m_iop == IOp::READ) ? POLLIN : POLLOUT;
        polls[num_p].revents = 0;
        num_p++;
    }

    if (poll(polls, num_p, -1) < 0) {
        return (errno != EINTR) ? errno : 0;
    }

    std::coroutine_handle<> cohandles[MAX_EXCLUSIVE_FD];
    int num_c = 0;
    for (int i = 0; i < num_p; ++i) {
        if (polls[i].revents == 0) {
            continue;
        }
        int fd = polls[i].fd;
        Awaitable* awaitable = m_awaitables[fd];
        if (!awaitable) {
            continue;
        }
        std::coroutine_handle<> cohandle= awaitable->retry();
        if(!cohandle) {
            continue;
        }
        
        m_awaitables[fd] = nullptr;
        cohandles[num_c++] = cohandle;
    }

    for (int i = 0; i< num_c; ++i) {
        cohandles[i].resume();
    }
    return 0;
}

class Coro { 
public:
    class promise_type {
    public :
        Coro get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { };
    };
};

Coro fizz(Scheduler * scheduler, const int fizz_pipe_end) {
    while(true) {
        co_await scheduler->async_write(fizz_pipe_end, "Tick1", 5);
        co_await scheduler->async_write(fizz_pipe_end, "Tick1", 5);
        co_await scheduler->async_write(fizz_pipe_end, "Fizz", 4);
    }
}

Coro buzz(Scheduler * scheduler, const int buzz_pipe_end) {
    while(true) {
        co_await scheduler->async_write(buzz_pipe_end, "Tock1", 5);
        co_await scheduler->async_write(buzz_pipe_end, "Tock2", 5);
        co_await scheduler->async_write(buzz_pipe_end, "Tock3", 5);
        co_await scheduler->async_write(buzz_pipe_end, "Tock4", 5);
        co_await scheduler->async_write(buzz_pipe_end, "Buzz", 4);
    }
}

Coro consume(Scheduler * scheduler, bool *done, const int fizz_pipe_end, const int buzz_pipe_end, const int timerfd) {
    static constexpr int stdout_fd = 1;
    int iteration = 1;
    char buf[64];
    while (true) { 
        size_t num_timer_events;
        co_await scheduler->async_read(timerfd, &num_timer_events, sizeof(num_timer_events));
        while (num_timer_events--) {
            bool fizzy_buzzy = false;
            AsyncIOResult fizz_result = co_await scheduler->async_read(fizz_pipe_end, buf, sizeof(buf));
            if (fizz_result.first == 4) {
                fizzy_buzzy = true;
                write(stdout_fd, buf, 4);
            }

            AsyncIOResult buzz_result = co_await scheduler->async_read(buzz_pipe_end, buf, sizeof(buf));
            if (buzz_result.first == 4) {
                fizzy_buzzy = true;
                write(stdout_fd, buf, 4);
            }

            if(!fizzy_buzzy) {
                if (size_t n = snprintf(buf, sizeof(buf), "%d", iteration); n < sizeof(buf)) {
                    write(stdout_fd, buf, n);
                }
            }
        }

        write(stdout_fd, "\n", 1);

        if (iteration++ == 20) {
            *done=true;
            co_return;
        }
    }
}

int main() {
    int fizz_pipe_fds[2];
    if (pipe2(fizz_pipe_fds, O_DIRECT | O_NONBLOCK) < 0) {
        std::cerr<< "fizz pipe2 call failed\n";
        return errno;
    }

    assert(fizz_pipe_fds[0] < MAX_EXCLUSIVE_FD);
    assert(fizz_pipe_fds[1] < MAX_EXCLUSIVE_FD);

    int buzz_pipe_fds[2];
    if (pipe2(buzz_pipe_fds, O_DIRECT | O_NONBLOCK) < 0) {
        std::cerr<< "buzz pipe2 call failed\n";
        return errno;
    }

    assert(buzz_pipe_fds[0] < MAX_EXCLUSIVE_FD);
    assert(buzz_pipe_fds[1] < MAX_EXCLUSIVE_FD);

    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd < 0) {
        std::cerr<< "timerfd_create call failed.\n";
        return errno;
    }
    assert(timerfd < MAX_EXCLUSIVE_FD);

    itimerspec t;
    t.it_value.tv_sec =0;
    t.it_value.tv_nsec = 100000000;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_nsec= 100000000;
    if(timerfd_settime(timerfd, 0, &t, nullptr) < 0) {
        std::cerr<<"timerfd_settime call failed.\n";
        return errno;
    }

    Scheduler s;
    bool done = false;
    fizz(&s, fizz_pipe_fds[1]);
    buzz(&s, buzz_pipe_fds[1]);
    consume(&s, &done, fizz_pipe_fds[0], buzz_pipe_fds[0], timerfd);

    while(!done) {
        if (int err = s.pump_events()) {
            return err;
        }
    }

    return 0;
}
