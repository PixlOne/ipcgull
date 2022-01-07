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
    class server;

    GVariantType* g_type(std::any& x);

    const GVariantType* const_g_type(const std::any& x);

    std::any g_type_to_any(GVariantType* x);
}

#endif //IPCGULL_COMMON_GDBUS_H
