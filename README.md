# pc98-c-utils

C programs I wrote or modified for use with my systems when doing things
related to NEC's PC-98 platform.

`nhdgen_posix` is a C program to create a NHD hard disk image for use with
emulators from a raw, real PC-98 drive image.

My version is based on a Windows C program, but adapted to work on non-Windows
platforms and uses the GCC compiler instead of Borland. It also has been made
to work on big-endian platforms (you will have to change a preprocessor
definition in the C code for this).

My version will also still build for windows using MinGW, so I think it's just
better in nearly every way than the original.
