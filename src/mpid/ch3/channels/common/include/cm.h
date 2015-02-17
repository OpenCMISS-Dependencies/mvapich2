/* Copyright (c) 2001-2014, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#ifndef MVAPICH2_GEN2_CM_H
#define MVAPICH2_GEN2_CM_H

#include "mpichconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <assert.h>

#include "rdma_impl.h"

#define MAX_PG_ID_SIZE 256      /* Max space allocatd for pg_id */
extern pthread_mutex_t cm_conn_state_lock;


/*
MPICM_Init_UD
Initialize MPICM based on UD connection, need to be called before calling
any of following interfaces, it will return a qpn for the established ud qp,
mpi lib needs to exchange the ud qpn with other procs
*/
int MPICM_Init_UD_CM(uint32_t * ud_qpn);

/*
MPICM_Init_ud_struct
Provide connect information to UD
*/
int MPICM_Init_UD_struct(MPIDI_PG_t *);

/*
MPICM_Init_Local_UD_struct
Provide local connect information to UD
*/
int MPICM_Init_Local_UD_struct(MPIDI_PG_t *);

/*
MPICM_Create_ud_threads
Create UD worker threads
*/
int MPICM_Create_UD_threads(void);
/*
MPICM_Finalize_UD
Cleanup ud related data structures
*/
int MPICM_Finalize_UD(void);

#ifdef _ENABLE_XRC_
typedef struct _xrc_hash {
    struct _xrc_hash *next;
    MPIDI_VC_t *vc;
    uint32_t xrc_qp_dst;        /* Dst rank of original qp */
} xrc_hash_t;
#define xrc_hash_s (sizeof (xrc_hash_t))

#define XRC_HASH_SIZE       1024
#define XRC_HASH_MASK       (XRC_HASH_SIZE - 1)

xrc_hash_t *xrc_hash[XRC_HASH_SIZE];
void cm_activate_xrc_qp_reuse(int peer_rank);
int compute_xrc_hash(uint32_t v);
void clear_xrc_hash(void);
void add_vc_xrc_hash(MPIDI_VC_t * vc);
int cm_send_xrc_cm_msg(MPIDI_VC_t * vc, MPIDI_VC_t * orig_vc);
int cm_qp_reuse(MPIDI_VC_t * vc, MPIDI_VC_t * orig);

#endif /* _ENABLE_XRC_ */

#define MV2_QP_NEW      0
#define MV2_QP_REUSE    1

#define MV2_QPT_RC      1
#define MV2_QPT_XRC     2

extern int mv2_pmi_max_keylen;
extern int mv2_pmi_max_vallen;
extern char *mv2_pmi_key;
extern char *mv2_pmi_val;

/*
 * mv2_allocate_pmi_keyval
 * Allocate a Key-Value pair of correct length
 * Return 0 on success, non-zero on failure
 */
int mv2_allocate_pmi_keyval(void);

/*
 * mv2_free_pmi_keyval
 * Free a previously allocated Key-Value pair
 */
void mv2_free_pmi_keyval(void);

#endif /* MVAPICH2_GEN2_CM_H */
