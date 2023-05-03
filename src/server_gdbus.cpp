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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <cassert>
#include <utility>
#include <gio/gio.h>
#include <ipcgull/exception.h>
#include <ipcgull/node.h>
#include <ipcgull/interface.h>
#include <ipcgull/server.h>

#include "common_gdbus.h"

using namespace ipcgull;

namespace ipcgull {
    // We may only have one DBus server per process;
    static std::atomic_bool server_exists = false;
    static std::mutex server_init_mutex;

    enum name_state {
        NAME_WAITING,
        NAME_LOST,
        NAME_OWNED
    };

    class _server : public server {
    public:
        template<typename... Args>
        explicit _server(Args... args) : server(std::forward<Args>(args)...) {}
    };
}

struct server::internal {
    struct internal_node {
        std::weak_ptr<node> object;
        std::map<std::string, guint> interfaces;

        explicit internal_node(std::weak_ptr<node> obj) :
                object(std::move(obj)) {}
    };

    std::map<std::string, internal_node> nodes;
    std::map<object*, std::string> object_path_lookup;

    GDBusConnection* connection = nullptr;
    GBusType bus_type = G_BUS_TYPE_NONE;
    GDBusObjectManagerServer* object_manager = nullptr;
    guint gdbus_name = 0;

    std::recursive_mutex server_lock;
    std::mutex run_lock;
    std::atomic<GMainLoop*> main_loop = nullptr;

    std::atomic<enum name_state> owns_name = NAME_LOST;

    std::atomic_bool stop_requested = false;

    variant from_gvariant(GVariant* v) {
        if (v == nullptr)
            return variant_tuple();

        const auto* type = g_variant_get_type(v);

        if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT16)) {
            return g_variant_get_int16(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT16)) {
            return g_variant_get_uint16(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT32)) {
            return g_variant_get_int32(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT32)) {
            return g_variant_get_uint32(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT64)) {
            return g_variant_get_int64(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT64)) {
            return g_variant_get_uint64(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_DOUBLE)) {
            return g_variant_get_double(v);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_BYTE)) {
            return g_variant_get_byte(v);
        } else if (g_variant_type_is_subtype_of(type,
                                                G_VARIANT_TYPE_OBJECT_PATH)) {
            gsize length;
            std::string path{g_variant_get_string(v, &length)};
            if (auto obj = nodes.at(path).object.lock()) {
                if (auto ptr = obj->managed().lock())
                    return ptr;
            }
            throw std::out_of_range("Node does not manage an object");
        } else if (g_variant_type_is_subtype_of(type,
                                                G_VARIANT_TYPE_SIGNATURE)) {
            gsize length;
            const char* c_str = g_variant_get_string(v, &length);
            return signature(c_str, length);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_STRING)) {
            gsize length;
            const char* c_str = g_variant_get_string(v, &length);
            return std::string(c_str, length);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_BOOLEAN)) {
            return {static_cast<bool>(g_variant_get_boolean(v))};
        } else if (g_variant_type_is_subtype_of(type,
                                                G_VARIANT_TYPE_DICTIONARY)) {
            const gsize length = g_variant_n_children(v);
            std::map<variant, variant> dict;
            for (gsize i = 0; i < length; ++i) {
                auto* element = g_variant_get_child_value(v, i);
                assert(g_variant_n_children(element) == 2);
                auto* key = g_variant_get_child_value(element, 0);
                auto* val = g_variant_get_child_value(element, 1);
                try {
                    dict.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(from_gvariant(key)),
                                 std::forward_as_tuple(from_gvariant(val)));
                } catch (std::exception& e) {
                    g_variant_unref(key);
                    g_variant_unref(val);
                    g_variant_unref(element);
                    throw;
                }
                g_variant_unref(key);
                g_variant_unref(val);
                g_variant_unref(element);
            }
            return dict;
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_TUPLE)) {
            const gsize length = g_variant_n_children(v);
            std::vector<variant> array(length);
            for (gsize i = 0; i < length; ++i) {
                auto* child_gvar = g_variant_get_child_value(v, i);
                try {
                    array[i] = from_gvariant(child_gvar);
                } catch (std::exception& e) {
                    g_variant_unref(child_gvar);
                    throw;
                }
                g_variant_unref(child_gvar);
            }

            return variant_tuple(array);
        } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_ARRAY)) {
            const gsize length = g_variant_n_children(v);
            std::vector<variant> array(length);
            for (gsize i = 0; i < length; ++i) {
                auto* child_gvar = g_variant_get_child_value(v, i);
                try {
                    array[i] = from_gvariant(child_gvar);
                } catch (std::exception& e) {
                    g_variant_unref(child_gvar);
                    throw;
                }
                g_variant_unref(child_gvar);
            }

            return array;
        } else {
            throw std::invalid_argument("Unsupported GVariant type");
        }
    }

    GVariant* to_gvariant(const variant& v,
                          const variant_type& type) {
        if (std::holds_alternative<int16_t>(v)) {
            return g_variant_new_int16(std::get<int16_t>(v));
        } else if (std::holds_alternative<uint16_t>(v)) {
            return g_variant_new_uint16(std::get<uint16_t>(v));
        } else if (std::holds_alternative<int32_t>(v)) {
            return g_variant_new_int32(std::get<int32_t>(v));
        } else if (std::holds_alternative<uint32_t>(v)) {
            return g_variant_new_uint32(std::get<uint32_t>(v));
        } else if (std::holds_alternative<int64_t>(v)) {
            return g_variant_new_int64(std::get<int64_t>(v));
        } else if (std::holds_alternative<uint64_t>(v)) {
            return g_variant_new_uint64(std::get<uint64_t>(v));
        } else if (std::holds_alternative<double>(v)) {
            return g_variant_new_double(std::get<double>(v));
        } else if (std::holds_alternative<uint8_t>(v)) {
            return g_variant_new_byte(std::get<uint8_t>(v));
        } else if (std::holds_alternative<std::shared_ptr<object>>(v)) {
            auto* ptr = std::get<std::shared_ptr<object>>(v).get();
            // May throw
            try {
                const auto object_path = object_path_lookup.at(ptr);
                return g_variant_new_object_path(object_path.c_str());
            } catch (std::out_of_range& e) {
                throw std::runtime_error("Invalid object path");
            }
        } else if (std::holds_alternative<signature>(v)) {
            return g_variant_new_signature(std::get<signature>(v).c_str());
        } else if (std::holds_alternative<std::string>(v)) {
            return g_variant_new_string(std::get<std::string>(v).c_str());
        } else if (std::holds_alternative<bool>(v)) {
            return g_variant_new_boolean(std::get<bool>(v));
        } else if (std::holds_alternative<variant_tuple>(v)) {
            const std::vector<variant>& vector = std::get<variant_tuple>(v);
            std::unique_ptr<GVariant* []> raw_array(
                    new GVariant* [vector.size()]);
            variant_type child_type{};
            for (gsize i = 0; i < vector.size(); ++i) {
                if (child_type == variant_type()) {
                    child_type = variant_type::from_internal(
                            g_variant_type_first(const_g_type(type.raw_data())
                            ));
                } else {
                    child_type = variant_type::from_internal(
                            g_variant_type_next(
                                    const_g_type(child_type.raw_data())
                            ));
                }
                assert(child_type != variant_type());
                try {
                    raw_array[i] = to_gvariant(vector[i], child_type);
                } catch (std::exception& e) {
                    for (int j = i - 1; j >= 0; --j)
                        g_variant_unref(g_variant_ref_sink(raw_array[j]));
                    throw;
                }
            }
            return g_variant_new_tuple(raw_array.get(), vector.size());
        } else if (std::holds_alternative<std::vector<variant>>(v)) {
            const auto& vector = std::get<std::vector<variant>>(v);
            const auto child_type = variant_type::from_internal(
                    g_variant_type_element(const_g_type(type.raw_data())
                    ));

            if (vector.empty())
                return g_variant_new_array(
                        const_g_type(child_type.raw_data()),
                        nullptr, 0);

            std::unique_ptr<GVariant* []> raw_array(new GVariant* [vector.size()]);
            for (gsize i = 0; i < vector.size(); ++i) {
                try {
                    raw_array[i] = to_gvariant(vector[i], child_type);
                } catch (std::exception& e) {
                    for (int j = i - 1; j >= 0; --j)
                        g_variant_unref(g_variant_ref_sink(raw_array[j]));
                    throw;
                }
            }

            return g_variant_new_array(
                    const_g_type(child_type.raw_data()),
                    raw_array.get(), vector.size());
        } else if (std::holds_alternative<std::map<variant, variant>>(v)) {
            const auto& map = std::get<std::map<variant, variant>>(v);
            const auto child_type = variant_type::from_internal(
                    g_variant_type_element(const_g_type(type.raw_data())
                    ));

            if (map.empty())
                return g_variant_new_array(
                        const_g_type(child_type), nullptr, 0);

            gsize i = 0;
            std::unique_ptr<GVariant* []> raw_array(new GVariant* [map.size()]);
            for (auto& x: map) {
                const auto key_type = variant_type::from_internal(
                        g_variant_type_key(const_g_type(child_type)));
                const auto value_type = variant_type::from_internal(
                        g_variant_type_value(const_g_type(child_type)));

                try {
                    raw_array[i] = g_variant_new_dict_entry(
                            to_gvariant(x.first, key_type),
                            to_gvariant(x.second, value_type));
                } catch (std::exception& e) {
                    for (int j = i - 1; j >= 0; --j)
                        g_variant_unref(g_variant_ref_sink(raw_array[j]));
                    throw;
                }

                ++i;
            }
            return g_variant_new_array(nullptr, raw_array.get(), map.size());
        } else {
            // This should not happen
            throw std::runtime_error("converted unhandled variant type");
        }
    }

    // C-style GDBus callbacks
    static void gdbus_method_call(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* sender,
            const gchar* object_path,
            const gchar* interface_name,
            const gchar* method_name,
            GVariant* parameters,
            GDBusMethodInvocation* invocation,
            gpointer internal_weak) {
        if (auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            std::lock_guard<std::recursive_mutex> lock(i->server_lock);
            auto weak_node = i->nodes.find(object_path);
            if (weak_node == i->nodes.end()) {
                g_dbus_method_invocation_return_error(
                        invocation, G_DBUS_ERROR,
                        G_DBUS_ERROR_UNKNOWN_OBJECT, "Unknown object");
                return;
            }
            if (auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if (iface_it == node->interfaces().end()) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_INTERFACE,
                            "Unknown interface");
                    return;
                }

                auto iface = iface_it->second.lock();
                if (!iface) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_INTERFACE,
                            "Interface expired");
                    return;
                }

                const auto& functions = iface->functions();
                auto f_it = functions.find(method_name);

                if (f_it == functions.end()) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_METHOD,
                            "Unknown method");
                    return;
                }

                try {
                    auto v_args = i->from_gvariant(parameters);
                    try {
                        const auto args = std::get<variant_tuple>(v_args);
                        const auto response = f_it->second(args);
                        if (response.empty()) {
                            g_dbus_method_invocation_return_value(
                                    invocation, nullptr);
                            return;
                        }
                        // Response is guaranteed to have a valid response type
                        auto g_response = i->to_gvariant(
                                response,
                                variant_type::tuple(
                                        f_it->second.return_types()));

                        g_dbus_method_invocation_return_value(
                                invocation, g_response);
                        return;
                    } catch (std::bad_variant_access& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_INVALID_SIGNATURE,
                                "Invalid argument type");
                        return;
                    } catch (std::invalid_argument& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_INVALID_ARGS,
                                "Invalid arguments");
                        return;
                    } catch (std::exception& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_FAILED,
                                "%s", e.what());
                        return;
                    }
                } catch (std::invalid_argument& e) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_INVALID_SIGNATURE,
                            "Unimplemented argument type");
                    return;
                } catch (std::out_of_range& e) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Invalid object path");
                }
            } else {
                // This shouldn't happen, but handle the case it does.
                g_dbus_method_invocation_return_error(
                        invocation, G_DBUS_ERROR,
                        G_DBUS_ERROR_UNKNOWN_OBJECT,
                        "Object no longer exists");
                return;
            }
        } else {
            g_dbus_method_invocation_return_error(
                    invocation, G_DBUS_ERROR,
                    G_DBUS_ERROR_FAILED,
                    "Internal error");
            assert(!"method call on non-existent server");
        }
    }

    static GVariant* gdbus_get_property(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* sender,
            const gchar* object_path,
            const gchar* interface_name,
            const gchar* property_name,
            GError** error,
            gpointer internal_weak) {
        if (auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            std::lock_guard<std::recursive_mutex> lock(i->server_lock);
            auto weak_node = i->nodes.find(object_path);
            if (weak_node == i->nodes.end()) {
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Unknown object");
                return nullptr;
            }
            if (auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if (iface_it == node->interfaces().end()) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                "Unknown interface");
                    return nullptr;
                }
                auto iface = iface_it->second.lock();
                if (!iface) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                "Interface expired");
                    return nullptr;
                }

                try {
                    const auto& property = iface->get_property(
                            property_name);

                    return i->to_gvariant(property.get_variant(),
                                          property.type());
                } catch (std::out_of_range& e) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_PROPERTY,
                                "Unknown property");
                    return nullptr;
                } catch (std::exception& e) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_FAILED,
                                "%s", e.what());
                    return nullptr;
                }

            } else {
                // This shouldn't happen, but handle the case it does.
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Object no longer exists");
                return nullptr;
            }
        } else {
            g_set_error(error, G_DBUS_ERROR,
                        G_DBUS_ERROR_FAILED,
                        "Internal error");
            assert(!"method call on non-existent server");
            return nullptr;
        }
    }

    static gboolean gdbus_set_property(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* sender,
            const gchar* object_path,
            const gchar* interface_name,
            const gchar* property_name,
            GVariant* value,
            GError** error,
            gpointer internal_weak) {
        if (auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            std::lock_guard<std::recursive_mutex> lock(i->server_lock);
            auto weak_node = i->nodes.find(object_path);
            if (weak_node == i->nodes.end()) {
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Unknown object");
                return false;
            }
            if (auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if (iface_it == node->interfaces().end()) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                "Unknown interface");
                    return false;
                }

                auto iface = iface_it->second.lock();
                if (!iface) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                "Interface expired");
                    return false;
                }

                try {
                    auto& p = iface->get_property(
                            property_name);

                    try {
                        return p.set_variant(i->from_gvariant(value));
                    } catch (std::bad_variant_access& e) {
                        g_set_error(error, G_DBUS_ERROR,
                                    G_DBUS_ERROR_INVALID_SIGNATURE,
                                    "Invalid argument type");
                        return false;
                    } catch (permission_denied& e) {
                        g_set_error(error, G_DBUS_ERROR,
                                    G_DBUS_ERROR_PROPERTY_READ_ONLY,
                                    "%s", e.what());
                        return false;
                    } catch (std::invalid_argument& e) {
                        g_set_error(error, G_DBUS_ERROR,
                                    G_DBUS_ERROR_INVALID_ARGS,
                                    "%s", e.what());
                        return false;
                    } catch (std::exception& e) {
                        g_set_error(error, G_DBUS_ERROR,
                                    G_DBUS_ERROR_FAILED,
                                    "%s", e.what());
                        return false;
                    }

                } catch (std::out_of_range& e) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_PROPERTY,
                                "Unknown property");
                    return false;
                }

            } else {
                // This shouldn't happen, but handle the case it does.
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Object no longer exists");
                return false;
            }
        } else {
            g_set_error(error, G_DBUS_ERROR,
                        G_DBUS_ERROR_FAILED,
                        "Internal error");
            assert(!"method call on non-existent server");
            return false;
        }
    }

    static void name_acquired_handler(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* name,
            gpointer internal_weak) {
        if (auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            i->owns_name = NAME_OWNED;
        } else {
            std::terminate();
        }
    }

    static void name_lost_handler(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* name,
            gpointer internal_weak) {
        if (auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            i->owns_name = NAME_LOST;

            if (i->main_loop) {
                if (g_main_loop_is_running(i->main_loop)) {
                    g_main_loop_quit(i->main_loop);
                }
            }
        } else {
            std::terminate();
        }
    }

    static void free_internal_weak(gpointer user_data) {
        auto* ptr = static_cast<std::weak_ptr<internal>*>(user_data);
        delete ptr;
    }

    static GDBusArgInfo* arg_info(const std::string& name,
                                  const variant_type& type) {
        auto* info = g_new(GDBusArgInfo, 1);
        assert(info);
        info->ref_count = 1;
        info->name = g_strdup(name.c_str());
        info->annotations = nullptr;
        try {
            const GVariantType* g_type;
            try {
                g_type = std::any_cast<GVariantType*>(type.raw_data());
            } catch (std::bad_any_cast& e) {
                g_type = std::any_cast<const GVariantType*>(type.raw_data());
            }
            if (!g_type)
                throw std::runtime_error("null ipcgull::variant_type");
            info->signature = g_variant_type_dup_string(g_type);
            assert(info->signature);
        } catch (std::bad_any_cast& e) {
            throw std::runtime_error("bad ipcgull::variant_type");
        }

        return info;
    }

    static GDBusArgInfo** args_info(const std::vector<std::string>& names,
                                    const std::vector<variant_type>& types) {
        assert(names.size() == types.size());
        const auto size = std::min(names.size(), types.size());
        if (!size)
            return nullptr;
        auto* g_args = g_new(GDBusArgInfo*, size + 1);
        assert(g_args);
        g_args[size] = nullptr;
        for (std::size_t i = 0; i < size; ++i)
            g_args[i] = arg_info(names[i], types[i]);

        return g_args;
    }

    static GDBusMethodInfo* function_info(const std::string& name,
                                          const function& f) {
        auto* info = g_new(GDBusMethodInfo, 1);
        assert(info);
        info->ref_count = 1;
        info->name = g_strdup(name.c_str());
        /// TODO: Annotation support
        info->annotations = nullptr;
        info->in_args = args_info(f.arg_names(), f.arg_types());
        info->out_args = args_info(f.return_names(), f.return_types());

        return info;
    }

    static GDBusPropertyInfo* property_info(const std::string& name,
                                            const base_property& p) {
        auto* info = g_new(GDBusPropertyInfo, 1);
        assert(info);
        info->ref_count = 1;
        info->name = g_strdup(name.c_str());
        info->annotations = nullptr;
        {
            int flags = G_DBUS_PROPERTY_INFO_FLAGS_NONE;
            if (p.permissions() & property_readable)
                flags |= G_DBUS_PROPERTY_INFO_FLAGS_READABLE;
            if (p.permissions() & property_writeable)
                flags |= G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
            info->flags = static_cast<GDBusPropertyInfoFlags>(flags);
        }
        try {
            const GVariantType* g_type;
            try {
                g_type = std::any_cast<GVariantType*>(p.type().raw_data());
            } catch (std::bad_any_cast& e) {
                g_type = std::any_cast<const GVariantType*>(
                        p.type().raw_data());
            }
            if (!g_type)
                throw std::runtime_error("null ipcgull::variant_type");
            info->signature = g_variant_type_dup_string(g_type);
            assert(info->signature);
        } catch (std::bad_any_cast& e) {
            throw std::runtime_error("bad ipcgull::variant_type");
        }

        return info;
    }

    static GDBusSignalInfo* signal_info(const std::string& name,
                                        const signal& s) {
        auto* info = g_new(GDBusSignalInfo, 1);
        assert(info);
        info->ref_count = 1;
        info->name = g_strdup(name.c_str());
        info->annotations = nullptr;
        info->args = args_info(s.names, s.types);
        return info;
    }

    static GDBusInterfaceInfo* interface_info(const interface& iface) {
        auto* info = g_new(GDBusInterfaceInfo, 1);
        assert(info);
        info->ref_count = 1;
        /// TODO: Annotation support
        info->annotations = nullptr;
        info->name = g_strdup(iface.name().c_str());

        {
            const auto& functions = iface.functions();
            if (functions.empty()) {
                info->methods = nullptr;
            } else {
                info->methods = g_new(GDBusMethodInfo*,
                                      functions.size() + 1);
                assert(info->methods);
                info->methods[functions.size()] = nullptr;
            }

            std::size_t i = 0;
            for (auto& x: functions) {
                info->methods[i] = function_info(x.first, x.second);
                ++i;
            }
        }

        {
            const auto& properties = iface.properties();

            if (properties.empty()) {
                info->properties = nullptr;
            } else {
                info->properties = g_new(GDBusPropertyInfo*,
                                         properties.size() + 1);
                assert(info->properties);
                info->properties[properties.size()] = nullptr;
            }

            std::size_t i = 0;
            for (auto& x: properties) {
                info->properties[i] = property_info(x.first, x.second);
                ++i;
            }
        }

        {
            const auto& signals = iface.signals();

            if (signals.empty()) {
                info->signals = nullptr;
            } else {
                info->signals = g_new(GDBusSignalInfo*,
                                      signals.size() + 1);
                assert(info->signals);
                info->signals[signals.size()] = nullptr;
            }

            std::size_t i = 0;
            for (auto& x: signals) {
                info->signals[i] = signal_info(x.first, x.second);
                ++i;
            }
        }

        return info;
    }

    static constexpr GDBusInterfaceVTable interface_vtable = {
            .method_call = gdbus_method_call,
            .get_property = gdbus_get_property,
            .set_property = gdbus_set_property,
            .padding = {}
    };
};


server::server(std::string name,
               std::string root_node,
               enum connection_mode mode) :
        _internal{std::make_shared<internal>()},
        _name{std::move(name)}, _root{std::move(root_node)} {
    if (!_internal)
        throw std::runtime_error("server internal struct failed to allocate");

    std::lock_guard<std::mutex> lock(server_init_mutex);
    if (server_exists)
        throw std::runtime_error("server already exists");

    GError* err = nullptr;

    switch (mode) {
        case IPCGULL_SYSTEM:
            _internal->bus_type = G_BUS_TYPE_SYSTEM;
            break;
        case IPCGULL_USER:
            _internal->bus_type = G_BUS_TYPE_SESSION;
            break;
        case IPCGULL_STARTER:
            _internal->bus_type = G_BUS_TYPE_STARTER;
            break;
    }

    _internal->connection = g_bus_get_sync(_internal->bus_type,
                                           nullptr, &err);

    if (err) {
        const std::string ewhat(err->message);
        g_clear_error(&err);
        throw connection_failed(ewhat);
    }
    if (!_internal->connection) {
        throw connection_failed();
    }

    {
        ///TODO: Support other DBus owner flags?
        auto* internal_weak = new std::weak_ptr<internal>(_internal);
        _internal->owns_name = NAME_WAITING;
        _internal->gdbus_name = g_bus_own_name_on_connection(
                _internal->connection, _name.c_str(),
                G_BUS_NAME_OWNER_FLAGS_NONE,
                internal::name_acquired_handler,
                internal::name_lost_handler, internal_weak,
                internal::free_internal_weak);
    }

    _internal->object_manager = g_dbus_object_manager_server_new(
            _root.c_str());
    assert(_internal->object_manager);
    g_dbus_object_manager_server_set_connection(_internal->object_manager,
                                                _internal->connection);

    // Only set server_exists on completion
    server_exists = true;
}

std::shared_ptr<server> server::make_server(
        const std::string& name, const std::string& root_node,
        enum connection_mode mode) {
    std::shared_ptr<server> ptr = std::make_shared<_server>(
            name, root_node, mode);
    ptr->_self = ptr;

    return ptr;
}

server::~server() {
    if (running())
        stop_sync();

    for (auto& x: _internal->nodes) {
        if (auto n = x.second.object.lock())
            n->drop_server(_self);
    }

    if (_internal->main_loop) {
        GMainLoop* loop = _internal->main_loop;
        _internal->main_loop = nullptr;
        g_main_loop_unref(loop);
    }

    if (_internal->object_manager) {
        g_dbus_object_manager_server_set_connection(_internal->object_manager,
                                                    nullptr);
        g_object_unref(_internal->object_manager);
    }

    if (_internal->gdbus_name)
        g_bus_unown_name(_internal->gdbus_name);

    if (_internal->connection) {
        g_dbus_connection_close_sync(_internal->connection, nullptr, nullptr);
        g_object_unref(_internal->connection);
    }
}

void server::emit_signal(
        const std::string& node, const std::string& iface,
        const std::string& signal, const variant_tuple& args,
        const variant_type& args_type) const {
    std::lock_guard<std::recursive_mutex> lock(_internal->server_lock);
    auto* g_args = g_variant_ref_sink(_internal->to_gvariant(args, args_type));
    GError* error = nullptr;

    // TODO: Destination bus support
    if (!g_dbus_connection_emit_signal(
            _internal->connection, nullptr,
            node.c_str(), iface.c_str(),
            signal.c_str(), g_args, &error)) {
        if (error) {
            g_variant_unref(g_args);
            const std::string ewhat(error->message);
            g_clear_error(&error);
            throw std::runtime_error(ewhat);
        }
    }

    g_variant_unref(g_args);
}

void server::add_interface(const std::shared_ptr<node>& node,
                           const interface& iface) {
    std::lock_guard<std::recursive_mutex> lock(_internal->server_lock);
    auto node_name = node->full_name(*this);
    auto node_it = _internal->nodes.find(node_name);
    if (node_it != _internal->nodes.end()) {
        if (node_it->second.interfaces.count(iface.name()))
            throw std::runtime_error("interface already exists");
    }

    auto* iface_info = internal::interface_info(iface);
    GError* error = nullptr;
    auto reg_id = g_dbus_connection_register_object(
            _internal->connection,
            node_name.c_str(),
            iface_info,
            &internal::interface_vtable,
            new std::weak_ptr<internal>(_internal),
            internal::free_internal_weak,
            &error);
    g_dbus_interface_info_unref(iface_info);

    if (error) {
        const std::string ewhat(error->message);
        g_clear_error(&error);
        throw std::runtime_error(ewhat);
    }

    if (node_it == _internal->nodes.end()) {
        node_it = _internal->nodes.emplace(node_name, node).first;
    }
    node_it->second.interfaces.emplace(iface.name(), reg_id);
}

bool server::drop_interface(const std::string& node_path,
                            const std::string& if_name) noexcept {
    std::lock_guard<std::recursive_mutex> lock(_internal->server_lock);
    bool ret;
    auto node_it = _internal->nodes.find(node_path);
    if (node_it == _internal->nodes.end())
        return false;

    auto iface_it = node_it->second.interfaces.find(if_name);
    if (iface_it == node_it->second.interfaces.end())
        return false;

    ret = g_dbus_connection_unregister_object(_internal->connection,
                                              iface_it->second);

    node_it->second.interfaces.erase(iface_it);
    if (node_it->second.interfaces.empty()) {
        if (auto node_sp = node_it->second.object.lock())
            _internal->object_path_lookup.erase(
                    node_sp->managed().lock().get());
        _internal->nodes.erase(node_it);
    }

    return ret;
}

void server::set_managing(const std::shared_ptr<node>& n,
                          const std::weak_ptr<object>& managing) {
    std::lock_guard<std::recursive_mutex> lock(_internal->server_lock);

    assert(n);

    _internal->object_path_lookup.erase(n->managed().lock().get());

    if (auto obj = managing.lock()) {
        if (_internal->object_path_lookup.count(obj.get()))
            throw std::runtime_error("Managed object must be unique");
        _internal->object_path_lookup.emplace(obj.get(),
                                              n->full_name(*this));
    }
}

[[maybe_unused]] void server::reconnect() {
    // We do not need to reconnect if already running.
    if (running())
        return;
    std::lock_guard<std::mutex> lock(_internal->run_lock);
    GError* err = nullptr;

    if (_internal->connection) {
        if (g_dbus_connection_is_closed(_internal->connection)) {
            if (_internal->object_manager)
                g_dbus_object_manager_server_set_connection(
                        _internal->object_manager, nullptr);

            g_object_unref(_internal->connection);
        }
    }

    if (!_internal->connection) {
        if (_internal->object_manager) {
            g_object_unref(_internal->object_manager);
            _internal->object_manager = nullptr;
        }

        _internal->owns_name = NAME_LOST;
        _internal->gdbus_name = 0;
        _internal->connection = g_bus_get_sync(_internal->bus_type,
                                               nullptr, &err);

        if (err) {
            const std::string ewhat(err->message);
            g_clear_error(&err);
            throw connection_failed(ewhat);
        }
        if (!_internal->connection) {
            throw connection_failed();
        }
    }

    if (_internal->owns_name == NAME_LOST) {
        auto* internal_weak = new std::weak_ptr<internal>(_internal);
        _internal->owns_name = NAME_WAITING;
        _internal->gdbus_name = g_bus_own_name_on_connection(
                _internal->connection, _name.c_str(),
                G_BUS_NAME_OWNER_FLAGS_NONE, internal::name_acquired_handler,
                internal::name_lost_handler, internal_weak,
                internal::free_internal_weak);
    }

    if (!_internal->object_manager) {
        _internal->object_manager = g_dbus_object_manager_server_new(
                _root.c_str());
        assert(_internal->object_manager);
        g_dbus_object_manager_server_set_connection(_internal->object_manager,
                                                    _internal->connection);
    }
}

[[maybe_unused]] void server::start() {
    if (running())
        throw std::runtime_error("server is already running");

    if (_internal->owns_name == NAME_LOST)
        throw connection_lost("dbus name lost");

    if (!_internal->main_loop)
        _internal->main_loop = g_main_loop_new(nullptr, false);

    _internal->stop_requested = false;

    std::lock_guard<std::mutex> lock(_internal->run_lock);
    g_main_loop_run(_internal->main_loop);

    if (!_internal->owns_name && !_internal->stop_requested)
        throw connection_lost("dbus name lost");
}

void server::stop() {
    _internal->stop_requested = true;
    if (_internal->main_loop) {
        g_main_loop_quit(_internal->main_loop);
    }
}

void server::stop_wait() {
    std::lock_guard<std::mutex> lock(_internal->run_lock);
}

void server::stop_sync() {
    stop();
    stop_wait();
}

bool server::running() const {
    if (_internal->main_loop)
        return g_main_loop_is_running(_internal->main_loop);
    return false;
}

const std::string& server::root_node() const {
    return _root;
}

std::string node::full_name(const server& s) const {
    const auto tree = tree_name();
    if (tree.empty())
        return s.root_node();
    else
        return s.root_node() + "/" + tree;
}

std::string node::tree_name() const {
    std::lock_guard<std::recursive_mutex> lock(*_hierarchy_lock);
    if (const auto parent = _parent.lock()) {
        return parent->tree_name() + "/" + name();
    } else {
        return name();
    }
}
