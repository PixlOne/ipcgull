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
#include <ipcgull/property.h>

using namespace ipcgull;

variant base_property::get_variant() const {
    if (permissions() & property_readable)
        return _get();
    throw permission_denied("property not readable");
}

///TODO: org.freedesktop.DBus.Properties support
bool base_property::set_variant(const variant& value) {
    if (!(permissions() & property_writeable))
        throw permission_denied("property not writeable");
    if (_validate(value))
        return _set(value);
    return false;
}

const variant_type& base_property::type() const {
    return _type;
}

property_permissions base_property::permissions() const {
    return _perms;
}


void base_property::notify_change() const {
    /// TODO: Support PropertiesChanged signal
}