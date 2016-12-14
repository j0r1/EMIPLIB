#include "mipconfig.h"

#ifdef MIPCONFIG_SUPPORT_GSM

/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header: /home/jori/CVSROOT/jvoiplib/src/thirdparty/gsm/src/gsm_option.cpp,v 1.2 2004/09/02 14:47:35 jori Exp $ */

#include "private.h"

#include "gsm.h"
#include "proto.h"

int gsm_option P3((r, opt, val), gsm r, int opt, int * val)
{
	int 	result = -1;

	switch (opt) {
	case GSM_OPT_LTP_CUT:
#ifdef 	LTP_CUT
		result = r->ltp_cut;
		if (val) r->ltp_cut = *val;
#endif
		break;

	case GSM_OPT_VERBOSE:
#ifndef	NDEBUG
		result = r->verbose;
		if (val) r->verbose = *val;
#endif
		break;

	case GSM_OPT_FAST:

#if	defined(FAST) && defined(USE_FLOAT_MUL)
		result = r->fast;
		if (val) r->fast = !!*val;
#endif
		break;

	case GSM_OPT_FRAME_CHAIN:

#ifdef WAV49
		result = r->frame_chain;
		if (val) r->frame_chain = *val;
#endif
		break;

	case GSM_OPT_FRAME_INDEX:

#ifdef WAV49
		result = r->frame_index;
		if (val) r->frame_index = *val;
#endif
		break;

	case GSM_OPT_WAV49:

#ifdef WAV49 
		result = r->wav_fmt;
		if (val) r->wav_fmt = !!*val;
#endif
		break;

	default:
		break;
	}
	return result;
}


#endif // MIPCONFIG_SUPPORT_GSM
