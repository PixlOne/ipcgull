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

#include <stdexcept>
#include <utility>
#include <ipcgull/variant.h>
#include <ipcgull/node.h>
#include <ipcgull/interface.h>
#include <cassert>

using namespace ipcgull;

interface::interface(std::string name,
                     function_table f,
                     property_table p,
                     signal_table s) :
                    _name (std::move(name)),
                    _functions (std::move(f)),
                    _properties (std::move(p)),
                    _signals (std::move(s)) {
}

interface::~interface() {
    assert(_owner.expired());
}

interface::interface(interface&& o) noexcept :
    _name (o._name), _functions (o._functions),
    _properties (o._properties), _signals (o._signals) {
}

interface::interface(const interface& o) :
    _name (o._name), _functions (o._functions),
    _properties (o._properties), _signals (o._signals) {
}

const std::map<std::string, function>& interface::functions() const {
    return _functions;
}

const std::map<std::string, property>& interface::properties() const {
    return _properties;
}

const std::map<std::string, ipcgull::signal>& interface::signals() const {
    return _signals;
}

void interface::_emit_signal(const std::string &signal,
                             const std::vector<variant>& args,
                             const variant_type& args_type) const {
    if(auto owner = _owner.lock())
        owner->emit_signal(name(), signal, args, args_type);
}

const std::string& interface::name() const {
    return _name;
}
