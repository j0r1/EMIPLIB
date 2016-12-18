EMIPLIB
=======

Introduction
------------

The name EMIPLIB means 'EDM Media over IP library'. This library was developed
at the [Expertise Centre for Digital Media (EDM)](http://www.edm.uhasselt.be), 
a research institute of the [Hasselt University](http://www.uhasselt.be). As 
the name suggests the goal of the library is to make it easier to stream 
several kinds of media, including (but not limited to) audio and video. For
more detailed information about the library, please refer to the library 
documentation.

License
-------

The license that applies to the library is the LGPL. However, it is possible 
to specify that you wish to use GPL licensed components as well, which then 
causes the GPL to apply to the entire library. The license texts of these 
two licenses can be found in the files `LICENSE.LGPL`and `LICENSE.GPL` of the 
source code archive.

Note that when creating an application, you have to take the licenses of 
other libraries into account too. For example, if your application uses the 
Qt component and you accepted the GPL license for the Qt library, linking 
with the Qt library requires your application to be GPL too. Similarly, if 
your version of libavcodec was compiled as a GPL library, using the 
libavcodec component of emiplib will require your application to be GPL 
too, since you'll need to link against your GPL version of libavcodec.

Installation
------------

The library depends on the JThread and JRTPLIB libraries, which can be
found at the following sites:

  - [http://research.edm.uhasselt.be/jori/jthread](http://research.edm.uhasselt.be/jori/jthread)
  - [http://research.edm.uhasselt.be/jori/jrtplib](http://research.edm.uhasselt.be/jori/jrtplib)

Note that for this version, at least JThread 1.3.0 and JRTPLIB 3.10.0 are
required.

The compilation of the library can be done using the CMake build
system. In case extra include directories or libraries are needed, you 
can use the `ADDITIONAL_` CMake variables to specify these. They will 
be stored in both the resulting EMIPLIB CMake configuration file and the 
pkg-config file.

For example, to be able to compile the DirectShow video input component,
you may need to enter something like this in the `ADDITIONAL_INCLUDE_DIRS`
variable:

    C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Include;
    C:/Program Files (x86)/Microsoft DirectX SDK (August 2007)/Include

And something like this in the `ADDITIONAL_LIBRARIES_GENERAL` variable:
   
    C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/strmiids.lib

To install JThread, JRTPLIB and EMIPLIB in a MS-Windows environment, an
easy way is the following. For each project, use the same installation
directory, and specify it using the CMake `CMAKE_INSTALL_PREFIX` variable
_before_ running the CMake configure step. If you then build and install 
JThread and JRTPLIB, the EMIPLIB configure step should be able to detect
them without any problem.

Contact
-------

The library home page can be found at
[http://research.edm.uhasselt.be/emiplib/](http://research.edm.uhasselt.be/emiplib/),
documentation is available at [emiplib.readthedocs.io](http://emiplib.readthedocs.io).

Questions, comments and bug reports can be sent to
[jori.liesenborgs@gmail.com](mailto:jori.liesenborgs@gmail.com)

Acknowledgments
---------------

We would like to thank Jutta Degener and Carsten Bormann for writing
their GSM codec and allowing us to include it in EMIPLIB. The
copyright and disclaimer of the GSM codec can be found in the file
`src/thirdparty/gsm/COPYRIGHT`.

We would also like to thank Ron Frederick for making his LPC codec
available to the public and for allowing us to include a derived
version in this library.

The component which converts JPEG data into an uncompressed image
uses the Tiny Jpeg Decoder by Luc Saillard. Thanks for making this
publically available. Please see the source code in 
`src/thirdparty/tinyjpeg` for more information about copyright and
disclaimer.

