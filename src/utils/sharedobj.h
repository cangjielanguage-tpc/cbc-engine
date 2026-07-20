#pragma once

#include <string>
#include <string_view>

namespace Utils {

class SharedObject {
public:
    struct SymbolSearchResult {
        void* symbol;
        std::string error;
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
    static SharedObject FromExternalHandle(void* handle, std::string_view name);

    static SharedObject OpenLibc();

    // returns a symbol address or a null pointer with an error message
    SymbolSearchResult SearchSym(char const* str) const;

    std::string const& Name() const;

private:
    SharedObject(void* handle, std::string&& name, bool ownsHandle = true);
    void* handle;
    std::string name;
    bool ownsHandle;
};

} // namespace Utils
