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

#ifndef IPCGULL_NODE_H
#define IPCGULL_NODE_H

#include <map>
#include <list>
#include <memory>
#include <string>
#include <ipcgull/variant.h>
#include <cassert>
#include <ipcgull/server.h>

namespace ipcgull {
    class interface;
    class server;

    class node {
        std::map<std::string, std::shared_ptr<interface>> _interfaces;
        std::list<std::weak_ptr<server>> _servers;
        std::string _name;

        std::weak_ptr<node> _parent;
        std::weak_ptr<node> _self;
        mutable std::list<std::weak_ptr<node>> _children;

        friend class interface;
        // Assumes that types are already checked
        void emit_signal(const std::string& iface,
                         const std::string& signal,
                         const variant_tuple& args,
                         const variant_type& args_type) const;

        friend class _node;
        explicit node(std::string name);
        explicit node(std::string name, std::weak_ptr<node> parent);
    public:
        ~node();

        node(node&&) = delete;
        node(const node&) = delete;

        [[maybe_unused]]
        static std::shared_ptr<node> make_root(const std::string& name);

        // Servers are passed onto the children
        [[maybe_unused]]
        std::shared_ptr<node> make_child(const std::string& name) const;

        template <typename T, typename... Args>
        [[maybe_unused]] std::shared_ptr<T> make_interface(Args... args) {
            static_assert(std::is_base_of<interface, T>::value,
                    "T must be an interface");
            auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
            ptr->_owner = _self;

            assert(!_self.expired());
            for(auto& s : _servers) {
                if(auto server = s.lock())
                    server->add_interface(_self.lock(), *ptr);
            }

            _interfaces.emplace(ptr->name(), ptr);

            return ptr;
        }

        [[maybe_unused]] bool drop_interface(const std::string& name);

        void add_server(const std::weak_ptr<server>& s);
        bool drop_server(const std::weak_ptr<server>& s);

        [[nodiscard]] const std::map<std::string, std::shared_ptr<interface>>&
            interfaces() const;

        [[nodiscard]] const std::string& name() const;

        // This should be provided by backend code.
        [[nodiscard]] std::string full_name(const server& s) const;
        [[nodiscard]] std::string tree_name() const;
    };
}

#endif //IPCGULL_NODE_H
