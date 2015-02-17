/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

/* Copyright (c) 2001-2014, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH2 software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */


#include "mpidimpl.h"
#include "mpiinfo.h"
#include "mpidrma.h"
#include "mpimem.h"
#include "rdma_impl.h"

#include "coll_shmem.h"
#include "bcast_tuning.h"

/* FIXME: get this from OS */
#define MPIDI_CH3_PAGESIZE ((MPI_Aint)4096)
#define MPIDI_CH3_PAGESIZE_MASK (~(MPIDI_CH3_PAGESIZE-1))
#define MPIDI_CH3_ROUND_UP_PAGESIZE(x) ((((MPI_Aint)x)+(~MPIDI_CH3_PAGESIZE_MASK)) & MPIDI_CH3_PAGESIZE_MASK)

#ifdef USE_MPIU_INSTR
MPIU_INSTR_DURATION_EXTERN_DECL(wincreate_allgather);
#endif

static int MPIDI_CH3I_Win_allocate_shm(MPI_Aint size, int disp_unit, MPID_Info *info, MPID_Comm *comm_ptr,
                                       void *base_ptr, MPID_Win **win_ptr);

#define SYNC_WIN_HND 111     
#define SYNC_WIN_MUTEX 112     

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Win_shared_query
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_SHM_Win_shared_query(MPID_Win *win_ptr, int target_rank, MPI_Aint *size, int *disp_unit, void *baseptr)
{
    int comm_size;
    int mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_WIN_SHARED_QUERY);

    MPIDI_RMA_FUNC_ENTER(MPID_STATE_MPIDI_CH3_WIN_SHARED_QUERY);

    comm_size = win_ptr->comm_ptr->local_size;

    /* Scan the sizes to locate the first process that allocated a nonzero
     * amount of space */
    if (target_rank == MPI_PROC_NULL) {
        int i;

        /* Default, if no processes have size > 0. */
        *size               = 0;
        *((void**) baseptr) = NULL;

        for (i = 0; i < comm_size; i++) {
            if (win_ptr->sizes[i] > 0) {
                *size               = win_ptr->sizes[i];
                *disp_unit          = win_ptr->disp_units[i];
                *((void**) baseptr) = win_ptr->shm_base_addrs[i];
                break;
            }
        }

    } else {
        *size               = win_ptr->sizes[target_rank];
        *disp_unit          = win_ptr->disp_units[target_rank];
        *((void**) baseptr) = win_ptr->shm_base_addrs[target_rank];
    }

fn_exit:
    MPIDI_RMA_FUNC_EXIT(MPID_STATE_MPIDI_CH3_WIN_SHARED_QUERY);
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_SHM_Win_free
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_SHM_Win_free(MPID_Win **win_ptr)
{
    int mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_SHM_WIN_FREE);

    MPIDI_RMA_FUNC_ENTER(MPID_STATE_MPIDI_CH3_SHM_WIN_FREE);

    mpi_errno = MPIDI_CH3I_Wait_for_pt_ops_finish(*win_ptr);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);

    /* Free shared memory region */
    if ((*win_ptr)->shm_allocated) {
        /* free shm_base_addrs that's only used for shared memory windows */
        MPIU_Free((*win_ptr)->shm_base_addrs);

        if ((*win_ptr)->shm_segment_len > 0) {
            /* detach from shared memory segment */
            mpi_errno = MPIU_SHMW_Seg_detach((*win_ptr)->shm_segment_handle, (char **)&(*win_ptr)->shm_base_addr,
                                         (*win_ptr)->shm_segment_len);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);

            MPIU_SHMW_Hnd_finalize(&(*win_ptr)->shm_segment_handle);
        }
    }

    /* Free shared process mutex memory region */
    if ((*win_ptr)->shm_mutex && (*win_ptr)->shm_segment_len > 0) {
        MPID_Comm *node_comm_ptr = NULL;
        /* When allocating shared memory region segment, we need comm of processes
           that are on the same node as this process (node_comm).
           If node_comm == NULL, this process is the only one on this node, therefore
           we use comm_self as node comm. */

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            MPI_Comm shmem_comm;
            shmem_comm = (*win_ptr)->comm_ptr->dev.ch.shmem_comm;
            MPID_Comm_get_ptr(shmem_comm, node_comm_ptr);
            MPIU_Assert(node_comm_ptr != NULL);
            if (node_comm_ptr->rank == 0) {
                MPIDI_CH3I_SHM_MUTEX_DESTROY(*win_ptr);
            }
        } else {
            if (g_smpi.my_local_id == 0) {
                MPIDI_CH3I_SHM_MUTEX_DESTROY(*win_ptr);
            }
            MPIU_Free((*win_ptr)->shm_l2g_rank);
        }

        /* detach from shared memory segment */
        mpi_errno = MPIU_SHMW_Seg_detach((*win_ptr)->shm_mutex_segment_handle, (char **)&(*win_ptr)->shm_mutex,
                                         sizeof(MPIDI_CH3I_SHM_MUTEX));
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        MPIU_SHMW_Hnd_finalize(&(*win_ptr)->shm_mutex_segment_handle);
    }

    mpi_errno = MPIDI_Win_free(win_ptr);
    if (mpi_errno != MPI_SUCCESS) { MPIU_ERR_POP(mpi_errno); }

fn_exit:
    MPIDI_RMA_FUNC_EXIT(MPID_STATE_MPIDI_CH3_SHM_WIN_FREE);
    return mpi_errno;

fn_fail:
    goto fn_exit;
}
#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Win_fns_init
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_Win_fns_init(MPIDI_CH3U_Win_fns_t *win_fns)
{
    int mpi_errno = MPI_SUCCESS;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_WIN_FNS_INIT);

    MPIDI_RMA_FUNC_ENTER(MPID_STATE_MPIDI_CH3_WIN_FNS_INIT);

    win_fns->allocate_shm = MPIDI_CH3I_Win_allocate_shm;

    MPIDI_RMA_FUNC_EXIT(MPID_STATE_MPIDI_CH3_WIN_FNS_INIT);

    return mpi_errno;
}


static int send_sync_msgs (MPID_Win **win_ptr, int comm_size, char *serialized_hnd_ptr, int tag)
{
    int i, mpi_errno = MPI_SUCCESS;
    MPI_Request *req;
    MPIU_CHKLMEM_DECL(2);
    MPI_Status *status;
    MPIDI_VC_t *vc = NULL;
    MPIU_CHKLMEM_MALLOC(req, MPI_Request *, comm_size*sizeof(MPI_Request), mpi_errno, "req");
    MPIU_CHKLMEM_MALLOC(status, MPI_Status *, comm_size*sizeof(MPI_Status), mpi_errno, "status");

    for (i = 0; i < comm_size; i++) {
        MPIDI_Comm_get_vc((*win_ptr)->comm_ptr, i, &vc);

        if (vc->pg_rank == MPIDI_Process.my_pg_rank) {
            req[i] = MPI_REQUEST_NULL;
            continue;
        }

        if (vc->smp.local_rank != -1) {
            MPID_Request *req_ptr;
            mpi_errno = MPID_Isend(serialized_hnd_ptr, MPIU_SHMW_GHND_SZ, MPI_BYTE, 
                    i, tag, (*win_ptr)->comm_ptr,
                    MPID_CONTEXT_INTRA_PT2PT, &req_ptr);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            req[i] = req_ptr->handle;
        } else {
            req[i] = MPI_REQUEST_NULL;
        }

    }

    mpi_errno = MPIR_Waitall_impl(comm_size, req, status);
    if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS) MPIU_ERR_POP(mpi_errno);

    /* --BEGIN ERROR HANDLING-- */
    if (mpi_errno == MPI_ERR_IN_STATUS) {
        for (i = 0; i < comm_size; i++) {
            if (status[i].MPI_ERROR != MPI_SUCCESS) {
                mpi_errno = status[i].MPI_ERROR;
                MPIU_ERR_POP(mpi_errno);
            }
        }
    }

fn_exit:
    MPIU_CHKLMEM_FREEALL();
    return mpi_errno;

fn_fail:
    goto fn_exit;
}

static int recv_sync_msgs (MPID_Win **win_ptr, char *serialized_hnd, int tag)
{
    int mpi_errno = MPI_SUCCESS;
    MPI_Request req[1];
    MPI_Status status[1];
    MPID_Request *req_ptr;

    mpi_errno = MPID_Irecv(serialized_hnd, MPIU_SHMW_GHND_SZ, MPI_BYTE, MPI_ANY_SOURCE, tag,
            (*win_ptr)->comm_ptr, MPID_CONTEXT_INTRA_PT2PT, &req_ptr);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);
    req[0] = req_ptr->handle;

    mpi_errno = MPIR_Waitall_impl(1, req, status);
    if (mpi_errno && mpi_errno != MPI_ERR_IN_STATUS) MPIU_ERR_POP(mpi_errno);
    /* --BEGIN ERROR HANDLING-- */
    if (mpi_errno == MPI_ERR_IN_STATUS) {
        if (status[0].MPI_ERROR != MPI_SUCCESS) {
            mpi_errno = status[0].MPI_ERROR;
            MPIU_ERR_POP(mpi_errno);
        }
    }
    /* --END ERROR HANDLING-- */

fn_exit:
    return mpi_errno;

fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3I_Win_allocate_shm
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static int MPIDI_CH3I_Win_allocate_shm(MPI_Aint size, int disp_unit, MPID_Info *info,
                                       MPID_Comm *comm_ptr, void *base_ptr, MPID_Win **win_ptr)
{ 
    int mpi_errno = MPI_SUCCESS;
    void **base_pp = (void **) base_ptr;
    int i, k, comm_size, rank;
    int  node_size, node_rank;
    MPID_Comm *node_comm_ptr;
    MPI_Aint *node_sizes;
    void **node_shm_base_addrs;
    MPI_Aint *tmp_buf;
    int errflag = FALSE;
    int noncontig = FALSE;
    MPIU_CHKPMEM_DECL(6);
    MPIU_CHKLMEM_DECL(3);
    MPI_Comm shmem_comm;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3I_WIN_ALLOCATE_SHM);

    MPIDI_RMA_FUNC_ENTER(MPID_STATE_MPIDI_CH3I_WIN_ALLOCATE_SHM);

    /* If create flavor is MPI_WIN_FLAVOR_ALLOCATE, alloc_shared_noncontig is set to 1 by default. */
    if ((*win_ptr)->create_flavor == MPI_WIN_FLAVOR_ALLOCATE)
        (*win_ptr)->info_args.alloc_shared_noncontig = 1;

    /* Check if we are allowed to allocate space non-contiguously */
    if (info != NULL) {
        int alloc_shared_nctg_flag = 0;
        char alloc_shared_nctg_value[MPI_MAX_INFO_VAL+1];
        MPIR_Info_get_impl(info, "alloc_shared_noncontig", MPI_MAX_INFO_VAL,
                           alloc_shared_nctg_value, &alloc_shared_nctg_flag);
        if ((alloc_shared_nctg_flag == 1)) {
            if (!strncmp(alloc_shared_nctg_value, "true", strlen("true")))
                (*win_ptr)->info_args.alloc_shared_noncontig = 1;
            if (!strncmp(alloc_shared_nctg_value, "false", strlen("false")))
                (*win_ptr)->info_args.alloc_shared_noncontig = 0;
        }
    }

    /* see if we can allocate all windows contiguously */
    noncontig = (*win_ptr)->info_args.alloc_shared_noncontig;

    (*win_ptr)->shm_allocated = TRUE;

    comm_size = (*win_ptr)->comm_ptr->local_size;
    rank      = (*win_ptr)->comm_ptr->rank;

    /* When allocating shared memory region segment, we need comm of processes
       that are on the same node as this process (node_comm).
       If node_comm == NULL, this process is the only one on this node, therefore
       we use comm_self as node comm. */

    /* This node comm only works with hydra, it doesn't work when using mpirun_rsh, so call this
     *  function to create shm comm */

    (*win_ptr)->shm_win_pt2pt = mv2_MPIDI_CH3I_RDMA_Process.shm_win_pt2pt;

    if (likely(!(*win_ptr)->shm_win_pt2pt)) {
        if (!mv2_enable_shmem_collectives && (*win_ptr)->shm_coll_comm_ref == -1) {
            /* Shared memory for collectives */
            mpi_errno = MPIDI_CH3I_SHMEM_COLL_init(MPIDI_Process.my_pg,
                    g_smpi.my_local_id);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }

            /* local barrier */
            mpi_errno = MPIR_Barrier_impl(comm_ptr, &errflag);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }

            /* Memory Mapping shared files for collectives*/
            mpi_errno = MPIDI_CH3I_SHMEM_COLL_Mmap(MPIDI_Process.my_pg,
                    g_smpi.my_local_id);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }

            /* local barrier */
            mpi_errno = MPIR_Barrier_impl(comm_ptr, &errflag);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }

            /* Unlink mapped files so that they get cleaned up when
             *          * process exits */
            MPIDI_CH3I_SHMEM_COLL_Unlink();
            (*win_ptr)->shm_coll_comm_ref = 1;
        } else if ((*win_ptr)->shm_coll_comm_ref > 0) {
            (*win_ptr)->shm_coll_comm_ref++;
        } 

        if((*win_ptr)->comm_ptr->dev.ch.shmem_coll_ok == 0)
            mpi_errno = create_2level_comm((*win_ptr)->comm_ptr->handle, 
                            (*win_ptr)->comm_ptr->local_size, (*win_ptr)->comm_ptr->rank);
        if(mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }           

        shmem_comm = (*win_ptr)->comm_ptr->dev.ch.shmem_comm;
        MPID_Comm_get_ptr(shmem_comm, node_comm_ptr);

        MPIU_Assert(node_comm_ptr != NULL);

        node_size = node_comm_ptr->local_size;
        node_rank = node_comm_ptr->rank;
    }
    else {
        mv2_init_rank_for_barrier(win_ptr);
        node_size = g_smpi.num_local_nodes;
        node_rank = g_smpi.my_local_id;
    }
    MPIR_T_PVAR_TIMER_START(RMA, rma_wincreate_allgather);
    /* allocate memory for the base addresses, disp_units, and
       completion counters of all processes */
    MPIU_CHKPMEM_MALLOC((*win_ptr)->base_addrs, void **,
                        comm_size*sizeof(void *),
                        mpi_errno, "(*win_ptr)->base_addrs");

    MPIU_CHKPMEM_MALLOC((*win_ptr)->shm_base_addrs, void **,
                        comm_size*sizeof(void *),
                        mpi_errno, "(*win_ptr)->shm_base_addrs");

    MPIU_CHKPMEM_MALLOC((*win_ptr)->sizes, MPI_Aint *, comm_size*sizeof(MPI_Aint),
                        mpi_errno, "(*win_ptr)->sizes");

    MPIU_CHKPMEM_MALLOC((*win_ptr)->disp_units, int *, comm_size*sizeof(int),
                        mpi_errno, "(*win_ptr)->disp_units");

    MPIU_CHKPMEM_MALLOC((*win_ptr)->all_win_handles, MPI_Win *,
                        comm_size*sizeof(MPI_Win),
                        mpi_errno, "(*win_ptr)->all_win_handles");

    MPIU_CHKPMEM_MALLOC((*win_ptr)->pt_rma_puts_accs, int *,
                        comm_size*sizeof(int),
                        mpi_errno, "(*win_ptr)->pt_rma_puts_accs");
    for (i=0; i<comm_size; i++)	(*win_ptr)->pt_rma_puts_accs[i] = 0;

    /* get the sizes of the windows and window objectsof
       all processes.  allocate temp. buffer for communication */
    MPIU_CHKLMEM_MALLOC(tmp_buf, MPI_Aint *, 3*comm_size*sizeof(MPI_Aint), mpi_errno, "tmp_buf");

    /* FIXME: This needs to be fixed for heterogeneous systems */
    tmp_buf[3*rank]   = (MPI_Aint) size;
    tmp_buf[3*rank+1] = (MPI_Aint) disp_unit;
    tmp_buf[3*rank+2] = (MPI_Aint) (*win_ptr)->handle;

    mpi_errno = MPIR_Allgather_impl(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                    tmp_buf, 3 * sizeof(MPI_Aint), MPI_BYTE,
                                    (*win_ptr)->comm_ptr, &errflag);
    MPIR_T_PVAR_TIMER_START(RMA, rma_wincreate_allgather);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);
    MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");

    if ((*win_ptr)->create_flavor != MPI_WIN_FLAVOR_SHARED) {
        MPIU_CHKLMEM_MALLOC(node_sizes, MPI_Aint *, node_size*sizeof(MPI_Aint), mpi_errno, "node_sizes");
        for (i = 0; i < node_size; i++) node_sizes[i] = 0;
    }
    else {
        node_sizes = (*win_ptr)->sizes;
    }

    (*win_ptr)->shm_segment_len = 0;
    k = 0;
    for (i = 0; i < comm_size; ++i) {
        (*win_ptr)->sizes[i]           = tmp_buf[k++];
        (*win_ptr)->disp_units[i]      = (int) tmp_buf[k++];
        (*win_ptr)->all_win_handles[i] = (MPI_Win) tmp_buf[k++];

        if ((*win_ptr)->create_flavor != MPI_WIN_FLAVOR_SHARED) {
            /* If create flavor is not MPI_WIN_FLAVOR_SHARED, all processes on this
               window may not be on the same node. Because we only need the sizes of local
               processes (in order), we copy their sizes to a seperate array and keep them
               in order, fur purpose of future use of calculating shm_base_addrs. */
            /* Since, this intranode_table is not populated when using run_rsh,
             * so we use our rank info */
            MPIDI_VC_t *vc;
            MPIDI_Comm_get_vc(comm_ptr, i, &vc);
            int l_rank = vc->smp.local_rank;
            if (l_rank >= 0) {
                MPIU_Assert(l_rank < node_size);
                node_sizes[l_rank] = (*win_ptr)->sizes[i];
            } 
        }
    }
    for (i = 0; i < node_size; i++) {
        if (noncontig)
            /* Round up to next page size */
            (*win_ptr)->shm_segment_len += MPIDI_CH3_ROUND_UP_PAGESIZE(node_sizes[i]);
        else
            (*win_ptr)->shm_segment_len += node_sizes[i];
    }

    if ((*win_ptr)->shm_segment_len == 0) {
        (*win_ptr)->base = NULL;
    }

    else {
    mpi_errno = MPIU_SHMW_Hnd_init(&(*win_ptr)->shm_segment_handle);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);

    if (node_rank == 0) {
        char *serialized_hnd_ptr = NULL;

        /* create shared memory region for all processes in win and map */
        mpi_errno = MPIU_SHMW_Seg_create_and_attach((*win_ptr)->shm_segment_handle, (*win_ptr)->shm_segment_len,
                                                    (char **)&(*win_ptr)->shm_base_addr, 0);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        /* serialize handle and broadcast it to the other processes in win */
        mpi_errno = MPIU_SHMW_Hnd_get_serialized_by_ref((*win_ptr)->shm_segment_handle, &serialized_hnd_ptr);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Shmem_Bcast_MV2(serialized_hnd_ptr, MPIU_SHMW_GHND_SZ, MPI_BYTE, 0, node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else { 
            /*Use pt2pt if the number of shared memory communicator is large */
            mpi_errno = send_sync_msgs(win_ptr, comm_size, serialized_hnd_ptr, SYNC_WIN_HND);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        /* wait for other processes to attach to win */
        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Barrier_impl(node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else {
            mpi_errno = MPIDI_CH3I_barrier_in_rma(win_ptr, rank, node_size, comm_size);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        /* unlink shared memory region so it gets deleted when all processes exit */
        mpi_errno = MPIU_SHMW_Seg_remove((*win_ptr)->shm_segment_handle);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

    } else {
        char serialized_hnd[MPIU_SHMW_GHND_SZ] = {0};

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Shmem_Bcast_MV2(serialized_hnd, MPIU_SHMW_GHND_SZ, MPI_BYTE, 0, node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else {
            mpi_errno = recv_sync_msgs(win_ptr, serialized_hnd, SYNC_WIN_HND);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        mpi_errno = MPIU_SHMW_Hnd_deserialize((*win_ptr)->shm_segment_handle, serialized_hnd, strlen(serialized_hnd));
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        /* attach to shared memory region created by rank 0 */
        mpi_errno = MPIU_SHMW_Seg_attach((*win_ptr)->shm_segment_handle, (*win_ptr)->shm_segment_len,
                                         (char **)&(*win_ptr)->shm_base_addr, 0);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Barrier_impl(node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else {
            mpi_errno = MPIDI_CH3I_barrier_in_rma(win_ptr, rank, node_size, comm_size);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }
    }

    /* Allocated the interprocess mutex segment. */
    mpi_errno = MPIU_SHMW_Hnd_init(&(*win_ptr)->shm_mutex_segment_handle);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);

    if (node_rank == 0) {
        char *serialized_hnd_ptr = NULL;

        /* create shared memory region for all processes in win and map */
        mpi_errno = MPIU_SHMW_Seg_create_and_attach((*win_ptr)->shm_mutex_segment_handle, sizeof(MPIDI_CH3I_SHM_MUTEX),
                                                    (char **)&(*win_ptr)->shm_mutex, 0);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        MPIDI_CH3I_SHM_MUTEX_INIT(*win_ptr);

        /* serialize handle and broadcast it to the other processes in win */
        mpi_errno = MPIU_SHMW_Hnd_get_serialized_by_ref((*win_ptr)->shm_mutex_segment_handle, &serialized_hnd_ptr);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Shmem_Bcast_MV2(serialized_hnd_ptr, MPIU_SHMW_GHND_SZ, MPI_CHAR, 0, node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else { 
            /*Use pt2pt if the number of shared memory communicator is large */
            mpi_errno = send_sync_msgs(win_ptr, comm_size, serialized_hnd_ptr, SYNC_WIN_MUTEX);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        /* wait for other processes to attach to win */
        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Barrier_impl(node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else {
            mpi_errno = MPIDI_CH3I_barrier_in_rma(win_ptr, rank, node_size, comm_size);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        /* unlink shared memory region so it gets deleted when all processes exit */
        mpi_errno = MPIU_SHMW_Seg_remove((*win_ptr)->shm_mutex_segment_handle);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);
    } else {
        char serialized_hnd[MPIU_SHMW_GHND_SZ] = {0};

        /* get serialized handle from rank 0 and deserialize it */
        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Shmem_Bcast_MV2(serialized_hnd, MPIU_SHMW_GHND_SZ, MPI_CHAR, 0, node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else { 
            /*Use pt2pt if the number of shared memory communicator is large */
            mpi_errno = recv_sync_msgs(win_ptr, serialized_hnd, SYNC_WIN_MUTEX);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        }

        mpi_errno = MPIU_SHMW_Hnd_deserialize((*win_ptr)->shm_mutex_segment_handle, serialized_hnd, strlen(serialized_hnd));
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        /* attach to shared memory region created by rank 0 */
        mpi_errno = MPIU_SHMW_Seg_attach((*win_ptr)->shm_mutex_segment_handle, sizeof(MPIDI_CH3I_SHM_MUTEX),
                                         (char **)&(*win_ptr)->shm_mutex, 0);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);

        if (likely(!(*win_ptr)->shm_win_pt2pt)) {
            mpi_errno = MPIR_Barrier_impl(node_comm_ptr, &errflag);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");
        } else {
            mpi_errno = MPIDI_CH3I_barrier_in_rma(win_ptr, rank, node_size, comm_size);
            if (mpi_errno) MPIU_ERR_POP(mpi_errno);
            sleep(1);
        }
    }

    /* compute the base addresses of each process within the shared memory segment */
    {
        int curr_rank;
        if ((*win_ptr)->create_flavor != MPI_WIN_FLAVOR_SHARED) {
            /* If create flavor is not MPI_WIN_FLAVOR_SHARED, all processes on this
               window may not be on the same node. Because we only need to calculate
               local processes' shm_base_addrs using local processes's sizes,
               we allocate a temporary array to place results and copy results
               back to shm_base_addrs on the window at last. */
            MPIU_CHKLMEM_MALLOC(node_shm_base_addrs, void **, node_size*sizeof(void*),
                                mpi_errno, "node_shm_base_addrs");
        }
        else {
            node_shm_base_addrs = (*win_ptr)->shm_base_addrs;
        }

        char *cur_base = (*win_ptr)->shm_base_addr;
        curr_rank = 0;
        node_shm_base_addrs[0] = (*win_ptr)->shm_base_addr;
        for (i = 1; i < node_size; ++i) {
            if (node_sizes[i]) {
                if (noncontig) {
                    node_shm_base_addrs[i] = cur_base + MPIDI_CH3_ROUND_UP_PAGESIZE(node_sizes[curr_rank]);
                } else {
                    node_shm_base_addrs[i] = cur_base + node_sizes[curr_rank];
                }
                cur_base = node_shm_base_addrs[i];
                curr_rank = i;
            } else {
                node_shm_base_addrs[i] = NULL; /* FIXME: Is this right? */
            }
        }

        if ((*win_ptr)->create_flavor != MPI_WIN_FLAVOR_SHARED) {
            /* if MPI_WIN_FLAVOR_SHARED is not set, copy from node_shm_base_addrs to
               (*win_ptr)->shm_base_addrs */
            for (i = 0; i < comm_size; i++) {
#if defined(CHANNEL_MRAIL)
            MPIDI_VC_t *vc;
            MPIDI_Comm_get_vc(comm_ptr, i, &vc);
            int l_rank = vc->smp.local_rank;
            if (l_rank >=0 ) {
                MPIU_Assert(l_rank < node_size);
                (*win_ptr)->shm_base_addrs[i] = node_shm_base_addrs[l_rank];
            } 
#else 
            if ((*win_ptr)->comm_ptr->intranode_table[i] >= 0) {
                MPIU_Assert((*win_ptr)->comm_ptr->intranode_table[i] < node_size);
                (*win_ptr)->shm_base_addrs[i] = node_shm_base_addrs[(*win_ptr)->comm_ptr->intranode_table[i]];
            } 
#endif
            else
                (*win_ptr)->shm_base_addrs[i] = NULL;
            }
        }
    }

    (*win_ptr)->base = (*win_ptr)->shm_base_addrs[rank];
    }

    /* get the base addresses of the windows.  Note we reuse tmp_buf from above
       since it's at least as large as we need it for this allgather. */
    tmp_buf[rank] = MPIU_PtrToAint((*win_ptr)->base);

    mpi_errno = MPIR_Allgather_impl(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                                    tmp_buf, 1, MPI_AINT,
                                    (*win_ptr)->comm_ptr, &errflag);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);
    MPIU_ERR_CHKANDJUMP(errflag, mpi_errno, MPI_ERR_OTHER, "**coll_fail");

    for (i = 0; i < comm_size; ++i)
        (*win_ptr)->base_addrs[i] = MPIU_AintToPtr(tmp_buf[i]);

    *base_pp = (*win_ptr)->base;

#if defined(CHANNEL_MRAIL)
    (*win_ptr)->my_id = rank;
    (*win_ptr)->comm_size = comm_size;
    /* -- OSU-MPI2 uses extended CH3 interface */
    if (comm_ptr->comm_kind == MPID_INTRACOMM)
    {
        /* Only Intracomm supports drect one-sided communication*/
        /* Intercomm is not well supported currently,
         * directly fall back to pt2pt implementation if we use inter
         * communicator */
        (*win_ptr)->fall_back = 0;
        MPIDI_CH3I_RDMA_win_create(*base_pp, size, comm_size,
                rank, win_ptr, comm_ptr);
    }
#endif /* defined(_OSU_MVAPICH_) */

    /* Provide operation overrides for this window flavor */
    (*win_ptr)->RMAFns.Win_shared_query = MPIDI_CH3_SHM_Win_shared_query;
    (*win_ptr)->RMAFns.Win_free         = MPIDI_CH3_SHM_Win_free;

    (*win_ptr)->use_direct_shm = 1;
fn_exit:
    MPIU_CHKLMEM_FREEALL();
    MPIDI_RMA_FUNC_EXIT(MPID_STATE_MPIDI_CH3I_WIN_ALLOCATE_SHM);
    return mpi_errno;
    /* --BEGIN ERROR HANDLING-- */
fn_fail:
    MPIU_CHKPMEM_REAP();
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}
