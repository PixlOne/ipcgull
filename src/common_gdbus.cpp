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

#include <gio/gio.h>
#include <ipcgull/variant.h>
#include <cassert>
#include <stdexcept>

#include "common_gdbus.h"

using namespace ipcgull;

GVariantType* ipcgull::g_type(std::any& x) {
    return std::any_cast<GVariantType*>(x);
}

const GVariantType* ipcgull::const_g_type(const std::any& x) {
    if(x.type() == typeid(const GVariantType*))
        return std::any_cast<const GVariantType*>(x);
    else
        return std::any_cast<GVariantType*>(x);
}

 std::any ipcgull::g_type_to_any(GVariantType* x) {
    return x;
}

variant_type::variant_type() :
    data {static_cast<GVariantType*>(nullptr)} {
}

variant_type variant_type::from_internal(std::any &&x) {
    variant_type t {};
    if(x.type() == typeid(GVariantType*))
        std::any_cast<GVariantType*>(x);
    else
        std::any_cast<const GVariantType*>(x);
    t.data = std::move(x);

    return t;
}

variant_type::variant_type(const std::type_info& primitive) {
    const GVariantType* type;
    if(primitive == typeid(int16_t))
        type = G_VARIANT_TYPE_INT16;
    else if(primitive == typeid(uint16_t))
        type = G_VARIANT_TYPE_UINT16;
    else if(primitive == typeid(int32_t))
        type = G_VARIANT_TYPE_INT32;
    else if(primitive == typeid(uint32_t))
        type = G_VARIANT_TYPE_UINT32;
    else if(primitive == typeid(int64_t))
        type = G_VARIANT_TYPE_INT64;
    else if(primitive == typeid(uint64_t))
        type = G_VARIANT_TYPE_UINT64;
    else if(primitive == typeid(double))
        type = G_VARIANT_TYPE_DOUBLE;
    else if(primitive == typeid(uint8_t))
        type = G_VARIANT_TYPE_BYTE;
    else if(primitive == typeid(std::shared_ptr<object>))
        type = G_VARIANT_TYPE_OBJECT_PATH;
    else if(primitive == typeid(signature))
        type = G_VARIANT_TYPE_SIGNATURE;
    else if(primitive == typeid(std::string))
        type = G_VARIANT_TYPE_STRING;
    else if(primitive == typeid(bool))
        type = G_VARIANT_TYPE_BOOLEAN;
    else
        assert(!"Invalid GVariant type");
    data = g_type_to_any(g_variant_type_copy(type));
}

[[maybe_unused]]
variant_type variant_type::vector(const variant_type& t) {
    variant_type vec {};
    auto gvar = const_g_type(t.data);
    assert(gvar);
    vec.data = g_type_to_any(g_variant_type_new_array(gvar));

    return vec;
}

[[maybe_unused]]
variant_type variant_type::vector(variant_type&& t) {
    variant_type vec {};
    auto gvar = const_g_type(t.data);
    assert(gvar);
    vec.data = g_type_to_any(g_variant_type_new_array(gvar));
    t = {};

    return vec;
}

[[maybe_unused]]
variant_type variant_type::map(const variant_type& k, const variant_type& v) {
    variant_type type {};
    auto k_type = const_g_type(k.data);
    auto v_type = const_g_type(k.data);
    assert(k_type && v_type);
    type.data = g_type_to_any(
            g_variant_type_new_array(
                    g_variant_type_new_dict_entry(k_type, v_type) ));

    return type;
}

[[maybe_unused]]
variant_type variant_type::map(variant_type &&k, variant_type &&v) {
    variant_type type {};
    auto k_type = const_g_type(k.data);
    auto v_type = const_g_type(v.data);
    assert(k_type && v_type);
    type.data = g_type_to_any(g_variant_type_new_array(
            g_variant_type_new_dict_entry(k_type, v_type) ));
    k = {};
    v = {};

    return type;
}

[[maybe_unused]]
variant_type variant_type::tuple(const std::vector<variant_type>& types) {
    const auto size = static_cast<gint>(types.size());
    const std::unique_ptr<const GVariantType*[]> g_types(
            new const GVariantType*[types.size()]);
    for(std::size_t i = 0; i < types.size(); ++i) {
        auto gvar = const_g_type(types[i].data);
        assert(gvar);
        g_types[i] = gvar;
    }
    variant_type type {};
    type.data = g_type_to_any(g_variant_type_new_tuple(g_types.get(), size));
    return type;
}

[[maybe_unused]]
variant_type variant_type::tuple(std::vector<variant_type>&& types) {
    const auto size = static_cast<gint>(types.size());
    const std::unique_ptr<const GVariantType*[]> g_types(
            new const GVariantType*[types.size()]);
    for(std::size_t i = 0; i < types.size(); ++i) {
        auto gvar = const_g_type(types[i].data);
        assert(gvar);
        g_types[i] = gvar;
        types[i] = {};
    }
    variant_type type {};
    type.data = g_type_to_any(g_variant_type_new_tuple(g_types.get(), size));
    return type;
}

variant_type::variant_type(const variant_type &o) {
    if(auto gvar = const_g_type(o.data)) {
        data = g_type_to_any(g_variant_type_copy(gvar));
    } else {
        data = static_cast<GVariantType*>(nullptr);
    }
}

[[maybe_unused]]
variant_type::variant_type(variant_type&& o) noexcept = default;

variant_type& variant_type::operator=(const variant_type& o) {
    if(this != &o) {
        if(auto gvar = const_g_type(o.data)) {
            data = g_type_to_any(g_variant_type_copy(gvar));
        }
    }

    return *this;
}

variant_type& variant_type::operator=(variant_type&& o) noexcept {
    if(this != &o) {
        std::swap(data, o.data);
    }

    return *this;
}

variant_type::~variant_type() {
    if((data.type() == typeid(GVariantType*)))
        g_variant_type_free(g_type(data));
}

bool variant_type::operator==(const variant_type &o) const {
    if(auto lhs = const_g_type(data)) {
        if(auto rhs = const_g_type(o.data))
            return g_variant_type_equal(lhs, rhs);
        return false;
    }

    // lhs is null
    return !const_g_type(o.data);
}

[[maybe_unused]] [[nodiscard]]
bool variant_type::valid() const {
    if((data.type() == typeid(const GVariantType*)) ||
        (data.type() == typeid(GVariantType*)))
        return const_g_type(data);
    return false;
}

const std::any& variant_type::raw_data() const {
    return data;
}
