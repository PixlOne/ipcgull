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

namespace ipcgull {
    GVariantType* g_type(std::any& x) {
        return std::any_cast<GVariantType*>(x);
    }

    const GVariantType* const_g_type(const std::any& x) {
        try {
            return std::any_cast<const GVariantType*>(x);
        } catch(std::bad_any_cast& e) {
            return std::any_cast<GVariantType*>(x);
        }
    }

     std::any g_type_to_any(GVariantType* x) {
        return x;
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
variant ipcgull::from_gvariant(GVariant* v) {
    if(v == nullptr)
        return variant_tuple();

    const auto *type = g_variant_get_type(v);

    if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT16)) {
        return g_variant_get_int16(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT16)) {
        return g_variant_get_uint16(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT32)) {
        return g_variant_get_int32(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT32)) {
        return g_variant_get_uint32(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_INT64)) {
        return g_variant_get_int64(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_UINT64)) {
        return g_variant_get_uint64(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_DOUBLE)) {
        return g_variant_get_double(v);
    } else if (g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_BYTE)) {
        return g_variant_get_byte(v);
    } else if(g_variant_type_is_subtype_of(type,
                                           G_VARIANT_TYPE_OBJECT_PATH)) {
        gsize length;
        const char* c_str = g_variant_get_string(v, &length);
        return object_path(c_str, length);
    } else if(g_variant_type_is_subtype_of(type,
                                           G_VARIANT_TYPE_SIGNATURE)) {
        gsize length;
        const char* c_str = g_variant_get_string(v, &length);
        return signature(c_str, length);
    } else if(g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_STRING)) {
        gsize length;
        const char* c_str = g_variant_get_string(v, &length);
        return std::string(c_str, length);
    } else if(g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_BOOLEAN)) {
        return {g_variant_get_boolean(v)};
    } else if(g_variant_type_is_subtype_of(type,
                                           G_VARIANT_TYPE_DICTIONARY)) {
        const gsize length = g_variant_n_children(v);
        std::map<variant, variant> dict;
        for(gsize i = 0; i < length; ++i) {
            auto* element = g_variant_get_child_value(v, i);
            assert(g_variant_n_children(element) == 2);
            auto* key = g_variant_get_child_value(element, 0);
            auto* val = g_variant_get_child_value(element, 1);
            dict.emplace(std::piecewise_construct,
                         std::forward_as_tuple(from_gvariant(key)),
                         std::forward_as_tuple(from_gvariant(val)));
            g_variant_unref(key);
            g_variant_unref(val);
            g_variant_unref(element);
        }
        return dict;
    } else if(g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_TUPLE)) {
        const gsize length = g_variant_n_children(v);
        std::vector<variant> array(length);
        for(gsize i = 0; i < length; ++i) {
            auto* child_gvar = g_variant_get_child_value(v, i);
            array[i] = from_gvariant(child_gvar);
            g_variant_unref(child_gvar);
        }

        return variant_tuple(array);
    } else if(g_variant_type_is_subtype_of(type, G_VARIANT_TYPE_ARRAY)) {
        const gsize length = g_variant_n_children(v);
        std::vector<variant> array(length);
        for(gsize i = 0; i < length; ++i) {
            auto* child_gvar = g_variant_get_child_value(v, i);
            array[i] = from_gvariant(child_gvar);
            g_variant_unref(child_gvar);
        }

        return array;
    } else {
        throw std::invalid_argument("Unsupported GVariant type");
    }
}
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
GVariant* ipcgull::to_gvariant(const variant& v,
                               const variant_type& type) noexcept {
    if(std::holds_alternative<int16_t>(v)) {
        return g_variant_new_int16(std::get<int16_t>(v));
    } else if(std::holds_alternative<uint16_t>(v)) {
        return g_variant_new_uint16(std::get<uint16_t>(v));
    } else if(std::holds_alternative<int32_t>(v)) {
        return g_variant_new_int32(std::get<int32_t>(v));
    } else if(std::holds_alternative<uint32_t>(v)) {
        return g_variant_new_uint32(std::get<uint32_t>(v));
    } else if(std::holds_alternative<int64_t>(v)) {
        return g_variant_new_int64(std::get<int64_t>(v));
    } else if(std::holds_alternative<uint64_t>(v)) {
        return g_variant_new_uint64(std::get<uint64_t>(v));
    } else if(std::holds_alternative<double>(v)) {
        return g_variant_new_double(std::get<double>(v));
    } else if(std::holds_alternative<uint8_t>(v)) {
        return g_variant_new_byte(std::get<uint8_t>(v));
    } else if(std::holds_alternative<object_path>(v)) {
        return g_variant_new_object_path(std::get<object_path>(v).c_str());
    } else if(std::holds_alternative<signature>(v)) {
        return g_variant_new_signature(std::get<signature>(v).c_str());
    } else if(std::holds_alternative<std::string>(v)) {
        return g_variant_new_string(std::get<std::string>(v).c_str());
    } else if(std::holds_alternative<bool>(v)) {
        return g_variant_new_boolean(std::get<bool>(v));
    } else if(std::holds_alternative<variant_tuple>(v)) {
        const std::vector<variant>& vector = std::get<variant_tuple>(v);
        std::unique_ptr<GVariant*[]> raw_array(new GVariant*[vector.size()]);
        variant_type child_type {};
        for(gsize i = 0; i < vector.size(); ++i) {
            if(child_type == variant_type()) {
                child_type = variant_type::from_internal(g_variant_type_first(
                                const_g_type(type.raw_data())
                            ));
            } else {
                child_type = variant_type::from_internal(g_variant_type_next(
                        const_g_type(child_type.raw_data())
                ));
            }
            assert(child_type != variant_type());
            raw_array[i] = to_gvariant(vector[i], child_type);
        }
        return g_variant_new_tuple(raw_array.get(), vector.size());
    } else if(std::holds_alternative<std::vector<variant>>(v)) {
        const auto& vector = std::get<std::vector<variant>>(v);
        const auto child_type = variant_type::from_internal(
                g_variant_type_element(const_g_type(type.raw_data())
        ));

        if(vector.empty())
            return g_variant_new_array(
                    const_g_type(child_type.raw_data()),
                    nullptr, 0);

        std::unique_ptr<GVariant*[]> raw_array(new GVariant*[vector.size()]);
        for(gsize i = 0; i < vector.size(); ++i)
            raw_array[i] = to_gvariant(vector[i], child_type);

        return g_variant_new_array(
                const_g_type(child_type.raw_data()),
                raw_array.get(), vector.size());
    } else if(std::holds_alternative<std::map<variant, variant>>(v)) {
        const auto& map = std::get<std::map<variant, variant>>(v);
        const auto child_type = variant_type::from_internal(
                g_variant_type_element(const_g_type(type.raw_data())
        ));

        if(map.empty())
            return g_variant_new_array(const_g_type(child_type), nullptr, 0);

        gsize i = 0;
        std::unique_ptr<GVariant*[]> raw_array(new GVariant*[map.size()]);
        for(auto& x : map) {
            const auto key_type = variant_type::from_internal(
                    g_variant_type_key(const_g_type(child_type)));
            const auto value_type = variant_type::from_internal(
                    g_variant_type_value(const_g_type(child_type)));

            raw_array[i] = g_variant_new_dict_entry(
                    to_gvariant(x.first, key_type),
                    to_gvariant(x.second, value_type));
            ++i;
        }
        return g_variant_new_array(nullptr, raw_array.get(), map.size());
    } else {
        // This should not happen
        assert(!"converted unhandled variant type");
    }
}

variant_type::variant_type() :
    data {static_cast<GVariantType*>(nullptr)} {
}

variant_type variant_type::from_internal(std::any &&x) {
    variant_type t {};
    try {
        std::any_cast<GVariantType*>(x);
        t.data = std::move(x);
    } catch(std::bad_any_cast& e) {
        std::any_cast<const GVariantType*>(x);
        t.data = std::move(x);
    }

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
    else if(primitive == typeid(object_path))
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
    try {
        if(auto gvar = g_type(data))
            g_variant_type_free(gvar);
    } catch(std::bad_any_cast& e) {
        // This is valid, data could be const
    }
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
    try {
        return const_g_type(data);
    } catch(std::bad_any_cast& e) {
        return false;
    }
}

const std::any& variant_type::raw_data() const {
    return data;
}
