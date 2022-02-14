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

#ifndef IPCGULL_FUNCTION_H
#define IPCGULL_FUNCTION_H

#include <cassert>
#include <functional>
#include <ipcgull/variant.h>

namespace ipcgull {
    namespace {
        template<typename T, template<typename...> class Base>
        struct is_specialization : std::false_type {};

        template<template<typename...> class Base, typename... Args>
        struct is_specialization<Base<Args...>, Base>: std::true_type {};

        template <typename R, typename... Args>
        struct _fn_generator {
            [[maybe_unused]]
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<R(Args...)> f) {
                return [func=std::move(f)]
                        (const variant_tuple& args)->variant_tuple {
                    std::vector<variant> ret {to_variant(
                            std::apply(func,
                                       from_variant<std::tuple<Args...>>(
                                               args)))};
                    return ret;
                };
            }
        };

        template <typename R>
        struct _fn_generator<R> {
            [[maybe_unused]]
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<R()> f) {
                return [func=std::move(f)]
                        (const variant_tuple& args)->variant_tuple {
                    if(!args.empty())
                        throw std::bad_variant_access();
                    std::vector<variant> ret {to_variant(func())};
                    return ret;
                };
            }
        };

        template <typename... R, typename... Args>
        struct _fn_generator<std::tuple<R...>, Args...> {
            [[maybe_unused]]
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<std::tuple<R...>(Args...)> f) {
                return [func=std::move(f)]
                        (const variant_tuple& args)->variant_tuple {
                    auto ret = to_variant(std::apply(
                            func, from_variant<std::tuple<Args...>>(args)));
                    assert(std::holds_alternative<variant_tuple>(ret));
                    return std::get<variant_tuple>(ret);
                };
            }
        };

        template <typename... R>
        struct _fn_generator<std::tuple<R...>> {
            [[maybe_unused]]
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<std::tuple<R...>()> f) {
                return [func=std::move(f)](const variant_tuple& args) {
                    if(!args.empty())
                        throw std::bad_variant_access();
                    auto ret = to_variant(func());
                    assert(std::holds_alternative<variant_tuple>(ret));
                    return std::get<variant_tuple>(ret);
                };
            }
        };

        template <typename... Args>
        struct _fn_generator<void, Args...> {
            [[maybe_unused]]
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<void(Args...)> f) {
                return [func=std::move(f)]
                        (const variant_tuple& args)->variant_tuple {
                    std::apply(func, from_variant<std::tuple<Args...>>(args));
                    return {};
                };
            }
        };

        template <>
        struct _fn_generator<void> {
            static std::function<variant_tuple(variant_tuple)> make_fn(
                    std::function<void()> f) {
                return [func=std::move(f)]
                        (const variant_tuple& args)->variant_tuple {
                    if(!args.empty())
                        throw std::bad_variant_access();
                    func();
                    return {};
                };
            }
        };
    }

    class function
    {
    private:
        std::function<variant_tuple(const variant_tuple&)> _f;
        std::vector<std::string> _arg_names;
        std::vector<variant_type> _arg_types;
        std::vector<std::string> _return_names;
        std::vector<variant_type> _return_types;
    public:
        function() = delete;

        template <typename... R, typename... Args>
        function(const std::function<std::tuple<R...>(Args...)>& f,
                const std::array<std::string, sizeof...(Args)>& arg_names,
                const std::array<std::string, sizeof...(R)>& return_names) :
                _f (_fn_generator<std::tuple<R...>, Args...>::make_fn(f)),
                _arg_names (arg_names.begin(), arg_names.end()),
                _arg_types({make_variant_type<Args>()...}),
                _return_names (return_names.begin(), return_names.end()),
                _return_types({make_variant_type<R>()...}) {
        }

        template <typename... R, typename... Args>
        function(std::tuple<R...>(*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                 function(std::function<std::tuple<R...>(Args...)>(f),
                         arg_names, return_names) {
        }

        template <typename T, typename... R, typename... Args>
        function(T* t, std::tuple<R...>(T::*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                 function(std::function<std::tuple<R...>(Args...)>(
                        [t, f](Args... args)->std::tuple<R...> {
                            return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename T, typename... R, typename... Args>
        function(T* t, std::tuple<R...>(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                function(std::function<std::tuple<R...>(Args...)>(
                        [t, f](Args... args)->std::tuple<R...> {
                            return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename T, typename... R, typename... Args>
        function(const T* t, std::tuple<R...>(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                function(std::function<std::tuple<R...>(Args...)>(
                        [t, f](Args... args)->std::tuple<R...> {
                            return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename... R>
        function(const std::function<std::tuple<R...>()>& f,
                const std::array<std::string, sizeof...(R)>& return_names) :
                _f (_fn_generator<std::tuple<R...>>::make_fn(f)),
                _return_names (return_names.begin(), return_names.end()),
                _return_types ({make_variant_type<R>()...}) { }

        template <typename... R>
        function(std::tuple<R...>(*f)(),
                 const std::array<std::string, sizeof...(R)>& return_names) :
                 function(std::function<std::tuple<R...>()>(f), return_names) {
        }

        template <typename T, typename... R>
        function(T* t, std::tuple<R...>(T::*f)(),
                 const std::array<std::string, sizeof...(R)>& return_names) :
                 function(std::function<std::tuple<R...>()>(
                        [t, f]()->std::tuple<R...> {
                            return (t->*f)(); }),
                         return_names) {
        }

        template <typename T, typename... R>
        function(T* t, std::tuple<R...>(T::*f)() const,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                function(std::function<std::tuple<R...>()>(
                        [t, f]()->std::tuple<R...> {
                            return (t->*f)(); }),
                         return_names) {
        }

        template <typename T, typename... R>
        function(const T* t, std::tuple<R...>(T::*f)() const,
                 const std::array<std::string, sizeof...(R)>& return_names) :
                function(std::function<std::tuple<R...>()>(
                        [t, f]()->std::tuple<R...> {
                            return (t->*f)(); }),
                         return_names) {
        }

        template <typename R, typename... Args>
        function(const std::function<R(Args...)>& f,
                const std::array<std::string, sizeof...(Args)>& arg_names,
                const std::array<std::string, 1>& return_names) :
                _f (_fn_generator<R, Args...>::make_fn(f)),
                _arg_names (arg_names.begin(), arg_names.end()),
                _arg_types({make_variant_type<Args>()...}),
                _return_names(return_names.begin(), return_names.end()),
                _return_types({make_variant_type<R>()}) {
            static_assert(!is_specialization<R, std::tuple>::value,
                    "Invalid function construction for tuple return type");
            static_assert(!std::is_same<R, void>::value,
                          "Invalid return name for void return type");
        }

        template <typename R, typename... Args>
        function(R(*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R(Args...)>(f),
                         arg_names, return_names) {
        }

        template <typename T, typename R, typename... Args>
        function(T* t, R(T::*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R(Args...)>(
                        [t, f](Args... args)->R { return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename T, typename R, typename... Args>
        function(T* t, R(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R(Args...)>(
                        [t, f](Args... args)->R { return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename T, typename R, typename... Args>
        function(const T* t, R(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R(Args...)>(
                        [t, f](Args... args)->R { return (t->*f)(args...); }),
                         arg_names, return_names) {
        }

        template <typename R>
        function(const std::function<R()>& f,
                 const std::array<std::string, 1>& return_names) :
                _f (_fn_generator<R>::make_fn(f)),
                _return_names(return_names.begin(), return_names.end()),
                _return_types ({make_variant_type<R>()}) {
            static_assert(!is_specialization<R, std::tuple>::value,
                    "Invalid function construction for tuple return type");
            static_assert(!std::is_same<R, void>::value,
                    "Invalid return name for void return type");
        }

        template <typename R>
        function(R(*f)(),
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R()>(f),arg_names, return_names) {
        }

        template <typename T, typename R>
        function(T* t, R(T::*f)(),
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R()>([t, f]()->R { return (t->*f)(); }),
                         {}, return_names) {
        }

        template <typename T, typename R>
        function(T* t, R(T::*f)() const,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R()>([t, f]()->R { return (t->*f)(); }),
                         {}, return_names) {
        }

        template <typename T, typename R>
        function(const T* t, R(T::*f)() const,
                 const std::array<std::string, 1>& return_names) :
                function(std::function<R()>([t, f]()->R { return (t->*f)(); }),
                         {}, return_names) {
        }

        template <typename... Args>
        function(const std::function<void(Args...)>& f,
                const std::array<std::string, sizeof...(Args)>& arg_names) :
                _f (_fn_generator<void, Args...>::make_fn(f)),
                _arg_names (arg_names.begin(), arg_names.end()),
                _arg_types({make_variant_type<Args>()...}) { }

        template <typename... Args>
        function(void(*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names) :
                function(std::function<void(Args...)>(f),
                         arg_names) {
        }

        template <typename T, typename... Args>
        function(T* t, void(T::*f)(Args...),
                 const std::array<std::string, sizeof...(Args)>& arg_names) :
                function(std::function<void(Args...)>(
                        [t, f](Args... args) { (t->*f)(args...); }),
                         arg_names) {
        }

        template <typename T, typename... Args>
        function(T* t, void(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names) :
                function(std::function<void(Args...)>(
                        [t, f](Args... args) { (t->*f)(args...); }),
                         arg_names) {
        }

        template <typename T, typename... Args>
        function(const T* t, void(T::*f)(Args...) const,
                 const std::array<std::string, sizeof...(Args)>& arg_names) :
                function(std::function<void(Args...)>(
                        [t, f](Args... args) { (t->*f)(args...); }),
                         arg_names) {
        }

        function(const std::function<void()>& f) :
                _f (_fn_generator<void>::make_fn(f)) { }

        function(void(*f)()) : function(std::function<void()>(f)) { }

        template <typename T>
        function(T* t, void(T::*f)()) :
            function(std::function<void()>([t, f]() { (t->*f)(); })) { }

        template <typename T>
        function(T* t, void(T::*f)() const) :
                function(std::function<void()>([t, f]() { (t->*f)(); })) { }

        template <typename T>
        function(const T* t, void(T::*f)() const) :
                function(std::function<void()>([t, f]() { (t->*f)(); })) { }

        variant_tuple operator()(const variant_tuple& args) const;

        [[nodiscard]] const std::vector<std::string>& arg_names() const;
        [[nodiscard]] const std::vector<variant_type>& arg_types() const;
        [[nodiscard]] const std::vector<std::string>& return_names() const;
        [[nodiscard]] const std::vector<variant_type>& return_types() const;
    };
}

#endif //IPCGULL_FUNCTION_H
