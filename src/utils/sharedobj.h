
#include <string>
#include <string_view>

namespace Utils {

class SharedObject {
public:
    SharedObject(SharedObject const&) = delete;
    SharedObject(SharedObject&& another);
    SharedObject();

    ~SharedObject();

    SharedObject& operator=(SharedObject&& other);
    SharedObject& operator=(SharedObject const& other) = delete;

    bool IsOpened() const;
    static SharedObject Open(std::string_view str);
    static SharedObject Open(std::string&& str);

    static SharedObject OpenCurrentExecutable();
    static SharedObject FromExternalHandle(void* handle, std::string_view name);

    // Searches for the symbol named `str` in given shared object.
    // Returns null on error.
    void* SearchSym(char const* str) const;

    std::string const& Name() const;

private:
    SharedObject(void* handle, std::string&& name, bool ownsHandle = true);
    void* handle;
    std::string name;
    bool ownsHandle;
};

} // namespace Utils
