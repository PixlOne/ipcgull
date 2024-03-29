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

#include <ipcgull/interface.h>
#include <ipcgull/node.h>
#include <ipcgull/server.h>
#include <iostream>
#include <thread>

#define SERVER_NAME "pizza.pixl.ipcgull.test"
#define SERVER_ROOT "/pizza/pixl/ipcgull_test"

constexpr ipcgull::connection_mode default_mode = ipcgull::IPCGULL_USER;

class sample_object : public ipcgull::object {
    int x;
public:
    explicit sample_object(int i = 0) : x{i} {}

    sample_object& operator=(int o) {
        x = o;
        return *this;
    }

    operator int&() {
        return x;
    }

    operator const int&() const {
        return x;
    }
};

class sample_interface : public ipcgull::interface {
private:
    std::weak_ptr<ipcgull::server> _server;
    std::weak_ptr<ipcgull::node> owner;

    static std::string echo(const std::string& input) {
        return input;
    }

    static void print(const std::string& input) {
        std::cout << input << std::endl;
    }

    void stop() {
        if (auto server = _server.lock()) {
            server->stop();
        } else {
            throw std::runtime_error("null server");
        }
    }

    void drop() {
        if (auto owner_sp = owner.lock())
            owner_sp->drop_interface("pizza.pixl.ipcgull.test.sample");
    }

    static void set_sample_object(const std::shared_ptr<sample_object>& o,
                                  const int& x) {
        *o = x;
    }

    static int get_sample_object(const std::shared_ptr<sample_object>& o) {
        return *o;
    }

    static std::tuple<std::string, int> cut_string(const std::string& in) {
        return {in.substr(0, 5), in.length()};
    }

public:
    sample_interface(const std::shared_ptr<ipcgull::server>& server,
                     const std::shared_ptr<ipcgull::node>& o,
                     const ipcgull::property<int>& ret) :
            ipcgull::interface("pizza.pixl.ipcgull.test.sample", {
                    {"Echo",      {echo,              {"input"},  {"output"}}},
                    {"Print",     {print,             {"input"}}},
                    {"Stop",      {this,              &sample_interface::stop}},
                    {"CutString", {cut_string,        {"input"},  {"cut", "original_length"}}},
                    {"Drop",      {this,              &sample_interface::drop}},
                    {"SetObject", {set_sample_object, {"object", "value"}}},
                    {"GetObject", {get_sample_object, {"object"}, {"value"}}}
            }, {
                                       {"ReturnCode", ret}
                               }, {
                                       {"InputReceived",
                                        ipcgull::make_signal<std::string>({"line"})}
                               }), _server(server), owner(o) {}

    void input_received(const std::string& line) {
        emit_signal("InputReceived", line);
    }
};

int main() {
    auto server = ipcgull::make_server(SERVER_NAME,
                                       SERVER_ROOT,
                                       default_mode);

    auto ret = ipcgull::property<int>(ipcgull::property_full_permissions);

    auto root = ipcgull::node::make_root("sample");
    auto ptr = std::make_shared<sample_object>(10);
    root->manage(ptr);
    root->add_server(server);
    auto iface = root->make_interface<sample_interface>(
            server, root, ret);

    std::thread signal_thread([iface, server, &ret]() {
        std::string line;

        while (std::getline(std::cin, line))
            iface->input_received(line);
    });

    server->start();
    signal_thread.join();

    return ret;
}