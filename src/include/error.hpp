
#pragma once

#include <iostream>

namespace kym
{

enum class StatusCode {
    /// Not an error; returned on success.
    Ok = 0,
    Unknown = 1,

    // System error
    Internal = 500,
    NotImplemented = 501,

    // User error
    InvalidArgument = 400,
    NotFound = 404,
    RateLimit = 420,

};

class Status
{
  public:
    Status() = default;

    explicit Status(StatusCode status_code) : code_(status_code) {}
    explicit Status(StatusCode status_code, std::string message)
        : code_(status_code), message_(std::move(message))
    {
    }

    bool ok() const { return code_ == StatusCode::Ok; }
    explicit operator bool() const { return ok(); }

    StatusCode code() const { return code_; }
    std::string const &message() const { return message_; }

    Status Wrap(std::string message)
    {
        return Status(this->code_, message + "\n" + this->message_);
    };

  private:
    StatusCode code_{StatusCode::Ok};
    std::string message_;
};

inline std::ostream &operator<<(std::ostream &os, Status const &status)
{
    if (status.message().empty()) {
        return os << "Statuscode " << (int)status.code();
    }
    return os << status.message();
}

template <typename T> class StatusOr
{
  public:
    StatusOr() : StatusOr(Status(StatusCode::Unknown, "default")) {}

    StatusOr(Status status) : status_(std::move(status)) {}

    StatusOr(T value) : value_(std::move(value)) {}

    T value() { return std::move(value_); }

    bool ok() const { return status_.ok(); }
    explicit operator bool() const { return status_.ok(); }

    /**
     * @name Status accessors.
     *
     * @return All these member functions return the (properly ref and
     *     const-qualified) status. If the object contains a value then
     *     `status().ok() == true`.
     */
    Status &status() & { return status_; }
    Status const &status() const & { return status_; }
    Status &&status() && { return std::move(status_); }

  private:
    Status status_;
    T value_;
};

} // namespace kym
