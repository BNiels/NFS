#pragma once
#include "Helper.h"
#include "Checks.h"
#include <memory>

#include <unordered_map>
#include <typeinfo>
#include <string>

#include <type_traits>

namespace lag
{
	/*
	TypeStruct class
	a struct that has every member in a given
	typelist as a member variable.
	is faster than using a normal list because all
	the data
	*/
	template<typename ... Types>
	struct TypeStruct;
	
	template<typename Front, typename ... Back>
	struct TypeStruct<Front, Back...>
	{
		using typelist = TypeList<Front, Back...>;
		Front front;
		TypeStruct<Back...> back;

		//get the amount of types in the struct
		constexpr unsigned int size();
		
		//get a type at an index
		template<typename T>
		T &getType();

		//compile time function to get a type at an index (much faster than runtime version)
		template<unsigned int I>
		auto &getAt();

		//runtime function to get a type at an index (much slower than the compile time version)
		template<typename T>
		T &getAt(unsigned int index);

		template<template<typename> typename F, typename ... Args>
		void runFuncFor(std::string className, Args ... args);
	};

	template<typename Front>
	struct TypeStruct<Front>
	{
		using typelist = TypeList<Front>;
		Front front;

		//get the amount of types in the struct
		constexpr unsigned int size()
		{
			return 1;
		}

		//get a type at an index
		template<typename T>
		T &getType()
		{
			if (std::is_same<T, Front>::value)
			{
				return front;
			}
		}

		//compile time function to get a type at an index (much faster than runtime version)
		template<unsigned int I>
		auto &getAt()
		{
			if (I == 0)
			{
				return front;
			}
		}

		//runtime function to get a type at an index (much slower than the compile time version)
		template<typename T>
		T &getAt(unsigned int index)
		{
			return *(T*)&front;
		}

		template<template<typename> typename F, typename ... Args>
		void runFuncFor(std::string className, Args ... args)
		{
			if (className == helper::removeBullshit(typeid(Front).name()))
			{
				F<Front>{}((void*)&front, args...);
			}
		}
	};


	template<unsigned int I, typename T>
	struct AC
	{
		using type = typename AC<I - 1, decltype(T::back)>::type;
	};

	template<typename T>
	struct AC<0, T>
	{
		using type = decltype(T::front);
	};


	template<unsigned int I, typename R, typename T>
	struct AccessTypeBack
	{
		static R &func(T &t)
		{
			return AccessTypeBack<I - 1, R, decltype(t.back)>::func(t.back);
		}
	};

	template<typename R, typename T>
	struct AccessTypeBack<0, R, T>
	{
		static R &func(T &t)
		{
			return t.front;
		}
	};

	template<unsigned int I, typename T>
	struct AccessType
	{
		using FinalType = typename AC<I, T>::type;
		static FinalType &func(T &t)
		{
			return AccessTypeBack<I, FinalType, T>::func(t);
		}
	};

	template<unsigned int I, typename ... Types>
	auto access_type(TypeStruct<Types...> &typeStruct)
		->typename AccessType<I, TypeStruct<Types...>>::FinalType&
	{ return AccessType<I, TypeStruct<Types...>>::func(typeStruct); }
	
	//get variable by type
	template<typename T, typename ... Types>
	T &access_by_type(TypeStruct<Types...> &typeStruct)
	{
		constexpr unsigned int index = TypeIndex<T, Types...>;
		return access_type<index>(typeStruct);
	}


	template<typename Front, typename ...Back>
	inline constexpr unsigned int TypeStruct<Front, Back...>::size()
	{
		return SizeTypes<Front, Back...>;
	}

	template<typename Front, typename ...Back>
	template<typename T>
	inline T & TypeStruct<Front, Back...>::getType()
	{
		return access_by_type<T>(*this);
	}

	template<typename Front, typename ...Back>
	template<unsigned int I>
	inline auto &TypeStruct<Front, Back...>::getAt()
	{
		return access_type<I>(*this);
	}

	//void* return = any type in the TypeStruct
	//void* argument = TypeList, will be cast in the function
	using getter_fp = void*(void*);

	template<unsigned int I, typename ... Types>
	void *getter_fp_function(void *ts)
	{
		TypeStruct<Types...> *realArg = (TypeStruct<Types...>*)ts;
		auto &val = realArg->getAt<I>();
		return (void*)&val;
	}

	template<typename Buffer, unsigned int I, typename ... Types>
	struct MakeAccessFpLoop
	{
		static void insert(TypeStruct<Types...> &ts, Buffer &buffer)
		{
			buffer[I] = getter_fp_function<I, Types...>;
			MakeAccessFpLoop<Buffer, I - 1, Types...>::insert(ts, buffer);
		}
	};

	template<typename Buffer, typename ... Types>
	struct MakeAccessFpLoop<Buffer, 0, Types...>
	{
		static void insert(TypeStruct<Types...> &ts, Buffer &buffer)
		{
			buffer[0] = getter_fp_function<0, Types...>;
		}
	};

	template<typename T, unsigned int I>
	struct TSArray
	{
		T ar[I];
		T &operator[](size_t index)
		{
			return ar[index];
		}
	};

	//functions to make buffer of function pointers to get variable at index
	template<typename ... Types>
	auto make_access_fp_buffer(TypeStruct<Types...> &typeStruct)
		->TSArray<getter_fp*, SizeTypes<Types...>>
	{
		constexpr unsigned int bufferSize = SizeTypes<Types...>;
		getter_fp *funcs[bufferSize];
		MakeAccessFpLoop<decltype(funcs), bufferSize - 1, Types...>::insert(typeStruct, funcs);
		TSArray<getter_fp*, SizeTypes<Types...>> ret;
		//ret.ar = funcs;
		memcpy(&ret.ar, &funcs, sizeof(funcs));
		return ret;
	}

	template<typename Front, typename ...Back>
	template<typename T>
	inline T & TypeStruct<Front, Back...>::getAt(unsigned int index)
	{
		const static auto funcs = make_access_fp_buffer<Front, Back...>(*this);
		getter_fp *getAtT = funcs.ar[index];
		return *((T*)(getAtT((void*)this)));
	}

	

	//type erasure any func
	template<typename ... Args>
	using any_fp = void(void*, Args...);

	template<template<typename> typename F, typename TSType, typename T, typename ... Args>
	void any_fp_function(void *ts, Args ... args)
	{
		using ActualTSType = std::remove_reference_t<TSType>;
		ActualTSType *tscast = (ActualTSType*)ts;
		F<T>{}((T*)&tscast->getType<T>(), args...);
	}

	/*
	Basic struct with function
	template<typename T>
	struct Func
	{
		void operator()(T *ptr, int some)
		{}
	};
	*/

	template<typename ... Args>
	using MapType = std::unordered_map<std::string, any_fp<Args...>*>;

	template<typename ... Types>
	struct MakeAnyFpMap;

	template<typename First, typename ... Types>
	struct MakeAnyFpMap<First, Types...>
	{
		template<template<typename> typename F, typename MapT, typename ... Args, typename ... TypeListTypes>//NOTE : finish this function, then the one below
		static void func(MapT &map, TypeStruct<TypeListTypes...> *ts)
		{
			map[helper::removeBullshit(typeid(First).name())] = &any_fp_function<F, decltype(*ts), First, Args...>;
			MakeAnyFpMap<Types...>::func<F, MapT, Args...>(map, ts);
		}
	};

	template<typename First>
	struct MakeAnyFpMap<First>
	{
		template<template<typename> typename F, typename MapT, typename ... Args, typename ... TypeListTypes>
		static void func(MapT &map, TypeStruct<TypeListTypes...> *ts)
		{
			map[helper::removeBullshit(typeid(First).name())] = &any_fp_function<F, decltype(*ts), First, Args...>;
		}
	};

	template<template<typename> typename F, typename ... Args, typename ... TypeListTypes>
	MapType<Args...> make_any_fp_map(TypeStruct<TypeListTypes...> *ts)
	{
		MapType<Args...> ret;
		MakeAnyFpMap<TypeListTypes...>::func<F, MapType<Args...>, Args...>(ret, ts);
		return ret;
	}

	template<typename Front, typename ...Back>
	template<template<typename> typename F, typename ...Args>
	inline void TypeStruct<Front, Back...>::runFuncFor(std::string className, Args ... args)
	{
		const static MapType<Args...> funcMap = make_any_fp_map<F, Args...>(this);
		void(*funcPtr)(void*, Args...) = funcMap.at(className);
		funcPtr((void*)this, args...);
	}
}