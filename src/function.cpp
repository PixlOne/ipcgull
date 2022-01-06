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

#include <ipcgull/function.h>
#include <stdexcept>

using namespace ipcgull;

variant_tuple function::operator()(const variant_tuple &args) const {
    return _f(args);
}

const std::vector<std::string>& function::arg_names() const {
    return _arg_names;
}

const std::vector<variant_type>& function::arg_types() const {
    return _arg_types;
}

const std::vector<std::string>& function::return_names() const {
    return _return_names;
}

const std::vector<variant_type>& function::return_types() const {
    return _return_types;
}
