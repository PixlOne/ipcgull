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

#ifndef IPCGULL_COMMON_GDBUS_H
#define IPCGULL_COMMON_GDBUS_H

#include <ipcgull/variant.h>

namespace ipcgull {
    variant from_gvariant(GVariant* v);

    // type is only necessary for arrays and maps
    GVariant* to_gvariant(const variant& v,
                          const variant_type& type) noexcept;
}

#endif //IPCGULL_COMMON_GDBUS_H
