# LUI (working title)
LUI is a declarative GUI library which uses lua to define the GUI. A  C++
backend is used to update the GUI and create draw lists which are then
returned to the user.

For 1.0, the primary focus will be on creating menus such as main menus, and
options menus. If performance permits, and good data bindings can be created,
it will later be expanded to be used for debug/editor tools as well.

Currently the library is linux-only because of the automatic reloading and
server implementation.

# Disclaimer
The library is in development and it (as well as the API) will probably change
before 1.0.

# Getting started
An example project which uses SDL and OpenGL is included. The extensions are
pretty well-commented, and most of the code is easily readable.

Some quick examples can be found in the wiki.

## Dependencies
Currently the only mandatory dependencies not included in the repo are lua 5.1
and freetype2. SDL2 is required to build the supplied CMakeLists as it includes
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
should take at most a handful of milliseconds.

Parsing and building the GUI should not take so long that it is a bother
(subjective).
## Minimal dependencies
The dependencies should be kept to a minimum. If a dependency is introduced it
will either be a well-known and widely used (meaning you probably already have
it or trust it), or it will be included.

# Roadmap
## 1.0
1.0 is when all of the following features are implemented and stable:
### Widgets
* ~~Text~~
* ~~Button~~
* ~~Checkbox~~
* ~~Color~~
* ~~Dropdown~~
* ~~Slider~~
* Scrollbar
* Single-line text input
* Texture
### Layouts
* ~~Linear~~
* ~~Scroll~~
* ~~Floating~~
* Accordion/Collapse
* Outline
### General features
* Cross-platform GUI server implementation
* Cross-platform file reloading
* Cross-platform extension registration
* Some non-intrusive way to build extensions into the main binary instead
* CPU text clipping (when generating text)
* Better font support (different fonts and sizes)
* Texture support
* Better, uniform, default style
* Switching between different GUIs, e.g. main menu -> options menu, or between
different categories in the options menu

## Additional features
These are features that are currently undecided. Either due to them not being
important enough, or due to implementation difficulties
* On-demand parsing and building
  
  Parsing the entire UI at once will be slow and unnecessary. Instead, it would
  be nice to parse only the parts of the UI that are needed to display it, and
  then parse e.g. elements in a dropdown only once the dropdown has been shown.
  Performance needs to be kept in mind, and a per-object "preload" flag should
  probably be implemented to avoid lag spikes.

* Data bindings
  
  I would like to avoid naming widgets and interacting with them by searching
  for them. Message-passing is fine for when a button is pressed and so on, but
  a widget might need to source its data from outside the lua context. A simple,
  non-cumbersome way of getting data into and out of the widget is necessary.

## 1.0+
### Widgets
* Color picker
### Layouts
* Floating window
* Resizable linear
* Menu bar
* Tree
### General features
* Right-click context menus
* Dockable windows
* Tabs

# License
LUI is licensed under the MIT licence. LICENSE.txt has more information.