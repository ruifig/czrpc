#pragma once

namespace cz
{
namespace rpc
{

// To work around the Windows vs Linux shenanigans with strncpy/strcpy/strlcpy, etc.
template<unsigned int N>
inline void copyStrToFixedBuffer(char (&dst)[N], const char* src)
{
#if _WIN32
	strncpy_s(dst, sizeof(dst), src, strlen(src));
#else
	strncpy(dst, src, strlen(src));
#endif
}

template<typename T>
std::shared_ptr<T> getSharedData()
{
    static std::mutex mtx;
    static std::weak_ptr<T> ptr;
    std::lock_guard<std::mutex> lk(mtx);
    auto p = ptr.lock();
    if (p)
        return p;
    p = std::make_shared<T>();
    ptr = p;
    return p;
}

//////////////////////////////////////////////////////////////////////////
// Based on
// https://functionalcpp.wordpress.com/2013/08/05/function-traits/
// with some additions
//////////////////////////////////////////////////////////////////////////

template<class F>
struct FunctionTraits {};

/*
// Specialization for lambdas
// Only works for non-generic lambdas.
template <class T>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())>
{
};
*/

namespace details
{
	template<typename T>
	struct CheckFuture
	{
		static constexpr bool value = false;
		using type = T;
	};
	template<typename T>
	struct CheckFuture<std::future<T>>
	{
		static constexpr bool value = true;
		using type = T;
	};
}

// function pointer
template<class R, class... Args>
struct FunctionTraits<R(*)(Args...)> : public FunctionTraits<R(Args...)>
{
};


template<class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...)> : public FunctionTraits<R(Args...)>
{
	using class_type = C;
};

template<class R, class C, class... Args>
struct FunctionTraits<R(C::*)(Args...) const> : public FunctionTraits<R(Args...)>
{
	using class_type = C;
};

template<class R, class... Args>
struct FunctionTraits<R(Args...)>
{
    using return_type = typename details::CheckFuture<R>::type;
	static constexpr bool valid = ParamTraits<return_type>::valid && ParamPack<Args...>::valid;
	static constexpr bool isasync = details::CheckFuture<R>::value;
	using param_tuple = std::tuple<typename ParamTraits<Args>::store_type...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};

//////////////////////////////////////////////////////////////////////////
//
// Calls a method on the specified object, unpacking the parameters from a tuple
//
//////////////////////////////////////////////////////////////////////////
// Based on: http://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
namespace detail
{
	template <typename F, typename Tuple, bool Done, int Total, int... N>
	struct callmethod_impl
	{
		static decltype(auto) call(typename FunctionTraits<F>::class_type& obj, F f, Tuple&& t)
		{
			return callmethod_impl<F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(obj, f, std::forward<Tuple>(t));
		}
	};

	template <typename F, typename Tuple, int Total, int... N>
	struct callmethod_impl<F, Tuple, true, Total, N...>
	{
		static decltype(auto) call(typename FunctionTraits<F>::class_type& obj, F f, Tuple&& t)
		{
			using Traits = FunctionTraits<F>;
			return (obj.*f)(
				ParamTraits<typename Traits::template argument<N>::type>::get(std::get<N>(std::forward<Tuple>(t)))...);
		}
	};
}

// user invokes this
template <typename F, typename Tuple>
decltype(auto) callMethod(typename FunctionTraits<F>::class_type& obj, F f, Tuple&& t)
{
	static_assert(FunctionTraits<F>::valid, "Function not usable as RPC");
	typedef typename std::decay<Tuple>::type ttype;
	return detail::callmethod_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>
		::call(obj, f, std::forward<Tuple>(t));
}

namespace details
{
	template<typename F, int N>
	struct Parameters
	{
		template<typename S>
		static void serialize(S&) {}
		template<typename S, typename First, typename... Rest>
		static void serialize(S& s, First&& first, Rest&&... rest)
		{
			using Traits = ParamTraits<typename FunctionTraits<F>::template argument<N>::type>;
			Traits::write(s, std::forward<First>(first));
			Parameters<F, N+1>::serialize(s, std::forward<Rest>(rest)...);
		}
	};
}

template<typename F, typename... Args>
void serializeMethod(Stream& s, Args&&... args)
{
	using Traits = FunctionTraits<F>;
	static_assert(Traits::valid, "Function signature not valid for RPC calls. Check if parameter types are valid");
	static_assert(Traits::arity == sizeof...(Args), "Invalid number of parameters for RPC call.");
	details::Parameters<F, 0>::serialize(s, std::forward<Args>(args)...);
}

template <class T, class MTX=std::mutex>
class Monitor
{
private:
	mutable T m_t;
	mutable MTX m_mtx;

public:
	using Type = T;
	Monitor() {}
	Monitor(T t_) : m_t(std::move(t_)) {}
	template <typename F>
	auto operator()(F f) const -> decltype(f(m_t))
	{
		std::lock_guard<std::mutex> hold{ m_mtx };
		return f(m_t);
	}
};

//
// Future continuations, since std::future::then is not available yet
//
template <typename Fut, typename Work>
decltype(auto) then(Fut f, Work w)
{
	auto fptr = std::make_shared<typename std::decay<Fut>::type>(std::move(f));
	return std::async([fptr = std::move(fptr), w = std::move(w)]
	{
		fptr->wait();
		return w(std::move(*fptr));
	});
}

} // namespace rpc
} // namespace cz
