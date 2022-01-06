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

#ifndef IPCGULL_EXCEPTION_H
#define IPCGULL_EXCEPTION_H

#include <string>
#include <stdexcept>

namespace ipcgull {

    class connection_failed : public std::exception {
    private:
        const std::string _what;
    public:
        explicit connection_failed(std::string w);
        connection_failed();

        [[nodiscard]] const char* what() const noexcept override;
    };

    class connection_lost : public connection_failed {
    public:
        explicit connection_lost(const std::string& w);
        connection_lost();
    };

    class permission_denied : public std::exception {
    private:
        const std::string _what;
    public:
        explicit permission_denied(std::string w);

        permission_denied();

        [[nodiscard]] const char* what() const noexcept override;
    };
}

#endif //IPCGULL_EXCEPTION_H
