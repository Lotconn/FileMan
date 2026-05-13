#ifndef SCOPED_ENUM_BITOPS_H
#define SCOPED_ENUM_BITOPS_H

#include <type_traits>

#define ENABLE_BITWISE_OPS_FOR_ENUM(E)                            \
static_assert( std::is_enum_v<E>, "enumeration type required" );        \
constexpr E operator|( E lhs, E rhs ) {                                 \
    using U = std::underlying_type_t<E>;                                \
    return static_cast<E>( static_cast<U>(lhs) | static_cast<U>(rhs) ); \
}                                                                       \
constexpr E operator&( E lhs, E rhs ) {                                 \
    using U = std::underlying_type_t<E>;                                \
    return static_cast<E>( static_cast<U>(lhs) & static_cast<U>(rhs) ); \
}                                                                       \
constexpr E operator^( E lhs, E rhs ) {                                 \
    using U = std::underlying_type_t<E>;                                \
    return static_cast<E>( static_cast<U>(lhs) ^ static_cast<U>(rhs) ); \
}                                                                       \
constexpr E operator~( E e ) {                                          \
    using U = std::underlying_type_t<E>;                                \
    return static_cast<E>( ~static_cast<U>( e ) );                      \
}                                                                       \
constexpr E &operator|=( E &lhs, E rhs ) {                              \
    return (lhs = lhs | rhs);                                           \
}                                                                       \
constexpr E &operator&=( E &lhs, E rhs ) {                              \
    return (lhs = lhs & rhs);                                           \
}                                                                       \
constexpr E &operator^=( E &lhs, E rhs ) {                              \
    return (lhs = lhs ^ rhs);                                           \
}                                                                       \
using type_to_eat_semicolon = int

#endif //SCOPED_ENUM_BITOPS_H