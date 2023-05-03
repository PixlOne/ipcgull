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

#ifndef IPCGULL_SERVER_H
#define IPCGULL_SERVER_H

#include <memory>
#include <mutex>
#include <ipcgull/variant.h>
#include <ipcgull/connection.h>

namespace ipcgull {
    class node;

    class interface;

    class server {
        // Implementation-defined internal class
        struct internal;
        std::shared_ptr<internal> _internal;

        std::weak_ptr<server> _self;

        const std::string _name;
        const std::string _root;

        friend class node;

        // Only the node should access these functions
        void emit_signal(
                const std::string& node, const std::string& iface,
                const std::string& signal, const variant_tuple& args,
                const variant_type& args_type) const;

        void add_interface(const std::shared_ptr<node>& node,
                           const interface& iface);

        bool drop_interface(const std::string& node_path,
                            const std::string& if_name) noexcept;

        void set_managing(const std::shared_ptr<node>& n,
                          const std::weak_ptr<object>& managing);

    protected:
        server(std::string name,
               std::string root_node,
               enum connection_mode mode);

    public:
        static std::shared_ptr<server> make_server(
                const std::string& name, const std::string& root_node,
                enum connection_mode mode);

        ~server();

        server(server&&) = delete;

        server(const server&) = delete;

        [[maybe_unused]] void reconnect();

        [[maybe_unused]] void start();

        void stop();

        void stop_wait();

        void stop_sync();

        [[nodiscard]] bool running() const;

        [[nodiscard]] const std::string& root_node() const;
    };

    [[maybe_unused]]
    const auto make_server = server::make_server;
}


#endif //IPCGULL_SERVER_H
