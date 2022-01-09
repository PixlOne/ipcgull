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
#include <list>
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

    enum property_permissions : uint8_t {
        property_no_permissions = 0,
        property_readable = 0b01,
        property_writeable = 0b10,
        property_full_permissions = 0b11
    };

    class interface;

    class base_property {
    private:
        const variant_type _type;
        property_permissions _perms;
        std::function<variant()> _get;
        std::function<bool(const variant&)> _validate;
        std::function<bool(const variant&)> _set;

        friend class interface;
    protected:
        template <typename T, typename Lock>
        base_property(property_permissions mode,
                      const std::shared_ptr<T>& target,
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

        template <typename T, typename Lock>
        base_property(property_permissions mode,
                      const std::shared_ptr<T>& target,
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

        template <typename T, typename Lock>
        base_property(property_permissions mode,
                      const std::shared_ptr<const T>& target,
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

        void notify_change() const;
    public:
        [[nodiscard]] variant get_variant() const;
        [[nodiscard]] bool set_variant(const variant& value);

        [[nodiscard]] const variant_type& type() const;

        [[nodiscard]] property_permissions permissions() const;
    };

    // properties are atomic
    template <typename T, typename Lock = std::mutex>
    class property : public base_property {
        std::shared_ptr<T> _data;
        mutable std::shared_ptr<Lock> _lock;
        property(const property_permissions& perms,
                 std::shared_ptr<T>&& data,
                 std::shared_ptr<Lock>&& lock) :
                base_property(perms, data, lock),
                _data (std::move(data)), _lock (std::move(lock)) {
            static_assert(variant_constructable<T>::value);
        }
    public:
        template <typename... Args>
        property(const property_permissions& perms, Args... args) :
            property(perms, std::make_shared<T>(std::forward<Args>(args)...),
                    std::make_shared<Lock>()) {
        }

        template <typename... Args>
        property(const property_permissions& perms,
                 const std::shared_ptr<Lock>& lock,
                 Args... args) :
                property(perms,
                         std::make_shared<T>(std::forward<Args>(args)...),
                         lock) {
        }

        template <typename... Args>
        property(const property_permissions& perms,
                 std::function<bool(const T&)> validate,
                 Args... args) :
                property(perms,
                         std::make_shared<T>(std::forward<Args>(args)...),
                         validate, std::make_shared<Lock>()) {
        }

        template <typename... Args>
        property(const property_permissions& perms,
                 const std::shared_ptr<Lock>& lock,
                 std::function<bool(const T&)> validate,
                 Args... args) :
                property(perms,
                         std::make_shared<T>(std::forward<Args>(args)...),
                         validate, lock) {
        }

        operator T() const {
            std::lock_guard<Lock> lock(*_lock);
            return *_data;
        }

        operator property<const T>() const {
            return property<const T, Lock>(*this);
        }

        property() = delete;

        property(const property& o) : base_property(o),
            _data (o._data), _lock (o._lock) {
        }

        property(property&& o) : base_property(std::move(o)),
            _data (std::move(o._data)), _lock (std::move(o._lock)) {
        }

        template <typename O, typename OLock>
        property(const property<O, OLock>& o) : base_property(o),
            _data (o._data), _lock(o._lock) {
        }

        template <typename O, typename OLock>
        property(property<O, OLock>&& o) : base_property(std::move(o)),
            _data (std::move(o._data)), _lock(std::move(o._lock)) {
        }

        property& operator=(const property& o) {
            if(this != &o) {
                operator=(o._data);
            }

            return *this;
        }
        property& operator=(property&& o) {
            if(this != &o) {
                operator=(std::move(o._data));
                o.notify_change();
            }

            return *this;
        }

        property& operator=(const T& o) {
            std::lock_guard<Lock> lock(*_lock);
            *_data = o;
            notify_change();
            return *this;
        }

        property& operator=(T&& o) {
            std::lock_guard<Lock> lock(*_lock);
            *_data = std::move(o);
            notify_change();
            return *this;
        }
    };
}

#endif //IPCGULL_PROPERTY_H
