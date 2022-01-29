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

#include <condition_variable>
#include <ipcgull/node.h>
#include <ipcgull/server.h>

using namespace ipcgull;

namespace ipcgull {
    class _server : public server {
    public:
        template <typename... Args>
        explicit _server(Args... args) : server(std::forward<Args>(args)...) { }
    };

    struct server::internal {
        std::mutex run_lock;
        bool running;
        std::mutex state_change;
        std::condition_variable cv;
        internal() : running (false) { }
    };
}

variant_type::variant_type() = default;
variant_type::~variant_type() = default;
variant_type::variant_type([[maybe_unused]] const std::type_info&) { }
variant_type::variant_type([[maybe_unused]] const variant_type &o) { }
variant_type variant_type::vector(const variant_type &t) { return {}; }
variant_type variant_type::map(const variant_type& k, const variant_type& v)
{ return {}; }
variant_type variant_type::tuple(const std::vector<variant_type>& t)
{ return {}; }
variant_type variant_type::vector(variant_type&& t) { return {}; }
variant_type variant_type::map(variant_type&& k, variant_type&& v)
{ return {}; }
variant_type variant_type::tuple(std::vector<variant_type>&& t) { return {}; }
variant_type& variant_type::operator=(const variant_type& o) { return *this; }
variant_type& variant_type::operator=(variant_type&& o) noexcept { return *this; }
bool variant_type::operator==(const variant_type& o) const { return true; }
bool variant_type::valid() const { return true; }
variant_type variant_type::from_internal(std::any &&x) { return {}; }
const std::any &variant_type::raw_data() const { return data; }

server::server(std::string name,
               std::string root_node,
               enum connection_mode mode) :
                       _name (std::move(name)), _root (std::move(root_node)),
                       _internal (std::make_shared<internal>())
{
}

std::shared_ptr<server> server::make_server(const std::string &name,
                                            const std::string &root_node,
                                            enum connection_mode mode)
{
    auto s = std::make_shared<_server>(name, root_node, mode);
    s->_self = s;
    return s;
}

void server::emit_signal(
        const std::string &node, const std::string &iface,
        const std::string &signal, const variant_tuple &args,
        const variant_type &args_type) const
{
}

void server::add_interface(const std::shared_ptr<node>& node,
                           const interface& iface)
{
}

bool server::drop_interface(const std::string &node_path,
                            const std::string &if_name) noexcept
{
    return true;
}

void server::set_managing(const std::shared_ptr<node>& n,
                  const std::weak_ptr<object>& managing)
{
}

server::~server() {
    if(running())
        stop_sync();
}

void server::reconnect()
{
}

void server::start()
{
    std::lock_guard<std::mutex> lock(_internal->run_lock);
    std::unique_lock<std::mutex> wait(_internal->state_change);
    _internal->running = true;
    while(_internal->running)
        _internal->cv.wait(wait);
}

void server::stop()
{
    {
        std::lock_guard<std::mutex> lock(_internal->state_change);
        _internal->running = false;
    }
    _internal->cv.notify_all();
}

void server::stop_wait()
{
    std::lock_guard<std::mutex> lock(_internal->run_lock);
}

void server::stop_sync()
{
    stop();
    stop_wait();
}

bool server::running() const
{
    std::lock_guard<std::mutex> lock(_internal->state_change);
    return _internal->running;
}

const std::string& server::root_node() const
{
    return _root;
}

std::string node::full_name(const server& s) const {
    const auto tree = tree_name();
    if(tree.empty())
        return s.root_node();
    else
        return s.root_node() + "/" + tree;
}

std::string node::tree_name() const {
    std::lock_guard<std::recursive_mutex> lock(*_hierarchy_lock);
    if(const auto parent = _parent.lock()) {
        return parent->tree_name() + "/" + name();
    } else {
        return name();
    }
}
