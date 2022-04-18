// config.h

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "exv_conf.h"
////////////////////////////////////////

///// Path separator marcos      /////
#ifndef EXV_SEPARATOR_STR
#if defined(WIN32) && !defined(__CYGWIN__)
#define EXV_SEPARATOR_STR "\\"
#define EXV_SEPARATOR_CHR '\\'
#else
#define EXV_SEPARATOR_STR "/"
#define EXV_SEPARATOR_CHR '/'
#endif
#endif
//////////////////////////////////////

#endif  // _CONFIG_H_
