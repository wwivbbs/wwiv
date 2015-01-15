#pragma once
#ifndef __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__
#define __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__

#include <exception>
#include <stdexcept>
#include <string>

namespace wwiv {
namespace net {

struct socket_error : public std::runtime_error {
socket_error(const std::string& message) : std::runtime_error(message) {}
};

struct socket_closed_error : public socket_error {
socket_closed_error(const std::string& message) : socket_error(message) {}
};

struct timeout_error : public socket_error {
  timeout_error(const std::string& message) : socket_error(message) {}
};

struct connection_error : public socket_error {
  connection_error(const std::string& host, int port);
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_SOCKET_EXCEPTIONS_H__
