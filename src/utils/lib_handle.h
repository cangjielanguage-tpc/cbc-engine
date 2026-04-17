#pragma once

#include <string>

using AotCodeAddr = void*;

struct LibHandle {
    void* handle;

    explicit LibHandle(std::string libName);
    ~LibHandle();

    LibHandle(const LibHandle&) = delete;
    LibHandle(LibHandle&& other) noexcept;

    LibHandle& operator=(const LibHandle&)  = delete;
    LibHandle& operator=(LibHandle&& other) = delete;

    operator void*() const { return handle; }

    AotCodeAddr FindTarget(std::string linkageName) const;
};
