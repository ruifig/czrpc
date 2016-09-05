+++
date = "2016-08-15T22:33:06+01:00"
next = "/introduction/a-small-taste"
prev = "/introduction"
title = "What is czrpc?"
toc = true
weight = 101

+++

czrpc is a C++ RPC framework that requires no code generation pre-build step.
It is intended to be used between trusted parties such as several servers, for efficient RPC calls. Thus, it only implements the bare minimum.


It was originally created as part of a [technical article for my blog](http://www.crazygaze.com/blog/2016/06/06/modern-c-lightweight-binary-rpc-framework-without-code-generation/).


Features:

* Requires no code generation pre-build step
* Type-safe (Detects **at compile time** invalid RPC calls)
* Relatively small API (considering it requires no code generation)
* Multiple ways to handle RPC replies (asynchronous handler or std::future)
* Allows the use of potentially any type in RPC parameters (or return values), provided the user implements the required type traits
* Bidirectional RPCs (A server can call RPCs on a client)
* Non intrusive (allows wrapping third party classes to use for RPC calls)
* Minimal bandwidth overhead per RPC call
* No external dependencies
* The user can specify its own transport
* Modern C++ (C++11)

Some of the features might be seen as handicaps, but I focused on what I needed.
For example, there is built-in secure/encrypted connections, and clients are trusted.
All this is by design, since the framework is intended to be used between trusted parties.

