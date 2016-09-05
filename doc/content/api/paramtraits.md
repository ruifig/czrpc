+++
date = "2016-08-15T23:26:03+01:00"
next = "/api/table"
prev = "/api/overview"
title = "ParamTraits"
toc = true
weight = 202
+++

# Parameter traits

By default, czrpc accepts fundamental types and some common complex types as RPC parameters or return values.
It is possible to implement suport for your own types, by specalizing the **ParamTraits** template for that type.

For example, for a custom type **Foo*...

```cpp
struct Foo
{
	int val;
};
```

By specializing **ParamTraits**, czrpc knows how to deal with **Foo**

```cpp
//
// Make Foo instances usable as RPC parameters
template<>
struct cz::rpc::ParamTraits<Foo> : public cz::rpc::DefaultParamTraits<Foo>
{
	template<typename S>
	static void write(S& s, const Foo& v) {
		s << v.val;
	}
	template<typename S>
	static void read(S&s, Foo& v) {
		s >> v.val;
	}
};
```






