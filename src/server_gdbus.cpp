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
        template <typename... Args>
        explicit _server(Args... args) : server(std::forward<Args>(args)...) { }
    };
}

struct server::internal {
    struct internal_node {
        std::weak_ptr<node> object;
        std::map<std::string, guint> interfaces;

        explicit internal_node(std::weak_ptr<node> obj) :
            object (std::move(obj)) { }
    };

    std::map<std::string, internal_node> nodes;

    GDBusConnection* connection = nullptr;
    GBusType bus_type = G_BUS_TYPE_NONE;
    GDBusObjectManagerServer* object_manager = nullptr;
    guint gdbus_name = 0;

    std::mutex run_lock;
    std::atomic<GMainLoop*> main_loop = nullptr;

    std::atomic<enum name_state> owns_name = NAME_LOST;

    std::atomic_bool stop_requested = false;

    // C-style GDBus callbacks
    static void gdbus_method_call(
            [[maybe_unused]] GDBusConnection *connection,
            [[maybe_unused]] const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant *parameters,
            GDBusMethodInvocation *invocation,
            gpointer internal_weak) {
        if(auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            auto weak_node = i->nodes.find(object_path);
            if(weak_node == i->nodes.end()) {
                g_dbus_method_invocation_return_error(
                        invocation, G_DBUS_ERROR,
                        G_DBUS_ERROR_UNKNOWN_OBJECT, "Unknown object");
                return;
            }
            if(auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if(iface_it == node->interfaces().end()) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_INTERFACE,
                            "Unknown interface");
                    return;
                }
                const auto& functions = iface_it->second->functions();
                auto f_it = functions.find(method_name);

                if(f_it == functions.end()) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_METHOD,
                            "Unknown method");
                    return;
                }

                try {
                    auto v_args = from_gvariant(parameters);
                    try {
                        const auto args = std::get<variant_tuple>(v_args);
                        const auto response = f_it->second(args);
                        if(response.empty()) {
                            g_dbus_method_invocation_return_value(
                                    invocation, nullptr);
                            return;
                        }
                        // Response is guaranteed to have a valid response type
                        auto g_response = to_gvariant(
                                response,
                                variant_type::tuple(
                                        f_it->second.return_types()));

                        g_dbus_method_invocation_return_value(
                                invocation, g_response);
                        return;
                    } catch(std::bad_variant_access& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_INVALID_SIGNATURE,
                                "Invalid argument type");
                        return;
                    } catch(std::invalid_argument& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_INVALID_ARGS,
                                "Invalid arguments");
                        return;
                    } catch(std::exception& e) {
                        g_dbus_method_invocation_return_error(
                                invocation, G_DBUS_ERROR,
                                G_DBUS_ERROR_FAILED,
                                "%s", e.what());
                        return;
                    }
                } catch(std::invalid_argument& e) {
                    g_dbus_method_invocation_return_error(
                            invocation, G_DBUS_ERROR,
                            G_DBUS_ERROR_INVALID_SIGNATURE,
                            "Unimplemented argument type");
                    return;
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
            [[maybe_unused]] GDBusConnection *connection,
            [[maybe_unused]] const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GError **error,
            gpointer internal_weak) {
        if(auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            auto weak_node = i->nodes.find(object_path);
            if(weak_node == i->nodes.end()) {
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Unknown object");
                return nullptr;
            }
            if(auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if(iface_it == node->interfaces().end()) {
                    g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_INTERFACE,
                            "Unknown interface");
                    return nullptr;
                }
                const auto& properties = iface_it->second->properties();
                auto p_it = properties.find(property_name);

                if(p_it == properties.end())
                    g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_PROPERTY,
                            "Unknown property");

                return to_gvariant(p_it->second.get(),
                                   p_it->second.type());
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
        }
    }

    static gboolean gdbus_set_property([[maybe_unused]] GDBusConnection *connection,
                                       [[maybe_unused]] const gchar *sender,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *property_name,
                                       GVariant *value,
                                       GError **error,
                                       gpointer internal_weak) {
        if(auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            auto weak_node = i->nodes.find(object_path);
            if(weak_node == i->nodes.end()) {
                g_set_error(error, G_DBUS_ERROR,
                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                            "Unknown object");
                return false;
            }
            if(auto node = weak_node->second.object.lock()) {
                auto iface_it = node->interfaces().find(interface_name);
                if(iface_it == node->interfaces().end()) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                "Unknown interface");
                    return false;
                }
                const auto& properties = iface_it->second->properties();
                auto p_it = properties.find(property_name);

                if(p_it == properties.end()) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_UNKNOWN_PROPERTY,
                                "Unknown property");
                    return false;
                }

                try {
                    return p_it->second.set(from_gvariant(value));
                } catch(std::bad_variant_access& e) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_INVALID_SIGNATURE,
                                "Invalid argument type");
                    return false;
                } catch(permission_denied& e) {
                    g_set_error(error, G_DBUS_ERROR,
                                G_DBUS_ERROR_PROPERTY_READ_ONLY,
                                "%s", e.what());
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
        }

    }

    static void name_acquired_handler(
            [[maybe_unused]] GDBusConnection* connection,
            [[maybe_unused]] const gchar* name,
            gpointer internal_weak) {
        if(auto i = static_cast<std::weak_ptr<internal>*>(
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
        if(auto i = static_cast<std::weak_ptr<internal>*>(
                internal_weak)->lock()) {
            i->owns_name = NAME_LOST;

            if(i->main_loop) {
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
            } catch(std::bad_any_cast& e) {
                g_type = std::any_cast<const GVariantType*>(type.raw_data());
            }
            if(!g_type)
                throw std::runtime_error("null ipcgull::variant_type");
            info->signature = g_variant_type_dup_string(g_type);
            assert(info->signature);
        } catch(std::bad_any_cast& e) {
            throw std::runtime_error("bad ipcgull::variant_type");
        }

        return info;
    }

    static GDBusArgInfo** args_info(const std::vector<std::string>& names,
                                    const std::vector<variant_type>& types) {
        assert(names.size() == types.size());
        const auto size = std::min(names.size(), types.size());
        if(!size)
            return nullptr;
        auto* g_args = g_new(GDBusArgInfo*,size+1);
        assert(g_args);
        g_args[size] = nullptr;
        for(std::size_t i = 0; i < size; ++i)
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
                                            const property& p) {
        auto* info = g_new(GDBusPropertyInfo, 1);
        assert(info);
        info->ref_count = 1;
        info->name = g_strdup(name.c_str());
        info->annotations = nullptr;
        {
            int flags = G_DBUS_PROPERTY_INFO_FLAGS_NONE;
            if(p.permissions() & property::readable)
                flags |= G_DBUS_PROPERTY_INFO_FLAGS_READABLE;
            if(p.permissions() & property::writeable)
                flags |= G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;
            info->flags = static_cast<GDBusPropertyInfoFlags>(flags);
        }
        try {
            const GVariantType* g_type;
            try {
                g_type = std::any_cast<GVariantType*>(p.type().raw_data());
            } catch(std::bad_any_cast& e) {
                g_type = std::any_cast<const GVariantType*>(
                        p.type().raw_data());
            }
            if(!g_type)
                throw std::runtime_error("null ipcgull::variant_type");
            info->signature = g_variant_type_dup_string(g_type);
            assert(info->signature);
        } catch(std::bad_any_cast& e) {
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
        auto* info = g_new(GDBusInterfaceInfo , 1);
        assert(info);
        info->ref_count = 1;
        /// TODO: Annotation support
        info->annotations = nullptr;
        info->name = g_strdup(iface.name().c_str());

        {
            const auto& functions = iface.functions();
            if(functions.empty()) {
                info->methods = nullptr;
            } else {
                info->methods = g_new(GDBusMethodInfo*,
                                      functions.size() + 1);
                assert(info->methods);
                info->methods[functions.size()] = nullptr;
            }

            std::size_t i = 0;
            for(auto& x : functions) {
                info->methods[i] = function_info(x.first, x.second);
                ++i;
            }
        }

        {
            const auto& properties = iface.properties();

            if(properties.empty()) {
                info->properties = nullptr;
            } else {
                info->properties = g_new(GDBusPropertyInfo*,
                                         properties.size() + 1);
                assert(info->properties);
                info->properties[properties.size()] = nullptr;
            }

            std::size_t i = 0;
            for(auto& x : properties) {
                info->properties[i] = property_info(x.first, x.second);
                ++i;
            }
        }

        {
            const auto& signals = iface.signals();

            if(signals.empty()) {
                info->signals = nullptr;
            } else {
                info->signals = g_new(GDBusSignalInfo*,
                                      signals.size() + 1);
                assert(info->signals);
                info->signals[signals.size()] = nullptr;
            }

            std::size_t i = 0;
            for(auto& x : signals) {
                info->signals[i] = signal_info(x.first, x.second);
                ++i;
            }
        }

        return info;
    }

    static constexpr GDBusInterfaceVTable interface_vtable = {
            .method_call = gdbus_method_call,
            .get_property = gdbus_get_property,
            .set_property = gdbus_set_property
    };
};


server::server(std::string name,
               std::string root_node,
               enum connection_mode mode) :
                       _internal {std::make_shared<internal>()},
                       _name {std::move(name)}, _root {std::move(root_node)}
{
    if(!_internal)
        throw std::runtime_error("server internal struct failed to allocate");

    std::lock_guard<std::mutex> lock(server_init_mutex);
    if(server_exists)
        throw std::runtime_error("server already exists");

    GError* err = nullptr;

    switch(mode) {
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

    if(err) {
        const std::string ewhat(err->message);
        g_clear_error(&err);
        throw connection_failed(ewhat);
    }
    if(!_internal->connection) {
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
        const std::string &name, const std::string &root_node,
        enum connection_mode mode) {
    std::shared_ptr<server> ptr = std::make_shared<_server>(
            name, root_node, mode);
    ptr->_self = ptr;

    return ptr;
}

server::~server() {
    if(running())
        stop_sync();

    for(auto& x : _internal->nodes) {
        if(auto n = x.second.object.lock())
            n->drop_server(_self);
    }

    if(_internal->main_loop) {
        GMainLoop* loop = _internal->main_loop;
        _internal->main_loop = nullptr;
        g_main_loop_unref(loop);
    }

    if(_internal->object_manager) {
        g_dbus_object_manager_server_set_connection(_internal->object_manager,
                                                    nullptr);
        g_object_unref(_internal->object_manager);
    }

    if(_internal->gdbus_name)
        g_bus_unown_name(_internal->gdbus_name);

    if(_internal->connection) {
        g_dbus_connection_close_sync(_internal->connection, nullptr, nullptr);
        g_object_unref(_internal->connection);
    }
}

void server::emit_signal(
        const std::string& node, const std::string& iface,
        const std::string& signal, const variant_tuple& args,
        const variant_type& args_type) const {
    auto* g_args = g_variant_ref_sink(to_gvariant(args, args_type));
    GError* error = nullptr;

    // TODO: Destination bus support
    if(!g_dbus_connection_emit_signal(
            _internal->connection, nullptr,
            node.c_str(), iface.c_str(),
            signal.c_str(), g_args, &error)) {
        if(error) {
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
    auto node_name = node->full_name(*this);
    auto node_it = _internal->nodes.find(node_name);
    if(node_it->second.interfaces.count(iface.name()))
        throw std::runtime_error("interface already exists");

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

    if(error) {
        const std::string ewhat(error->message);
        g_clear_error(&error);
        throw std::runtime_error(ewhat);
    }

    if(node_it == _internal->nodes.end()) {
        node_it = _internal->nodes.emplace(node_name, node).first;
    }
    node_it->second.interfaces.emplace(iface.name(), reg_id);
}

bool server::drop_interface(const std::string& node_path,
                            const std::string& if_name) noexcept {
    bool ret;
    auto node_it = _internal->nodes.find(node_path);
    if(node_it == _internal->nodes.end())
        return false;

    auto iface_it = node_it->second.interfaces.find(if_name);
    if(iface_it == node_it->second.interfaces.end())
        return false;

    ret = g_dbus_connection_unregister_object(_internal->connection,
                                              iface_it->second);

    node_it->second.interfaces.erase(iface_it);
    if(node_it->second.interfaces.empty())
        _internal->nodes.erase(node_it);

    return ret;
}

[[maybe_unused]] void server::reconnect() {
    // We do not need to reconnect if already running.
    if(running())
        return;
    std::lock_guard<std::mutex> lock(_internal->run_lock);
    GError* err = nullptr;

    if(_internal->connection) {
        if(g_dbus_connection_is_closed(_internal->connection)) {
            if(_internal->object_manager)
                g_dbus_object_manager_server_set_connection(
                        _internal->object_manager,nullptr);

            g_object_unref(_internal->connection);
        }
    }

    if(!_internal->connection) {
        if(_internal->object_manager) {
            g_object_unref(_internal->object_manager);
            _internal->object_manager = nullptr;
        }

        _internal->owns_name = NAME_LOST;
        _internal->gdbus_name = 0;
        _internal->connection = g_bus_get_sync(_internal->bus_type,
                                               nullptr, &err);

        if(err) {
            const std::string ewhat(err->message);
            g_clear_error(&err);
            throw connection_failed(ewhat);
        }
        if(!_internal->connection) {
            throw connection_failed();
        }
    }

    if(_internal->owns_name == NAME_LOST) {
        auto* internal_weak = new std::weak_ptr<internal>(_internal);
        _internal->owns_name = NAME_WAITING;
        _internal->gdbus_name = g_bus_own_name_on_connection(
                _internal->connection, _name.c_str(),
                G_BUS_NAME_OWNER_FLAGS_NONE, internal::name_acquired_handler,
                internal::name_lost_handler, internal_weak,
                internal::free_internal_weak);
    }

    if(!_internal->object_manager) {
        _internal->object_manager = g_dbus_object_manager_server_new(
                _root.c_str());
        assert(_internal->object_manager);
        g_dbus_object_manager_server_set_connection(_internal->object_manager,
                                                    _internal->connection);
    }
}

[[maybe_unused]] void server::start() {
    if(running())
        throw std::runtime_error("server is already running");

    if(_internal->owns_name == NAME_LOST)
        throw connection_lost("dbus name lost");

    if(!_internal->main_loop)
        _internal->main_loop = g_main_loop_new(nullptr, false);

    _internal->stop_requested = false;

    std::lock_guard<std::mutex> lock(_internal->run_lock);
    g_main_loop_run(_internal->main_loop);

    if(!_internal->owns_name && !_internal->stop_requested)
        throw connection_lost("dbus name lost");
}

void server::stop() {
    _internal->stop_requested = true;
    if(_internal->main_loop) {
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
    if(_internal->main_loop)
        return g_main_loop_is_running(_internal->main_loop);
    return false;
}

const std::string& server::root_node() const {
    return _root;
}

std::string node::full_name(const server& s) const {
    return s.root_node() + "/" + tree_name();
}

std::string node::tree_name() const {
    if(const auto parent = _parent.lock()) {
        return parent->name() + "/" + name();
    } else {
        return name();
    }
}
