#include "cbc/disasm.h"
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <type_traits>

namespace Cbc {

using ::std::cout;

template<typename T, typename I = void>
struct has_name : ::std::false_type {};

template<typename T>
struct has_name<T, ::std::void_t<decltype(::std::declval<T>().name())>> : ::std::true_type {};

template<typename T>
constexpr bool has_name_v = has_name<T>::value;

template<typename T, typename I = void>
struct name_conformal : ::std::false_type {};

template<typename T>
struct name_conformal<T, ::std::void_t<decltype(name(::std::declval<T>()))>> : ::std::true_type {};

template<typename T>
constexpr bool name_conformal_v = name_conformal<T>::value;

struct concat_t {};
constexpr concat_t concat;

template<typename T>
constexpr void Disassembler::print_it(const T arg, const char delim)
{
    if constexpr (::std::is_arithmetic_v<T> ||
        ::std::is_convertible_v<T, ::std::string_view>)
    {
        cout << arg << delim;
    }
    else if constexpr (::std::is_pointer_v<T>)
    {
        cout << ::std::hex << (uint64_t)arg << ::std::dec << delim;
    }
    else if constexpr (has_name_v<T>)
    {
        cout << arg.name() << delim;
    }
    else if constexpr (name_conformal_v<T>)
    {
        cout << name(arg) << delim;
    }
    else
    {
        cout << "<cannot name given type>" << delim;
    }
}

template<typename T, typename... Ts>
constexpr void Disassembler::print0(const T arg, const Ts... tail)
{
    if constexpr (sizeof...(Ts) > 0)
    {
        if constexpr (::std::is_same_v<T, concat_t>) {
            // print_it(arg, '\0');
            print0(tail...);
        }
        else
        {
            print_it(arg, ' ');
            print0(tail...);
        }
    }
    else
    {
        print_it(arg, '\n');
    }
}

template<typename T, typename... Ts>
constexpr void Disassembler::print(const T arg, const Ts... tail)
{
    cout << std::setw(log10size) << (uint64_t)CurrentOffset() << ": ";
    print0(arg, tail...);
}

}; // namespace Cbc
