// SPDX-FileCopyrightText: © 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

export module pragma.udm:util;

export import :array;
import :structure;
import :types.element;
import :types.string;

export {
	namespace udm {
		std::string CONTROL_CHARACTERS = "{}[]<>$,:;";
		std::string WHITESPACE_CHARACTERS = " \t\f\v\n\r";
		constexpr auto PATH_SEPARATOR = '/';
		DLLUDM bool is_whitespace_character(char c);
		DLLUDM bool is_control_character(char c);
		DLLUDM bool does_key_require_quotes(const std::string_view &key);

		DLLUDM void sanitize_key_name(std::string &key);

		constexpr size_t size_of_base_type(Type t)
		{
			if(is_non_trivial_type(t)) {
				auto tag = get_non_trivial_tag(t);
				return std::visit([&](auto tag) { return sizeof(typename decltype(tag)::type); }, tag);
			}
			return size_of(t);
		}

		template<typename T>
		void lerp_value(const T &value0, const T &value1, float f, T &outValue, Type type)
		{
			using TBase = base_type<T>;
			if constexpr(std::is_same_v<TBase, Transform> || std::is_same_v<TBase, ScaledTransform>) {
				outValue.SetOrigin(uvec::lerp(value0.GetOrigin(), value1.GetOrigin(), f));
				outValue.SetRotation(uquat::slerp(value0.GetRotation(), value1.GetRotation(), f));
				if constexpr(std::is_same_v<TBase, ScaledTransform>)
					outValue.SetScale(uvec::lerp(value0.GetScale(), value1.GetScale(), f));
			}
			else if constexpr(std::is_same_v<TBase, Half>)
				outValue = static_cast<float>(pragma::math::lerp(static_cast<float>(value0), static_cast<float>(value1), f));
			else if constexpr(::udm::is_arithmetic<TBase>)
				outValue = pragma::math::lerp(value0, value1, f);
			else if constexpr(::udm::is_vector_type<TBase>) {
				if constexpr(std::is_integral_v<typename TBase::value_type>)
					; // TODO
				else
					outValue = value0 + (value1 - value0) * f;
			}
			else if constexpr(std::is_same_v<TBase, EulerAngles>) {
				auto q0 = uquat::create(value0);
				auto q1 = uquat::create(value1);
				auto qr = uquat::slerp(q0, q1, f);
				outValue = EulerAngles {qr};
			}
			else if constexpr(std::is_same_v<TBase, Quaternion>)
				outValue = uquat::slerp(value0, value1, f);
			else {
				outValue = value0;
				auto n = udm::get_numeric_component_count(type);
				for(auto i = decltype(n) {0u}; i < n; ++i) {
					auto &f0 = *(reinterpret_cast<const float *>(&value0) + i);
					auto &f1 = *(reinterpret_cast<const float *>(&value1) + i);

					*(reinterpret_cast<float *>(&outValue) + i) = pragma::math::lerp(f0, f1, f);
				}
			}
		}

		void to_json(LinkedPropertyWrapperArg prop, std::stringstream &ss);
	}
}
