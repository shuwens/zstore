#pragma once
#include <boost/outcome.hpp>
#include <boost/stacktrace.hpp>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#ifndef LOGLV
#define LOGLV 1
#endif

// I do miss Rust
namespace outcome = boost::outcome_v2;
template <typename T> using Result = outcome::result<T>;
template <typename T> using Option = std::optional<T>;

template <typename T> using sptr = std::shared_ptr<T>;
template <typename T> using uptr = std::unique_ptr<T>;
template <typename T> using vec = std::vector<T>;
// template <typename T> using fvec = folly::fbvector<T>;

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;
using s64 = int64_t;
using s32 = int32_t;
using s16 = int16_t;
using s8 = int8_t;
using usize = size_t;
using ssize = ssize_t;
using byte = std::byte;
using str = std::string;
// using fspath = std::filesystem::path;

#define log_trace(MSG, ...)                                                    \
    do {                                                                       \
        if (LOGLV <= 0)                                                        \
            fmt::print(stderr, "[TRC {}:{} {}] " MSG "\n", __FILE__, __LINE__, \
                       __func__, ##__VA_ARGS__);                               \
    } while (0)

#define log_debug(MSG, ...)                                                    \
    do {                                                                       \
        if (LOGLV <= 1)                                                        \
            fmt::print(stderr, fg(fmt::terminal_color::magenta),               \
                       "[DBG {}:{} {}] " MSG "\n", __FILE__, __LINE__,         \
                       __func__, ##__VA_ARGS__);                               \
    } while (0)

#define log_info(MSG, ...)                                                     \
    do {                                                                       \
        if (LOGLV <= 2)                                                        \
            fmt::print(stderr,                                                 \
                       fg(fmt::terminal_color::cyan) | fmt::emphasis::bold,    \
                       "[INFO {}:{} {}] " MSG "\n", __FILE__, __LINE__,        \
                       __func__, ##__VA_ARGS__);                               \
    } while (0)

#define log_error(MSG, ...)                                                    \
    do {                                                                       \
        fmt::print(stderr, fg(fmt::terminal_color::red) | fmt::emphasis::bold, \
                   "[ERR {}:{} {}] " MSG "\n", __FILE__, __LINE__, __func__,   \
                   ##__VA_ARGS__);                                             \
    } while (0)

#define log_warn(MSG, ...)                                                     \
    do {                                                                       \
        if (LOGLV <= 3)                                                        \
            fmt::print(stderr,                                                 \
                       fg(fmt::terminal_color::yellow) | fmt::emphasis::bold,  \
                       "[WARN {}:{} {}] " MSG "\n", __FILE__, __LINE__,        \
                       __func__, ##__VA_ARGS__);                               \
    } while (0)

#define trap_to_debugger()                                                     \
    do {                                                                       \
        raise(SIGTRAP);

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

// Boost error handling
template <typename T> inline auto errcode_to_result(int rc) -> Result<T>
{
    if (rc > 0)
        return outcome::failure(std::error_code(rc, std::generic_category()));
    return outcome::success();
}

enum class ZstoreError {
    Success = 0, // 0 should not represent an error
    ValueNotExisting,
    IoError,
    // InvalidObjectType,
    // MissingData,
};

namespace std
{
template <> struct is_error_code_enum<ZstoreError> : true_type {
};
}; // namespace std

// namespace detail
// {
// // Define a custom error code category derived from std::error_category
// class BackendError_category : public std::error_category
// {
//   public:
//     // Return a short descriptive name for the category
//     virtual const char *name() const noexcept override final
//     {
//         return "Data validity error";
//     }
//
//     // Return what each enum means in text
//     virtual std::string message(int c) const override final
//     {
//         switch (static_cast<LsvdError>(c)) {
//         case LsvdError::Success:
//             return "passed validity checks";
//         case LsvdError::InvalidMagic:
//             return "invalid magic number in the header";
//         case LsvdError::InvalidVersion:
//             return "version number unsupported";
//         case LsvdError::InvalidObjectType:
//             return "does not match required object type";
//         case LsvdError::MissingData:
//             return "required data not found";
//         }
//     }
// };
// } // namespace detail

// extern inline const detail::BackendError_category &BackendError_category()
// {
//     static detail::BackendError_category c;
//     return c;
// }

// inline std::error_code make_error_code(ZstoreError e)
// {
//     static detail::BackendError_category c;
//     return {static_cast<int>(e), c};
// }

inline int result_to_rc(Result<void> res)
{
    if (res.has_value())
        return 0;
    else
        return res.error().value();
}

inline int result_to_rc(Result<int> res)
{
    if (res.has_value())
        return res.value();
    else
        return res.error().value();
}

inline auto
tdiff_us(std::chrono::time_point<std::chrono::high_resolution_clock> end,
         std::chrono::time_point<std::chrono::high_resolution_clock> start)
{
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                  .count();
    return std::abs(us);
}

inline auto
tdiff_ns(std::chrono::time_point<std::chrono::high_resolution_clock> end,
         std::chrono::time_point<std::chrono::high_resolution_clock> start)
{
    auto us = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                  .count();
    return std::abs(us);
}
