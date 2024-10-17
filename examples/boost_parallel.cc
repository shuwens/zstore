#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <vector>

boost::asio::awaitable<int> async_operation(int i) { co_return i * 2; }

boost::asio::awaitable<std::vector<int>>
run_in_parallel(boost::asio::io_context &io_context)
{
    std::vector<boost::asio::awaitable<int>> tasks;

    for (int i = 0; i < 5; ++i) {
        tasks.push_back(boost::asio::co_spawn(io_context, async_operation(i),
                                              boost::asio::use_awaitable));
    }

    std::vector<int> results;
    for (auto &task : tasks) {
        results.push_back(co_await task);
    }

    co_return results;
}

int main()
{
    boost::asio::io_context io_context;

    auto results = boost::asio::co_spawn(
        io_context, run_in_parallel(io_context), boost::asio::use_awaitable);

    io_context.run();

    for (auto result : results.get()) {
        std::cout << result << " ";
    }
    std::cout << std::endl;

    return 0;
}
