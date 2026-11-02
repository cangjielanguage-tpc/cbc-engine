#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Utils {

class SharedObject {
public:
    struct SymbolSearchResult {
        void* symbol;
        std::optional<std::string> error;
    };

    SharedObject(SharedObject const&) = delete;
    SharedObject(SharedObject&& another);
    SharedObject();

    ~SharedObject();

    SharedObject& operator=(SharedObject&& other);
    SharedObject& operator=(SharedObject const& other) = delete;

    bool IsOpened() const;
    static SharedObject Open(std::string_view str);
    static SharedObject Open(std::string&& str);
    static SharedObject Open(const char* str);

    static SharedObject OpenCurrentExecutable();

    static SharedObject OpenLibc();

    // Searches for the symbol named `str` in given shared object.
    // returns the symbol address and an owned error message, if lookup failed
    // a null symbol address alone does not imply a lookup error
    SymbolSearchResult SearchSym(char const* str) const;

    std::string const& Name() const;

private:
    SharedObject(void* handle, std::string&& name);
    void* handle;
    std::string name;
};

} // namespace Utils
