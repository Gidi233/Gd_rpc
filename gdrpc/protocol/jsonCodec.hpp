
#ifndef _JSON_
#define _JSON_

#include <nlohmann/json.hpp>

#include <tuple>
#include <type_traits>

namespace gdrpc {
namespace rpc {
namespace detail {

template <typename Fn, typename Tuple, std::size_t... I>
inline constexpr void ForEachTuple(Tuple&& tuple, Fn&& fn,
                                   std::index_sequence<I...>) {
  using Expander = int[];
  (void)Expander{0, ((void)fn(std::get<I>(std::forward<Tuple>(tuple))), 0)...};
}

template <typename Fn, typename Tuple>
inline constexpr void ForEachTuple(Tuple&& tuple, Fn&& fn) {
  ForEachTuple(
      std::forward<Tuple>(tuple), std::forward<Fn>(fn),
      std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
}

template <typename T>
struct is_field_pointer : std::false_type {};

template <typename C, typename T>
struct is_field_pointer<T C::*> : std::true_type {};

template <typename T>
constexpr auto is_field_pointer_v = is_field_pointer<T>::value;

}  // namespace detail

template <typename T>
inline constexpr auto StructSchema() {
  return std::make_tuple();
}

#define DEFINE_STRUCT_SCHEMA(Struct, ...)                    \
  template <>                                                \
  inline constexpr auto gdrpc::rpc::StructSchema<Struct>() { \
    using _Struct = Struct;                                  \
    return std::make_tuple(__VA_ARGS__);                     \
  }

#define DEFINE_SAME_STRUCT_FIELD(FieldName) \
  DEFINE_STRUCT_FIELD(FieldName, #FieldName)

#define DEFINE_STRUCT_FIELD(StructField, FieldName) \
  std::make_tuple(&_Struct::StructField, FieldName)

template <typename T, typename Fn>
inline constexpr void ForEachField(T&& value, Fn&& fn) {
  constexpr auto struct_schema = StructSchema<std::decay_t<T>>();
  static_assert(std::tuple_size<decltype(struct_schema)>::value != 0,
                "StructSchema<T>() for type T should be specialized to return "
                "FieldSchema tuples, like ((&T::field, field_name), ...)");

  detail::ForEachTuple(struct_schema, [&value, &fn](auto&& field_schema) {
    using FieldSchema = std::decay_t<decltype(field_schema)>;
    static_assert(
        std::tuple_size<FieldSchema>::value >= 2 &&
            detail::is_field_pointer_v<std::tuple_element_t<0, FieldSchema>>,
        "FieldSchema tuple should be (&T::field, field_name)");

    fn(value.*(std::get<0>(std::forward<decltype(field_schema)>(field_schema))),
       std::get<1>(std::forward<decltype(field_schema)>(field_schema)));
  });
}

}  // namespace rpc
}  // namespace gdrpc
namespace {

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::unique_ptr<T>> : std::true_type {};

template <typename T>
constexpr bool is_optional_v = is_optional<std::decay_t<T>>::value;

template <typename T>
constexpr bool has_schema =
    std::tuple_size<decltype(gdrpc::rpc::StructSchema<T>())>::value;

}  // namespace
namespace nlohmann {

template <typename T>
struct adl_serializer<T, std::enable_if_t<has_schema<T>>> {
  template <typename BasicJsonType>
  static void to_json(BasicJsonType& j, const T& value) {
    gdrpc::rpc::ForEachField(
        value, [&j](auto&& field, auto&& name) { j[name] = field; });
  }

  template <typename BasicJsonType>
  static void from_json(const BasicJsonType& j, T& value) {
    gdrpc::rpc::ForEachField(value, [&j](auto&& field, auto&& name) {
      // ignore missing field of optional
      if (is_optional_v<decltype(field)> && j.find(name) == j.end()) return;

      j.at(name).get_to(field);
    });
  }
};

}  // namespace nlohmann
#endif  // _JSON_