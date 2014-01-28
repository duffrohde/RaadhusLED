RaadhusLED
==========

This is the code for putting nice graphics on the RaadhusLED based on e.g. OpenGL shaders. It consists of a 'daemon' that can receive UDP packets containing a frame and send it to the LED controller, and a tool for reading in OpenGL shaders and feeding the frames to the 'daemon'.

## Simple Howto

Compile raadhus_daemon with 'gcc -O2 -o raadhus_daemon raadhus_daemon.c -lpthread' replacing gcc with one that is compatible with the platform it should be executed on.
Compile raadhus_shader with 'make'.

Start raadhus_daemon on your daemon device (e.g. the Linksys WRT54G) and then raadhus_shader on the machine that should execute the shaders.

## raadhus_daemon.c

This program listens on port 1234 and expects UDP packets that in the payload contains a frame to be shown. A frame consists of 24-bit RGB values with a width of 56 and a height of 57. Frames should be sent at roughly 20 FPS since this is the frame rate that is used for the LEDs.

The LEDs are addressed using UDP multicast to the destination port 1097.

## raadhus_shader.c

This program is able to execute OpenGL shaders, grab the frames and send them to the 'daemon'. You most likely need to fit the value below to fit your network setup:

```
#define DESTINATION_HOST "192.168.2.1"
```

To setup shaders just put the filenames in shaders.conf prefixed with the time it should be displayed measured in miliseconds:

```
1000.0 galaxy.glsl
1000.0 hovedbib.glsl
1000.0 hovedbib2.glsl
```
