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

#ifndef IPCGULL_PROPERTY_H
#define IPCGULL_PROPERTY_H

#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <ipcgull/variant.h>
#include <ipcgull/exception.h>

namespace ipcgull {
    namespace {
        template <typename T, typename Lock>
        static variant get_property(const std::shared_ptr<T>& data,
                                    const std::shared_ptr<Lock>& lock) {
            assert(lock); assert(data);
            std::lock_guard<Lock> guard(*lock);
            return to_variant(*data);
        }

        template <typename T>
        static variant get_property(const std::shared_ptr<T>& data) {
            assert(data);
            return to_variant(*data);
        }

        template <typename T>
        static bool validate_input(std::function<bool(const T&)> validate,
                                   const variant& input) {
            return validate(from_variant<T>(input));
        }

        template <typename T, typename Lock>
        static bool set_property(const std::shared_ptr<T>& data,
                                 const variant& input,
                                 const std::shared_ptr<Lock>& lock) {
            assert(lock); assert(data);
            std::lock_guard<Lock> guard(*lock);
            *data = from_variant<T>(input);
            return true;
        }

        template <typename T>
        static bool set_property(const std::shared_ptr<T>& data,
                                 const variant& input) {
            assert(data);
            *data = from_variant<T>(input);
            return true;
        }
    }

    class property {
    public:
        enum permission_mode : uint8_t {
            no_permissions = 0,
            readable = 0b01,
            writeable = 0b10,
            full = 0b11
        };
    private:
        const variant_type _type;
        permission_mode _perms;

        std::function<variant()> _get;
        std::function<bool(const variant&)> _validate;
        std::function<bool(const variant&)> _set;
    public:
        template <typename T>
        property(const std::shared_ptr<T>& target,
                 permission_mode mode) :
                _type (make_variant_type<T>()), _perms (mode),
                _get ([target]()->variant{ return get_property(target); }),
                _validate ([](const variant&)->bool { return true; }),
                _set ( [target](const variant& v)->bool {
                    return set_property(target, v);
                } ) {
            if(!target)
                throw std::runtime_error("null property");
        }

        template <typename T, typename Lock>
        property(const std::shared_ptr<T>& target,
                 permission_mode mode,
                 const std::shared_ptr<Lock>& lock) :
                _type (make_variant_type<T>()), _perms (mode),
                _get ([target, lock]()->variant{
                    return get_property(target, lock);
                }),
                _validate ([](const variant&)->bool { return true; }),
                _set ( [target, lock](const variant& v)->bool {
                    return set_property(target, v, lock);
                } ) {
            if(!target || !lock)
                throw std::runtime_error("null property");
        }

        template <typename T>
        property(const std::shared_ptr<T>& target,
                 permission_mode mode,
                 const std::function<bool(const T&)>& validate) :
                _type (make_variant_type<T>()), _perms (mode),
                _get ([target]()->variant{
                    return get_property(target);
                }),
                _validate ([validate](const variant& v)->bool {
                    return validate_input(validate, v);
                }),
                _set ( [target](const variant& v)->bool {
                    return set_property(target, v);
                } ) {
            if(!target)
                throw std::runtime_error("null property");
        }

        template <typename T, typename Lock>
        property(const std::shared_ptr<T>& target,
                 permission_mode mode,
                 const std::function<bool(const T&)>& validate,
                 const std::shared_ptr<Lock>& lock) :
                _type (make_variant_type<T>()), _perms (mode),
                _get ([target, lock]()->variant{
                    return get_property(target, lock);
                }),
                _validate ([validate](const variant& v)->bool {
                    return validate_input(validate, v);
                }),
                _set ( [target, lock](const variant& v)->bool {
                    return set_property(target, v, lock);
                } ) {
            if(!target || !lock)
                throw std::runtime_error("null property");
        }

        template <typename T>
        property(const std::shared_ptr<const T>& target,
                 permission_mode mode) :
                 _type (make_variant_type<T>()),
                _perms (static_cast<permission_mode>(mode & ~writeable)),
                _get ([target]()->variant{ return get_property(target); }),
                _validate ([](const variant&)->bool { return false; }),
                _set ( [](const variant&)->bool { return false; } ) {
            if(!target)
                throw std::runtime_error("null property");
        }

        template <typename T, typename Lock>
        property(const std::shared_ptr<const T>& target,
                 permission_mode mode,
                 const std::shared_ptr<Lock>& lock) :
                _type (make_variant_type<T>()), _perms (mode),
                _get ([target, lock]()->variant{
                    return get_property(target, lock);
                }),
                _validate ([](const variant&)->bool { return true; }),
                _set ( [](const variant&)->bool { return false; } ) {
            if(!target || !lock)
                throw std::runtime_error("null property");
        }

        [[nodiscard]] variant get() const;
        [[nodiscard]] bool set(const variant& value) const;

        [[nodiscard]] const variant_type& type() const;

        [[nodiscard]] permission_mode permissions() const;
    };
}

#endif //IPCGULL_PROPERTY_H
