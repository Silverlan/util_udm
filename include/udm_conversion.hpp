/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UDM_CONVERSION_HPP__
#define __UDM_CONVERSION_HPP__

#include "udm_types.hpp"
#include "udm_trivial_types.hpp"
#include <type_traits>

#pragma warning( push )
#pragma warning( disable : 4715 )
namespace udm
{
	template <typename T>
		using base_type = typename std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;
	namespace detail
	{
		void test_conversions();
		inline uint32_t translate_quaternion_index(uint32_t idx) {return (idx == 0) ? 3 : (idx -1);}

		template<typename T0,typename T1>
		struct TypeConverter
		{
			static constexpr auto is_convertible = false;
		};

		template<typename T0,typename T1> requires((!std::is_same_v<T0,String> || std::is_same_v<T1,String> || std::is_same_v<T1,Reference>) && std::is_constructible_v<T1,T0> && !std::is_same_v<T1,Srgba> && !std::is_same_v<T1,HdrColor>)
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				if constexpr(std::is_same_v<T0,Quat> && std::is_same_v<T1,Mat4>)
					return umat::create(v0);
				else
					return T1(v0);
			}
		};

		template<typename T0,typename T1> requires(
			std::is_same_v<T0,String> && (is_numeric_type(type_to_enum<T1>()) || is_generic_type(type_to_enum<T1>()) || std::is_same_v<T1,Utf8String>) && !std::is_same_v<T1,Nil>
		)
		struct TypeConverter<T0,T1>
		{
			template<typename T,typename TValue,uint32_t TCount,uint32_t(*TTranslateIdx)(uint32_t)=nullptr>
			static void parse_value_list(const String &s,T &out)
			{
				size_t startPos = std::numeric_limits<size_t>::max();
				uint32_t idx = 0;
				size_t cIdx = 0;
				auto len = s.length();
				while(idx < TCount)
				{
					while(cIdx < len && std::isspace(s[cIdx]))
						++cIdx;
					auto start = cIdx;
					while(cIdx < len && !std::isspace(s[cIdx]))
						++cIdx;
					TValue *p;
					if constexpr(TTranslateIdx == nullptr)
					{
						if constexpr(std::is_same_v<T,Mat4> || std::is_same_v<T,Mat3x4>)
							p = &out[idx /4][idx %4];
						else
							p = &out[idx];
					}
					else
						p = &out[TTranslateIdx(idx)];
					*p = {};
					std::from_chars(s.data() +start,s.data() +cIdx,*p);
					++idx;
				}
			}

			static constexpr auto is_convertible = true;
			static T1 convert(const String &v0)
			{
				if constexpr(std::is_same_v<T1,bool>)
				{
					if(v0 == "1" || v0 == "true")
						return true;
					if(v0 == "0" || v0 == "false")
						return false;
					return false;
				}
				else if constexpr(std::is_same_v<T1,Half>)
				{
					uint16_t v1 {};
					std::from_chars(v0.data(),v0.data() +v0.size(),v1);
					return Half{v1};
				}
				else if constexpr(is_arithmetic<T1>)
				{
					T1 v1 {};
					std::from_chars(v0.data(),v0.data() +v0.size(),v1);
					return v1;
				}
				else if constexpr(std::is_same_v<T1,Half>)
				{
					Half half;
					half.value = 0;
					std::from_chars(v0.data(),v0.data() +v0.size(),half.value);
					return half;
				}
				else if constexpr(is_vector_type<T1>)
				{
					T1 v1;
					parse_value_list<T1,T1::value_type,T1::length()>(v0,v1);
					return v1;
				}
				else if constexpr(std::is_same_v<T1,Mat4> || std::is_same_v<T1,Mat3x4>)
				{
					T1 v1;
					parse_value_list<T1,T1::value_type,T1::length() *4>(v0,v1);
					return v1;
				}
				else if constexpr(std::is_same_v<T1,Srgba> || std::is_same_v<T1,HdrColor>)
				{
					T1 v1;
					parse_value_list<T1,T1::value_type,v1.size()>(v0,v1);
					return v1;
				}
				else if constexpr(std::is_same_v<T1,EulerAngles>)
				{
					T1 v1;
					parse_value_list<T1,float,3>(v0,v1);
					return v1;
				}
				else if constexpr(std::is_same_v<T1,Quaternion>)
				{
					T1 v1;
					parse_value_list<T1,T1::value_type,T1::length(),translate_quaternion_index>(v0,v1);
					return v1;
				}
				else if constexpr(std::derived_from<T1,Transform>)
				{
					T1 v1;
					using TTranslation = decltype(T1::translation);
					parse_value_list<TTranslation,TTranslation::value_type,TTranslation::length()>(v0,v1.translation);
					using TRotation = decltype(T1::rotation);
					parse_value_list<TRotation,TRotation::value_type,TRotation::length(),translate_quaternion_index>(v0,v1.rotation);
					if constexpr(std::is_same_v<T1,ScaledTransform>)
					{
						using TScale = decltype(T1::scale);
						parse_value_list<TScale,TScale::value_type,TScale::length()>(v0,v1.scale);
					}
					return v1;
				}
				else if constexpr(std::is_same_v<T1,Utf8String>)
				{
					std::vector<uint8_t> data;
					data.resize(v0.size() +1);
					memcpy(data.data(),v0.data(),v0.size());
					data.back() = '\0';
					return T1 {std::move(data)};
				}
				else
					return static_cast<T1>(v0);
			}
		};
		
		template<typename T0,typename T1> requires(std::derived_from<T0,Transform> && (std::is_same_v<T1,Mat4> || std::is_same_v<T1,Mat3x4>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				return v0.ToMatrix();
			}
		};
		
		template<typename T0,typename T1> requires(std::is_same_v<T0,Srgba> && std::is_same_v<T1,HdrColor>)
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				return T1{v0[0],v0[1],v0[2]};
			}
		};
		
		template<typename T0,typename T1> requires(std::derived_from<T0,Quat> && std::is_same_v<T1,Mat3x4>)
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				return umat::create(v0);
			}
		};
		
		template<typename T0,typename T1> requires(std::derived_from<T0,EulerAngles> && (std::derived_from<T1,Transform> || std::is_same_v<T1,Mat4> || std::is_same_v<T1,Mat3x4>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				if constexpr(std::is_same_v<T1,Mat4> || std::is_same_v<T1,Mat3x4>)
					return umat::create(uquat::create(v0));
				else
					return T1{uquat::create(v0)};
			}
		};
		
		template<typename T0,typename T1> requires(std::is_same_v<T0,Vector3> && (std::is_same_v<T1,Srgba> || std::is_same_v<T1,HdrColor>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				return T1{
					static_cast<T1::value_type>(std::clamp<float>(v0.x *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max())),
					static_cast<T1::value_type>(std::clamp<float>(v0.y *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max())),
					static_cast<T1::value_type>(std::clamp<float>(v0.z *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max()))
				};
			}
		};
		
		template<typename T0,typename T1> requires(std::is_same_v<T0,Vector4> && (std::is_same_v<T1,Srgba> || std::is_same_v<T1,HdrColor>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				if constexpr(std::is_same_v<T1,Srgba>)
				{
					return T1{
						static_cast<T1::value_type>(std::clamp<float>(v0.x *std::numeric_limits<T1::value_type>::max(),0,std::numeric_limits<T1::value_type>::max())),
						static_cast<T1::value_type>(std::clamp<float>(v0.y *std::numeric_limits<T1::value_type>::max(),0,std::numeric_limits<T1::value_type>::max())),
						static_cast<T1::value_type>(std::clamp<float>(v0.z *std::numeric_limits<T1::value_type>::max(),0,std::numeric_limits<T1::value_type>::max())),
						static_cast<T1::value_type>(std::clamp<float>(v0.w *std::numeric_limits<T1::value_type>::max(),0,std::numeric_limits<T1::value_type>::max()))
					};
				}
				else
				{
					return T1{
						static_cast<T1::value_type>(std::clamp<float>(v0.x *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max())),
						static_cast<T1::value_type>(std::clamp<float>(v0.y *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max())),
						static_cast<T1::value_type>(std::clamp<float>(v0.z *std::numeric_limits<uint8_t>::max(),0,std::numeric_limits<T1::value_type>::max()))
					};
				}
			}
		};
		
		template<typename T0,typename T1> requires((std::is_same_v<T0,Srgba> || std::is_same_v<T0,HdrColor>) && (std::is_same_v<T1,Vector3> || std::is_same_v<T1,Vector4>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				if constexpr(std::is_same_v<T1,Vector3>)
				{
					return T1{
						v0[0] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
						v0[1] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
						v0[2] /static_cast<float>(std::numeric_limits<uint8_t>::max())
					};
				}
				else
				{
					if constexpr(std::is_same_v<T0,HdrColor>)
					{
						return T1{
							v0[0] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							v0[1] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							v0[2] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							1.f
						};
					}
					else
					{
						return T1{
							v0[0] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							v0[1] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							v0[2] /static_cast<float>(std::numeric_limits<uint8_t>::max()),
							v0[3] /static_cast<float>(std::numeric_limits<uint8_t>::max())
						};
					}
				}
			}
		};
		
		template<typename T0,typename T1> requires(std::is_same_v<T0,T1> && (std::is_same_v<T0,Srgba> || std::is_same_v<T0,HdrColor>))
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				return v0;
			}
		};

		template<typename T0,typename T1> requires(
			(
				std::is_arithmetic_v<T0> || umath::is_vector_type<T0> || umath::is_matrix_type<T0> || std::is_same_v<T0,Quat> || std::is_same_v<T0,EulerAngles> ||
				std::is_same_v<T0,Srgba> || std::is_same_v<T0,HdrColor> || std::is_same_v<T0,Transform> || std::is_same_v<T0,ScaledTransform> || std::is_same_v<T0,Reference> ||
				std::is_same_v<T0,Half> || std::is_same_v<T0,Nil>
			) && 
			std::is_same_v<T1,String>
		)
		struct TypeConverter<T0,T1>
		{
			static constexpr auto is_convertible = true;
			static T1 convert(const T0 &v0)
			{
				if constexpr(std::is_arithmetic_v<T0>)
					return std::to_string(v0);
				else if constexpr(umath::is_vector_type<T0>)
					return uvec::to_string<T0>(v0,' ');
				else if constexpr(umath::is_matrix_type<T0>)
					return umat::to_string<T0>(v0,' ');
				else if constexpr(std::is_same_v<T0,Quat>)
					return uquat::to_string(v0,' ');
				else if constexpr(std::is_same_v<T0,EulerAngles>)
					return std::to_string(v0.p) +' ' +std::to_string(v0.y) +' ' +std::to_string(v0.r);
				else if constexpr(std::is_same_v<T0,Srgba>)
					return std::to_string(v0[0]) +' ' +std::to_string(v0[1]) +' ' +std::to_string(v0[2]) +' ' +std::to_string(v0[3]);
				else if constexpr(std::is_same_v<T0,HdrColor>)
					return std::to_string(v0[0]) +' ' +std::to_string(v0[1]) +' ' +std::to_string(v0[2]);
				else if constexpr(std::is_same_v<T0,Transform>)
					return '[' +uvec::to_string<Vector3>(v0.GetOrigin(),' ') +"][" +uquat::to_string(v0.GetRotation(),' ') +']';
				else if constexpr(std::is_same_v<T0,ScaledTransform>)
					return '[' +uvec::to_string<Vector3>(v0.GetOrigin(),' ') +"][" +uquat::to_string(v0.GetRotation(),' ') +"][" +uvec::to_string<Vector3>(v0.GetScale(),' ') +']';
				else if constexpr(std::is_same_v<T0,Reference>)
					return v0.path;
				else if constexpr(std::is_same_v<T0,Half>)
					return std::to_string(v0.value);
				else if constexpr(std::is_same_v<T0,Nil>)
					return "nil";
			}
		};
	};
	template<typename TFrom,typename TTo>
		constexpr bool is_convertible()
	{
		return detail::TypeConverter<TFrom,TTo>::is_convertible;
	}
	template<typename TFrom,typename TTo> requires(is_convertible<TFrom,TTo>())
		constexpr TTo convert(const TFrom &from)
	{
		return detail::TypeConverter<TFrom,TTo>::convert(from);
	}
	template<typename TTo>
		constexpr bool is_convertible_from(Type tFrom);
	template<typename TFrom>
		constexpr bool is_convertible(Type tTo);
	constexpr bool is_convertible(Type tFrom,Type tTo);
};
template<typename TTo>
	constexpr bool udm::is_convertible_from(Type tFrom)
{
	if(is_ng_type(tFrom))
		return visit_ng(tFrom,[&](auto tag){return is_convertible<decltype(tag)::type,TTo>();});

	if(tFrom == Type::String)
		return is_convertible<String,TTo>();
	static_assert(umath::to_integral(Type::Count) == 33,"Update this list when new types are added!");
	return false;
}

template<typename TFrom>
	constexpr bool udm::is_convertible(Type tTo)
{
	if(is_ng_type(tTo))
		return visit_ng(tTo,[&](auto tag){return is_convertible<TFrom,decltype(tag)::type>();});

	if(tTo == Type::String)
		return is_convertible<TFrom,String>();
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
	return false;
}

constexpr bool udm::is_convertible(Type tFrom,Type tTo)
{
	if(is_ng_type(tFrom))
		return visit_ng(tFrom,[&](auto tag){return is_convertible<decltype(tag)::type>(tTo);});

	if(tFrom == Type::String)
		return is_convertible<String>(tTo);
	static_assert(umath::to_integral(Type::Count) == 36,"Update this list when new types are added!");
	return false;
}

#pragma warning( pop )

#endif
