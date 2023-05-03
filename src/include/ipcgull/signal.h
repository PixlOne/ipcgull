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

#ifndef IPCGULL_SIGNAL_H
#define IPCGULL_SIGNAL_H

#include <vector>

namespace ipcgull {
    class variant_type;

    struct signal {
    private:
        signal(std::vector<variant_type> t,
               std::vector<std::string> n);

    public:
        const std::vector<variant_type> types;
        const std::vector<std::string> names;

        template<typename... Args>
        static signal make_signal(
                const std::array<std::string, sizeof...(Args)>& n) {
            return {{make_variant_type<Args>()...},
                    {n.begin(), n.end()}};
        }
    };

    template<typename... Args>
    [[maybe_unused]]
    const auto make_signal = signal::make_signal<Args...>;
}

#endif //IPCGULL_SIGNAL_H
