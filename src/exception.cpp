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

#include <ipcgull/exception.h>

using namespace ipcgull;

connection_failed::connection_failed(std::string w) : _what (std::move(w)) { }

connection_failed::connection_failed() :
    connection_failed("Connection failed") { }

const char* connection_failed::what() const noexcept {
    return _what.c_str();
}

connection_lost::connection_lost(const std::string& w) :
    connection_failed (w) { }

connection_lost::connection_lost() :
    connection_lost("Connection lost") { }

permission_denied::permission_denied(std::string w) :
    _what (std::move(w)) { }

permission_denied::permission_denied() :
    permission_denied("Permission denied") { }

const char* permission_denied::what() const noexcept {
    return _what.c_str();
}
