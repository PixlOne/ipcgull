/*
 * Copyright 2022 PixlOne
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef IPCGULL_VARIANT_H
#define IPCGULL_VARIANT_H

#include <any>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ipcgull {
    class object {
    public:
        virtual ~object() = default;
    };

    /// TODO: client_object that is a wrapper for the object path string?

    template<typename T, std::size_t k = 0>
    class _wrapper : public T {
    private:
        static constexpr int K = k;
    public:
        _wrapper() = default;

        template<typename... Args>
        _wrapper(Args... args) : T(std::forward<Args>(args)...) {}

        _wrapper(const _wrapper& o) = default;

        _wrapper(_wrapper&& o) noexcept = default;

        template<typename... Args>
        _wrapper& operator=(Args... args) {
            T::operator=(std::forward<Args>(args)...);
            return *this;
        }

        _wrapper& operator=(const _wrapper& o) = default;

        _wrapper& operator=(_wrapper&& o) noexcept = default;
    };

    /// TODO: Do we need Signature?
    //typedef _wrapper<std::string, 0> object_path;
    typedef _wrapper<std::string, 1> signature;

    template<typename T>
    using _variant = std::variant<
            int16_t,
            uint16_t,
            int32_t,
            uint32_t,
            int64_t,
            uint64_t,
            double,
            uint8_t,
            //object_path,
            std::shared_ptr<object>,
            signature,
            std::string,
            bool,
            std::vector<T>,
            _wrapper<std::vector<T>, 0>, // tuple
            std::map<T, T>
    >;

    template<template<typename> typename K>
    struct _y_comb : K<_y_comb<K>> {
    public:
        using K<_y_comb>::K;
    };

    typedef _y_comb<_variant> variant;
    typedef _wrapper<std::vector<variant>, 0> variant_tuple;

    template<typename A, typename B>
    struct and_type : and_type<typename A::type, B> {
    };

    template<typename B>
    struct and_type<std::false_type, B> : std::false_type {
    };

    template<typename B>
    struct and_type<std::true_type, B> : B {
    };

    template<typename T>
    struct variant_constructable : std::false_type {
    };

    template<>
    struct variant_constructable<int16_t> : std::true_type {
    };
    template<>
    struct variant_constructable<uint16_t> : std::true_type {
    };
    template<>
    struct variant_constructable<int32_t> : std::true_type {
    };
    template<>
    struct variant_constructable<uint32_t> : std::true_type {
    };
    template<>
    struct variant_constructable<int64_t> : std::true_type {
    };
    template<>
    struct variant_constructable<uint64_t> : std::true_type {
    };
    template<>
    struct variant_constructable<double> : std::true_type {
    };
    template<>
    struct variant_constructable<uint8_t> : std::true_type {
    };
    template<typename T>
    struct variant_constructable<std::shared_ptr<T>> :
            std::is_base_of<object, T> {
    };
    template<typename T>
    struct variant_constructable<std::shared_ptr<const T>> :
            std::is_base_of<object, T> {
    };
    template<>
    struct variant_constructable<signature> : std::true_type {
    };
    template<>
    struct variant_constructable<std::string> : std::true_type {
    };
    template<>
    struct variant_constructable<bool> : std::true_type {
    };
    template<typename T>
    struct variant_constructable<std::vector<T>> :
            variant_constructable<T> {
    };
    template<typename T, typename... Args>
    struct variant_constructable<std::tuple<T, Args...>> :
            and_type<variant_constructable<T>,
                    variant_constructable<std::tuple<Args...>>> {
    };
    template<typename T>
    struct variant_constructable<std::tuple<T>> :
            variant_constructable<T> {
    };
    template<typename K, typename V>
    struct variant_constructable<std::map<K, V>> :
            and_type<variant_constructable<K>,
                    variant_constructable<V>> {
    };

    template<typename T>
    struct variant_constructable<const T> : variant_constructable<T> {
    };

    template<typename T>
    struct _variant_helper {
        static T get(const variant& v) {
            return std::get<T>(v);
        }

        static variant make(const T& x) {
            return x;
        }
    };

    template<typename T>
    struct _variant_helper<std::vector<T>> {
        static std::vector<T> get(const variant& v) {
            const auto& vec = std::get<std::vector<variant>>(v);
            std::vector<T> ret(vec.size());
            for (std::size_t i = 0; i < vec.size(); ++i)
                ret[i] = _variant_helper<T>::get(vec[i]);

            return ret;
        }

        [[maybe_unused]]
        static variant make(const std::vector<T>& x) {
            std::vector<variant> ret(x.size());
            for (std::size_t i = 0; i < x.size(); ++i)
                ret[i] = _variant_helper<T>::make(x[i]);

            return ret;
        }
    };

    template<typename... Args>
    struct _variant_helper<std::tuple<Args...>> {
    private:
        template<std::size_t... S>
        static std::tuple<Args...> get(const variant_tuple& v,
                                       std::index_sequence<S...>) {
            return std::make_tuple<Args...>(
                    _variant_helper<typename std::tuple_element<S,
                            std::tuple<Args...> >::type>::get(v[S])...);
        }

        template<std::size_t... S>
        static variant_tuple make(const std::tuple<Args...>& x,
                                  std::index_sequence<S...>) {
            std::vector<variant> v =
                    {_variant_helper<Args>::make(std::get<S>(x))...};
            return v;
        }

    public:
        static std::tuple<Args...> get(const variant& v) {
            const auto& vec = std::get<variant_tuple>(v);
            if (vec.size() != sizeof...(Args))
                throw std::bad_variant_access();

            return get(vec, std::make_index_sequence<sizeof...(Args)>());
        }

        static variant make(const std::tuple<Args...>& x) {
            return make(x, std::make_index_sequence<sizeof...(Args)>());
        }
    };

    template<typename K, typename V>
    struct _variant_helper<std::map<K, V>> {
        static std::map<K, V> get(const variant& v) {
            std::map<K, V> ret;
            const auto& m = std::get<std::map<K, variant>>(v);
            for (const auto& i: m)
                ret.emplace(std::piecewise_construct,
                            std::forward_as_tuple(
                                    _variant_helper<K>::get(i.first)),
                            std::forward_as_tuple(
                                    _variant_helper<V>::get(i.second)));

            return ret;
        }

        static variant make(const std::map<K, V>& v) {
            std::map<variant, variant> ret;
            const auto& m = std::get<std::map<K, variant>>(v);
            for (const auto& i: m)
                ret.emplace(std::piecewise_construct,
                            std::forward_as_tuple(
                                    _variant_helper<K>::make(i.first)),
                            std::forward_as_tuple(
                                    _variant_helper<K>::make(i.second)));

            return ret;
        }
    };

    template<typename T>
    struct _variant_helper<std::shared_ptr<T>> {
        static std::shared_ptr<T> get(const variant& v) {
            static_assert(std::is_base_of<object, T>::value,
                          "T must be an ipcgull::object");
            return std::dynamic_pointer_cast<T>(
                    std::get<std::shared_ptr<object>>(v));
        }

        static variant make(const std::shared_ptr<object>& obj) {
            return obj;
        }
    };

    template<typename T>
    struct _normalize_type {
        typedef T type;
    };

    template<typename T>
    struct _normalize_type<const T> {
        typedef typename _normalize_type<T>::type type;
    };

    template<typename T>
    struct _normalize_type<T&> {
        typedef typename _normalize_type<T>::type type;
    };

    template<typename... Args>
    struct _normalize_type<std::tuple<Args...> > {
        typedef std::tuple<typename _normalize_type<Args>::type...> type;
    };

    template<typename T>
    struct _normalize_type<std::vector<T> > {
        typedef std::vector<typename _normalize_type<T>::type> type;
    };

    template<typename K, typename V>
    struct _normalize_type<std::map<K, V> > {
        typedef std::map<typename _normalize_type<K>::type,
                typename _normalize_type<V>::type> type;
    };

    template<typename T>
    struct _normalize_type<std::shared_ptr<T>> {
        static_assert(std::is_base_of<object, T>::value_type);
        typedef std::shared_ptr<object> type;
    };

    template<typename T>
    typename _normalize_type<T>::type from_variant(const variant& v) {
        return _variant_helper<typename _normalize_type<T>::type>::get(v);
    }

    template<typename T>
    variant to_variant(const T& t) {
        static_assert(variant_constructable<T>::value);
        return _variant_helper<T>::make(t);
    }

    // To be defined in implementation-specific files
    class variant_type {
    private:
        std::any data;
    public:
        // Default constructed variant_types are not valid
        variant_type();

        explicit variant_type(const std::type_info& primitive);

        [[maybe_unused]]
        static variant_type vector(const variant_type& t);

        [[maybe_unused]]
        static variant_type map(const variant_type& k, const variant_type& v);

        [[maybe_unused]]
        static variant_type tuple(const std::vector<variant_type>& types);

        [[maybe_unused]]
        static variant_type vector(variant_type&& t);

        [[maybe_unused]]
        static variant_type map(variant_type&& k, variant_type&& v);

        [[maybe_unused]]
        static variant_type tuple(std::vector<variant_type>&& types);

        ~variant_type();

        variant_type(const variant_type& o);

        [[maybe_unused]] [[maybe_unused]]
        variant_type(variant_type&& o) noexcept;

        variant_type& operator=(const variant_type& o);

        variant_type& operator=(variant_type&& o) noexcept;

        bool operator==(const variant_type& o) const;

        bool operator!=(const variant_type& o) const {
            return !operator==(o);
        }

        [[maybe_unused]] [[nodiscard]] bool valid() const;

        static variant_type from_internal(std::any&& x);

        [[nodiscard]] const std::any& raw_data() const;
    };

    template<typename T>
    struct _variant_type_helper {
        static variant_type make() {
            return variant_type(typeid(T));
        }
    };

    template<typename T>
    struct _variant_type_helper<const T> {
        static variant_type make() {
            return _variant_type_helper<T>::make();
        }
    };

    template<typename T>
    struct _variant_type_helper<T&> {
        static variant_type make() {
            return _variant_type_helper<T>::make();
        }
    };

    template<typename T>
    struct _variant_type_helper<std::vector<T>> {
        static variant_type make() {
            return variant_type::vector(_variant_type_helper<T>::make());
        }
    };

    template<typename... T>
    struct _variant_type_helper<std::tuple<T...>> {
        static variant_type make() {
            return variant_type::tuple(
                    {_variant_type_helper<T>::make()...});
        }
    };

    template<typename K, typename V>
    struct _variant_type_helper<std::map<K, V>> {
        static variant_type make() {
            return variant_type::map(
                    _variant_type_helper<K>::make(),
                    _variant_type_helper<V>::make()
            );
        }
    };

    template<typename T>
    struct _variant_type_helper<std::shared_ptr<T>> {
        static variant_type make() {
            static_assert(std::is_base_of<object, T>::value,
                          "T must be an ipcgull::object");
            return variant_type(typeid(std::shared_ptr<object>));
        }
    };

    template<typename T>
    variant_type make_variant_type() {
        return _variant_type_helper<T>::make();
    }
}

#endif //IPCGULL_VARIANT_H
