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

#include <set>
#include <utility>
#include <ipcgull/server.h>
#include <ipcgull/node.h>
#include <ipcgull/interface.h>

using namespace ipcgull;

namespace ipcgull {
    class _node : public node {
    public:
        explicit _node(const std::string& name) : node(name) {}

        explicit _node(const std::string& name,
                       const std::shared_ptr<const node>& parent) :
                node(name, parent) {}
    };
}

node::node(std::string name) : _name(std::move(name)),
                               _hierarchy_lock(std::make_shared<std::recursive_mutex>()) {}

node::node(std::string name,
           const std::shared_ptr<const node>& parent) :
        _name(std::move(name)), _hierarchy_lock(parent->_hierarchy_lock),
        _parent(parent) {}

node::~node() {
    std::lock_guard<std::recursive_mutex> lock(*_hierarchy_lock);

    if (auto p = _parent.lock()) {
        assert(_parent_it != p->_children.end());
        p->_children.erase(_parent_it);
    }

    for (auto& s: _servers)
        drop_server(s);

    // Orphans are moved to the parent
    for (auto& x: _children) {
        assert(!x.expired());
        if (auto child = x.lock()) {
            // TODO: Move this into GDBus-specifc code somehow
            child->_name = name() + "/" + child->_name;
            child->_parent = _parent;
            if (auto p = _parent.lock()) {
                p->_children.push_front(x);
                child->_parent_it = p->_children.cbegin();
            }
        }
    }
}

[[maybe_unused]]
std::shared_ptr<node> node::make_root(const std::string& name) {
    std::shared_ptr<node> ptr = std::make_shared<_node>(name);
    ptr->_self = ptr;

    return ptr;
}

[[maybe_unused]]
std::shared_ptr<node> node::make_child(const std::string& name) const {
    assert(!_self.expired());
    std::shared_ptr<node> ptr = std::make_shared<_node>(name, _self.lock());
    ptr->_self = ptr;

    for (auto& s: _servers) {
        if (!s.expired())
            ptr->add_server(s);
    }

    _children.push_front(ptr);
    ptr->_parent_it = _children.cbegin();

    return ptr;
}

[[maybe_unused]] bool node::drop_interface(const std::string& name) {
    auto if_it = _interfaces.find(name);
    if (if_it == _interfaces.end())
        return false;

    for (auto& s: _servers) {
        if (auto server = s.lock())
            server->drop_interface(full_name(*server), name);
    }
    if (auto lock = if_it->second.lock())
        lock->_owner.reset();
    _interfaces.erase(if_it);

    return true;
}

void node::add_server(const std::weak_ptr<server>& s) {
    if (auto server = s.lock()) {
        for (auto& existing_server: _servers) {
            if (existing_server.lock() == server)
                return;
        }

        auto self = _self.lock();
        std::set<std::shared_ptr<interface>> interfaces;
        const std::string node_path = full_name(*server);
        for (auto& x: _interfaces) {
            try {
                if (auto iface = x.second.lock()) {
                    server->add_interface(self, *iface);
                    interfaces.insert(iface);
                }
            } catch (std::exception& e) {
                for (auto& iface: interfaces)
                    server->drop_interface(node_path, iface->name());
                throw;
            }
        }
        _servers.push_front(s);
    }
}

bool node::drop_server(const std::weak_ptr<server>& s) {
    auto server = s.lock();
    bool server_found = false;
    for (auto& existing_server: _servers) {
        if (existing_server.lock() == server) {
            server_found = true;
            break;
        }
    }
    if (!server_found)
        return false;

    if (server) {
        const std::string node_path = full_name(*server);
        for (auto& x: _interfaces)
            server->drop_interface(node_path, x.first);
    }

    return true;
}

void node::manage(const std::weak_ptr<object>& obj) {
    _managing = obj;
    for (auto& x: _servers) {
        if (auto server = x.lock())
            server->set_managing(_self.lock(), obj);
    }
}

const std::weak_ptr<object>& node::managed() const {
    return _managing;
}

void node::emit_signal(const std::string& iface,
                       const std::string& signal,
                       const variant_tuple& args,
                       const variant_type& args_type) const {
    for (auto& s: _servers) {
        if (auto server = s.lock()) {
            server->emit_signal(full_name(*server), iface,
                                signal, args, args_type);
        }
    }
}

const std::map<std::string, std::weak_ptr<interface>>&
node::interfaces() const {
    return _interfaces;
}

const std::string& node::name() const {
    return _name;
}
