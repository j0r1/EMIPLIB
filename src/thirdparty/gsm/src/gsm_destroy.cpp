#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_GSM

/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header: /home/jori/CVSROOT/jvoiplib/src/thirdparty/gsm/src/gsm_destroy.cpp,v 1.2 2004/09/02 14:47:35 jori Exp $ */

#include "gsm.h"
#include "config.h"
#include "proto.h"

#ifdef	HAS_STDLIB_H
#	include	<stdlib.h>
#else
#	ifdef	HAS_MALLOC_H
#		include 	<malloc.h>
#	else
		extern void free();
#	endif
#endif

void gsm_destroy P1((S), gsm S)
{
	if (S) free((char *)S);
}



#endif // MIPCONFIG_SUPPORT_GSM
