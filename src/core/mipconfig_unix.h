/*
    
  This file is a part of EMIPLIB, the EDM Media over IP Library.
  
  Copyright (C) 2006-2009  Hasselt University - Expertise Centre for
                      Digital Media (EDM) (http://www.edm.uhasselt.be)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  
  USA

*/

#ifndef MIPCONFIG_UNIX_H

#define MIPCONFIG_UNIX_H

// No GPL components will be compiled

// Little endian system

// No support for libsndfile

#define MIPCONFIG_SUPPORT_AUDIOFILE

#define MIPCONFIG_SUPPORT_QT

#define MIPCONFIG_SUPPORT_ALSA

#define MIPCONFIG_SUPPORT_SPEEX

#define MIPCONFIG_SUPPORT_GSM

#define MIPCONFIG_SUPPORT_LPC

#define MIPCONFIG_SUPPORT_AVCODEC

// No old libavcodec support

// No support for the Intel IPP library

#define MIPCONFIG_SUPPORT_ESD

// No support for JACK

#define MIPCONFIG_SUPPORT_VIDEO4LINUX

#define MIPCONFIG_SUPPORT_OSS

#define MIPCONFIG_SUPPORT_SDLAUDIO

// No support for OpenAL

// No support for PortAudio

#endif // MIPCONFIG_UNIX_H

