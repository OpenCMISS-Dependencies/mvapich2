/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpid_nem_impl.h"

#include "upmi.h"

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Abort
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_Abort(int exit_code, const char *error_msg)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_ABORT);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_ABORT);

    UPMI_ABORT(exit_code, error_msg);

    /* if abort returns for some reason, exit here */

    MPIU_Error_printf("%s", error_msg);
    fflush(stderr);

    MPIU_Exit(exit_code);

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_ABORT);
    return MPI_ERR_INTERN;
}
