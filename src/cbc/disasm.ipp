#include <iostream>
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

struct stream_pos_t {};
constexpr stream_pos_t stream_pos;

template<typename T>
constexpr void Disassembler::print_it(const T arg, const char head_delim, const char tail_delim)
{
    if constexpr (::std::is_arithmetic_v<T> ||
        ::std::is_convertible_v<T, ::std::string_view>) {
        cout << head_delim << arg << tail_delim;
    }
    else if constexpr (::std::is_pointer_v<T>) {
        cout << head_delim << ::std::hex << (uint64_t)arg << ::std::dec << tail_delim;
    }
    else if constexpr (has_name_v<T>) {
        cout << head_delim << arg.name() << tail_delim;
    }
    else if constexpr (name_conformal_v<T>) {
        cout << head_delim << name(arg) << tail_delim;
    }
    else {
        cout << head_delim << "<cannot name given type>" << tail_delim;
    }
}

template<typename T, typename... Ts>
constexpr void Disassembler::print_concat(const T arg, const Ts... tail)
{
    if constexpr (sizeof...(Ts) > 0) {
        if constexpr (::std::is_same_v<T, concat_t>) {
            print_concat(tail...);
        }
        else if constexpr (::std::is_same_v<T, stream_pos_t>) {
            cout << std::setw(log10size) << (uint64_t)CurrentOffset() << ::std::setw(0) << ':';
            print(tail...);
        }
        else {
            print_it(arg);
            print(tail...);
        }
    }
    else {
        print_it(arg, '\0', '\n');
    }
}

template<typename T, typename... Ts>
constexpr void Disassembler::print(const T arg, const Ts... tail)
{
    if constexpr (sizeof...(Ts) > 0) {
        if constexpr (::std::is_same_v<T, concat_t>) {
            print_concat(tail...);
        }
        else if constexpr (::std::is_same_v<T, stream_pos_t>) {
            cout << std::setw(log10size) << (uint64_t)CurrentOffset() << ::std::setw(0) << ':';
            print(tail...);
        }
        else {
            print_it(arg, ' ');
            print(tail...);
        }
    }
    else {
        print_it(arg, ' ', '\n');
    }
}

}; // namespace Cbc
