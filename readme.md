# LUI (working title)
LUI is a declarative GUI library which uses lua to define the GUI. A  C++
backend is used to update the GUI and create draw lists which are then
returned to the user.

Currently the library is linux-only because of the automatic reloading and
server implementation.

# Disclamer
The library is in development and it (as well as the API) will probably change
before 1.0.

# Getting started
An example project which uses SDL and OpenGL is included.

More in-depth information is available in the wiki.

## Dependencies
Currently the only mandatory dependencies not included in the repo are lua 5.1
and freetype2. SDL2 is required to build the supplied makefile as it includes
a simple example application.

## Building
A CMakeLists.txt is included, and requires a C++14 compiler to run. This
requirement will be lowered at a later time.

The default cmake file also requires SDL2 since it also builds an example
application.

# Goals
## Easy to use
The library should be easy to use.

Using lua has a couple of benefits:
* Functions are first-class citizens, so it is easy to make e.g. a button either
pass a message to the user, or call a lua function which does some work.
* The table structure makes it easy to create "macros" for creating GUI widgets.
Functions can return tables, meaning redundant typing can be reduced to a single
function call.
* Changing the GUI at runtime is as easy as reassigning a table.
## Easy to extend
The widget API should be well-defined and easy to work with.
* The API has a number of functions where most of them are optional. This means
you only have to write out what you need, and simply leave out the rest.
* Each element is small and serves a singular purpose. Complicated layouts are
created by combining simple building blocks.
* Widgets can be created by other widgets though lua. For instance, a dropdown
menu might require a scrollbar, which is automatically created.
## Real-time capable
The GUI should run with a minimal performance impact. Rendering a given UI
should take a most a handful of milliseconds.

Parsing and building the GUI should not take so long that it is a bother
(subjective).
## Minimal dependencies
The dependencies should be kept to a minimum. If a dependency is introduced it
will either be a well-known and widely used (meaning you probably already have
it or trust it), or it will be included.

# License
MIT
