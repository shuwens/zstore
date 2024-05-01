#pragma once

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

/**
 * Check return values of libstdc functions. If it's -1, print the error and
 * throw an exception
 */
#define check_ret_errno(ret, MSG, ...)                                         \
    do {                                                                       \
        if (ret == -1) {                                                       \
            auto en = errno;                                                   \
            auto fs = fmt::format(MSG "\n", ##__VA_ARGS__);                    \
            auto s = fmt::format("[ERR {}:{} {} | errno {}/{}] {}", __FILE__,  \
                                 __LINE__, __func__, en, strerror(en), fs);    \
            fmt::print(stderr, fg(fmt::color::red) | fmt::emphasis::bold, s);  \
            throw std::runtime_error(fs);                                      \
        }                                                                      \
    } while (0)

/**
 * Check return values of functions that return -errno in the case of an error
 * and throw an exception
 */
#define check_ret_neg(ret, MSG, ...)                                           \
    do {                                                                       \
        if (ret < 0) {                                                         \
            auto en = -ret;                                                    \
            auto fs = fmt::format(MSG "\n", ##__VA_ARGS__);                    \
            auto s = fmt::format("[ERR {}:{} {} | errno {}/{}] {}", __FILE__,  \
                                 __LINE__, __func__, en, strerror(en), fs);    \
            fmt::print(stderr, fg(fmt::color::red) | fmt::emphasis::bold, s);  \
            throw std::runtime_error(fs);                                      \
        }                                                                      \
    } while (0)

#define check_cond(cond, MSG, ...)                                             \
    do {                                                                       \
        if (cond) {                                                            \
            auto s = fmt::format("[ERR {}:{} {}]: " MSG "\n", __FILE__,        \
                                 __LINE__, __func__, ##__VA_ARGS__);           \
            fmt::print(stderr, fg(fmt::color::red) | fmt::emphasis::bold, s);  \
            throw std::runtime_error(s);                                       \
        }                                                                      \
    } while (0)
