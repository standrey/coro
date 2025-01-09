#include <array>
#include <iostream>
#include <memory>
#include <boost/asio.hpp>

namespace ProxyBoostAsio {

struct proxy_state {
    proxy_state(boost::asio::ip::tcp::socket client) : client(std::move(client)) {}
    boost::asio::ip::tcp::socket client;
    boost::asio::ip::tcp::socket server{client.get_executor()};

};

using proxy_state_ptr = std::shared_ptr<proxy_state>;

boost::asio::awaitable<void> server_to_client(proxy_state_ptr state){
    std::array<char, 1024> data;
    for(;;) {
        auto [e1, n1] = co_await state->server.async_read_some(boost::asio::buffer(data), boost::asio::as_tuple(boost::asio::use_awaitable));
        if (e1) break;
        auto [e2, n2] = co_await async_write(state->client, boost::asio::buffer(data, n1), boost::asio::as_tuple(boost::asio::use_awaitable));
        if (e2) break;
    }
    state->client.close();
    state->server.close();
}

boost::asio::awaitable<void> client_to_server(proxy_state_ptr state){
    std::array<char, 1024> data;
    for(;;) {
        auto [e1, n1] = co_await state->client.async_read_some(boost::asio::buffer(data), boost::asio::as_tuple(boost::asio::use_awaitable));
        if (e1) break;
        auto [e2, n2] = co_await async_write(state->server, boost::asio::buffer(data, n1), boost::asio::as_tuple(boost::asio::use_awaitable));
        if (e2) break;
    }
    state->client.close();
    state->server.close();
}

boost::asio::awaitable<void> proxy(boost::asio::ip::tcp::socket client, boost::asio::ip::tcp::endpoint target) {
    auto state = std::make_shared<proxy_state>(std::move(client));
    auto [e] = co_await state->server.async_connect(target, boost::asio::as_tuple(boost::asio::use_awaitable));
    if (!e) {
        auto ex = state->client.get_executor();
        co_spawn(ex, client_to_server(state), boost::asio::detached);
        co_await server_to_client(state);
    }
}

boost::asio::awaitable<void> listen(boost::asio::ip::tcp::acceptor & acceptor, boost::asio::ip::tcp::endpoint target) {
    for(;;) {
        auto [e, client] = co_await acceptor.async_accept(boost::asio::as_tuple(boost::asio::use_awaitable));
        if (e) break;
        auto ex = client.get_executor();
        co_spawn(ex, proxy(std::move(client), target), boost::asio::detached);
    }
}
} // namespace ProxyBoostAsio
int main () {
    boost::asio::io_context ctx1;
    boost::asio::io_context ctx2;

    boost::asio::ip::tcp::acceptor acceptor(ctx1, {boost::asio::ip::tcp::v4(), 54545});
    co_spawn(ctx1, ProxyBoostAsio::listen(acceptor, *boost::asio::ip::tcp::resolver(ctx2).resolve("www.boost.org", "80")), boost::asio::detached);
    ctx2.run();
    ctx1.run();
}

