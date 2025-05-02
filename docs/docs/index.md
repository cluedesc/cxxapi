# Overview

**CXXAPI** is a modern, high-performance, asynchronous **C++20** (**optional C++23**) web framework and toolkit for building scalable HTTP APIs, servers, and network applications.
It leverages Boost.Asio coroutines, OpenSSL, fmt, nlohmann or Glaze json variant, providing a flexible, efficient, and extensible foundation.  

- **Easy to learn, with a simple and well-organized structure that makes understanding the server architecture straightforward.**
- **Inspired by the popular Python framework FastAPI, this project adopts a similar approach to create a minimalistic yet high-performance framework for building servers.**


**The project is currently in active development, with many essential features to be added in the near future. It will also be continuously improved and supplemented with comprehensive documentation. I welcome your support, feedback, and suggestions to help shape and enhance this promising project.**

## Features

- **Modern C++:** concepts, templates, constexpr, inline, and other solutions
- **Asynchronous core:** Boost.Asio with C++20 coroutines for non-blocking I/O

- **HTTP/1.1 support:** keep-alive, cookies, chunked transfer, streaming

- **Middleware pipeline:**
    - Built-in: CORS, **new ones will be added in the next releases**
    - Easy to extend with custom middleware

- **Advanced Logging:**
    - Async or sync version
    - Log levels, overflow strategies
    - Colorized output

- **Redis integration:** async client with c++20 coroutines
- **Postgres integration:** soon...