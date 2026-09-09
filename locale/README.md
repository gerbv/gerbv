# Gerbv translations

Currently there are translations to
* Chinese Simplified (zh_CN)
* Chinese Traditional (zh_TW)
* Dutch (nl)
* French (fr)
* German (de)
* Japanese (ja)
* Korean (ko)
* Russian (ru)
* Spanish (es)
* Swedish (sv)

It uses the GNU gettext project to handle the translations.

## CMake support

 The blog [Getting started with GNU gettext for C++](https://erri120.github.io/posts/2022-05-05/) from
 eri120 was a great source of information.

 There is a matching CMake script [here](https://github.com/erri120/Gettext-CMake/tree/master) which is
 included in the `cmake` directory.


## File setup

When we run the CMake gettext script it assumes the .po files are located at
the certain places. So we copy the translations into the correct places 
with correct names instead of storing them in `git` using that directory structure.

The advantage is that the translations does not get "dirty" from the perspective of `git` when CMake and
`gettext` does its things. And it is a more shallow directory structure that clearly signals what languages they
are a translation to.

What the script really should do is to put the generated files in the build directory instead.
