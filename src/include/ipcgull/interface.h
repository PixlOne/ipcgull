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

#ifndef IPCGULL_INTERFACE_H
#define IPCGULL_INTERFACE_H

#include <string>
#include <map>
#include <ipcgull/function.h>
#include <ipcgull/property.h>
#include <ipcgull/signal.h>

namespace ipcgull {
    class function;
    class base_property;
    struct signal;
    class node;

    class interface {
    public:
        typedef std::map<std::string, function> function_table;
        typedef std::map<std::string, base_property> property_table;
        typedef std::map<std::string, signal> signal_table;
        typedef std::tuple<function_table, property_table, signal_table> tables;
    private:
        const std::string _name;
        const function_table _functions;
        property_table _properties;
        const signal_table _signals;

        // The node should take ownership of the interface
        friend class node;
        std::weak_ptr<node> _owner;

        // Assumes types are checked
        [[maybe_unused]]
        void _emit_signal(const std::string& signal,
                         const std::vector<variant>& args,
                         const variant_type& args_type) const;
    public:
        interface(std::string name,
                  function_table f,
                  property_table p,
                  signal_table s);

        explicit interface(std::string name,
                           tables t={});

        ~interface();

        // Moving or copying the interface detaches from owner
        interface(interface&& o) noexcept;
        interface(const interface& o);

        template <typename T>
        interface& operator=(T) = delete;

        [[nodiscard]] const function_table& functions() const;
        [[nodiscard]] const property_table& properties() const;
        [[nodiscard]] const signal_table& signals() const;

        [[nodiscard]] const base_property& get_property(
                const std::string& name) const;
        [[nodiscard]] base_property& get_property(const std::string& name);

        template <typename... Args>
        [[maybe_unused]]
        void emit_signal(
                const std::string& signal, Args... args) const {
            try {
                const auto& expected_types = _signals.at(signal).types;
                if(sizeof...(Args) != expected_types.size())
                    throw std::runtime_error("invalid ipc signal arg count");

                std::vector<variant_type> v_types =
                        {make_variant_type<Args>()...};
                auto mismatch = std::mismatch(v_types.begin(),
                                              v_types.end(),
                                              expected_types.begin());
                if(mismatch.first != v_types.end() ||
                   mismatch.second != expected_types.end())
                    throw std::runtime_error("invalid ipc signal arg type");

                _emit_signal(signal, {to_variant(args)...},
                             variant_type::tuple(v_types));
            } catch(std::out_of_range& e) {
                throw std::runtime_error("unknown ipc signal emitted");
            }
        }

        [[nodiscard]] const std::string& name() const;
    };
}

#endif //IPCGULL_INTERFACE_H
