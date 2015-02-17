/* -*- Mode: C; c-basic-offset:4 ; -*- */
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

#include "mpidi_ch3_impl.h"
#include "dreg.h"

#ifdef DEBUG
#define DEBUG_PRINT(args...)                                  \
do {                                                          \
    int rank;                                                 \
    UPMI_GET_RANK(&rank);                                      \
    fprintf(stderr, "[%d][%s:%d] ", rank, __FILE__, __LINE__);\
    fprintf(stderr, args);                                    \
    fflush(stderr); \
} while (0)
#else
#define DEBUG_PRINT(args...)
#endif

int rts_send = 0;
int cts_recv = 0;

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Prepare_rndv
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static inline void MPIDI_CH3_Prepare_rndv(MPIDI_VC_t *vc, MPID_Request *sreq)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_PREPARE_RNDV);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_PREPARE_RNDV);

#if defined(_ENABLE_CUDA_) && defined(HAVE_CUDA_IPC)
    if (rdma_enable_cuda
        && rdma_cuda_ipc) {
        if (cudaipc_stage_buffered &&
            sreq->dev.iov[0].MPID_IOV_LEN < cudaipc_stage_buffered_limit) {
            if (MPIDI_CH3I_MRAIL_Prepare_rndv_cuda_ipc_buffered (vc, sreq)) {
                goto fn_exit;
            }
        } else {
            if (MPIDI_CH3I_MRAIL_Prepare_rndv_cuda_ipc (vc, sreq)) {
                goto fn_exit;
            }
        }
    }
#endif

    if (SMP_INIT && vc->smp.local_nodes >= 0) { 
        sreq->mrail.protocol = MV2_RNDV_PROTOCOL_R3;
        sreq->mrail.d_entry = NULL;
    } else 
    {
        MPIDI_CH3I_MRAIL_Prepare_rndv(vc, sreq);
    }

fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_PREPARE_RNDV);
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_iStartRndvMsg
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_iStartRndvMsg(MPIDI_VC_t * vc,
                            MPID_Request * sreq, MPIDI_CH3_Pkt_t * rts_pkt)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_ISTARTRNDVMSG);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_ISTARTRNDVMSG);
    int mpi_errno = MPI_SUCCESS;
    DEBUG_PRINT("ch3_istartrndvmsg\n");
    MPIDI_DBG_PRINTF((50, FCNAME, "entering"));
    /* If send queue is empty attempt to send
       data, queuing any unsent data. */
#ifdef CKPT
    MPIDI_CH3I_CR_lock();
#endif

    ++rts_send;
    if (MPIDI_CH3I_SendQ_empty(vc)) {   /* MT */
        MPID_Request * send_req;

        MPIDI_CH3_Pkt_rndv_req_to_send_t *rndv_pkt =
            &(rts_pkt->rndv_req_to_send);
        /* MT - need some signalling to lock down our right to use the
           channel, thus insuring that the progress engine does also try to
           write */
        MPIDI_CH3_Prepare_rndv(vc, sreq);
        MPIDI_CH3I_MRAIL_SET_PKT_RNDV(rndv_pkt, sreq);
        if(1 == sreq->mrail.rndv_buf_alloc) {
            MPIDI_CH3I_MRAIL_REVERT_RPUT(sreq);
            if (MV2_RNDV_PROTOCOL_RGET == rndv_pkt->rndv.protocol) {
                rndv_pkt->rndv.protocol = MV2_RNDV_PROTOCOL_RPUT;
            }
        }

        if ((mpi_errno = MPIDI_CH3_iStartMsg(
            vc,
            rndv_pkt,
            sizeof(MPIDI_CH3_Pkt_rndv_req_to_send_t),
            &send_req)) != MPI_SUCCESS)
        {
            MPIU_Object_set_ref(sreq, 0);
            MPIDI_CH3_Request_destroy(sreq);
            sreq = NULL;
            MPIU_ERR_POP(mpi_errno);
        }

        if (send_req != NULL) {
            MPID_Request_release(send_req);
        }
    } else {
        MPIDI_DBG_PRINTF((55, FCNAME, "send queue not empty, enqueuing"));
        MPIDI_CH3I_SendQ_enqueue(vc, sreq);
    }

  fn_exit:
#ifdef CKPT
    MPIDI_CH3I_CR_unlock();
#endif
    DEBUG_PRINT("[send rts]successful complete\n");
    MPIDI_DBG_PRINTF((50, FCNAME, "exiting"));
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_ISTARTRNDVMSG);
    return mpi_errno;

fn_fail:
   goto fn_exit;
}


#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_iStartGetAccRndv
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_iStartGetAccRndv(MPIDI_VC_t * vc,
                            MPID_Request * sreq, MPID_Request *rreq, int control_cnt)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_ISTARTGETACCRNDV);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_ISTARTGETACCRNDV);
    int mpi_errno = MPI_SUCCESS;
    MPIDI_CH3_Pkt_get_accum_rndv_t *get_accum_rndv =
            (void *) sreq->dev.iov[0].MPID_IOV_BUF;
    MPID_Request *rts_sreq;
    MPID_IOV *iov;
    MPID_Datatype *result_dtp = NULL;
    MPI_Aint result_type_size;
    MPIDI_msg_sz_t data_sz;

#ifdef CKPT
    MPIDI_CH3I_CR_lock();
#endif

    /* we adjust iov because the rndv process assume the data starts from the first
     * vector of iov array */
    if ((iov = MPIU_Malloc(sizeof(MPID_IOV) * (control_cnt))) == NULL)
    {
        MPIU_CHKMEM_SETERR(mpi_errno, sizeof(MPID_IOV) * control_cnt, "MPID IOV");
    }
    MPIU_Memcpy((void *) iov, (void *) sreq->dev.iov,
           sizeof(MPID_IOV) * control_cnt);
    memmove((void *) sreq->dev.iov,
           (void *) &sreq->dev.iov[control_cnt],
           sizeof(MPID_IOV) * (sreq->dev.iov_count - control_cnt));

    sreq->dev.iov_count -= control_cnt;

    /*prepare send request*/
    MPIDI_CH3_Prepare_rndv(vc, sreq);
    MPIDI_CH3I_MRAIL_REVERT_RPUT(sreq);

    /*prepare receive request for get*/
    MPID_Datatype_get_ptr(rreq->dev.datatype, result_dtp);
    MPID_Datatype_get_size_macro(rreq->dev.datatype, result_type_size);
    data_sz = rreq->dev.user_count*result_type_size;
    if(result_dtp->is_contig) {
        rreq->dev.OnDataAvail = 0;

        rreq->dev.iov[0].MPID_IOV_BUF = (MPID_IOV_BUF_CAST) ((char*)rreq->dev.user_buf + result_dtp->true_lb);
        rreq->dev.iov[0].MPID_IOV_LEN = data_sz;
        rreq->dev.iov_count = 1;
    } else {
        rreq->dev.segment_ptr = MPID_Segment_alloc( );

        MPID_Segment_init(rreq->dev.user_buf, rreq->dev.user_count,
              rreq->dev.datatype, rreq->dev.segment_ptr, 0);
        rreq->dev.iov_count = MPID_IOV_LIMIT;
        rreq->dev.segment_first = 0;
        rreq->dev.segment_size = data_sz;

        /* One the initial load of a send iov req, set the OnFinal action (null
         *        for point-to-point) */
        rreq->dev.OnFinal = 0;
        mpi_errno = MPIDI_CH3U_Request_load_send_iov(rreq, &rreq->dev.iov[0],
                             &rreq->dev.iov_count);
    }
    MPIDI_CH3_Prepare_rndv(vc, rreq);
    MPIDI_CH3I_MRAIL_REVERT_RPUT(rreq);

#ifdef _ENABLE_UD_
    if(rdma_enable_hybrid && rreq->mrail.protocol == MV2_RNDV_PROTOCOL_UD_ZCOPY) {
        rreq->mrail.protocol = MV2_RNDV_PROTOCOL_R3;
        MPIDI_CH3I_MRAIL_FREE_RNDV_BUFFER(rreq);
    }
#endif

    /*we send the rndv information of the result buffer*/
    MPIDI_CH3I_MRAIL_SET_PKT_RNDV(get_accum_rndv, rreq);

    get_accum_rndv->sender_req_id = sreq->handle;

    if ((mpi_errno = MPIDI_CH3_iStartMsgv(vc, iov, control_cnt, &rts_sreq)) != MPI_SUCCESS)
    {
        MPIU_Object_set_ref(sreq, 0);
        MPIDI_CH3_Request_destroy(sreq);
        sreq = NULL;
        MPIU_ERR_POP(mpi_errno);
    }

    if (rts_sreq != NULL) {
        MPID_Request_release(rts_sreq);
    }
    MPIU_Free(iov);

fn_exit:
#if defined(CKPT)
    MPIDI_CH3I_CR_unlock();
#endif /* defined(CKPT) */
    DEBUG_PRINT("[send rts]successful complete\n");

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_ISTARTGETACCRNDV);
    return mpi_errno;

fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_iStartRmaRndv
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_iStartRmaRndv(MPIDI_VC_t * vc,
                            MPID_Request * sreq, int control_cnt)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_ISTARTRMARNDV);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_ISTARTRMARNDV);
    int mpi_errno = MPI_SUCCESS;
    MPIDI_CH3_Pkt_put_rndv_t *put_rndv =
        (void *) sreq->dev.iov[0].MPID_IOV_BUF;
    MPIDI_CH3_Pkt_accum_rndv_t *accum_rndv =
        (void *) sreq->dev.iov[0].MPID_IOV_BUF;
    MPID_Request *rts_sreq;
    MPID_IOV *iov;

#ifdef CKPT
    MPIDI_CH3I_CR_lock();
#endif

    if ((iov = MPIU_Malloc(sizeof(MPID_IOV) * (control_cnt))) == NULL)
    {
        MPIU_CHKMEM_SETERR(mpi_errno, sizeof(MPID_IOV) * control_cnt, "MPID IOV");
    }

    DEBUG_PRINT("sreq before adjust iov0.len %d\n",
                sreq->dev.iov[control_cnt].MPID_IOV_LEN);
    MPIU_Memcpy((void *) iov, (void *) sreq->dev.iov,
           sizeof(MPID_IOV) * control_cnt);

    /* we adjust iov because the rndv process assume the data starts from the first
     * vector of iov array */
    /* We can't use MPIU_Memcpy due to the overlapping check when using the debug flags.*/
    memmove((void *) sreq->dev.iov,
           (void *) &sreq->dev.iov[control_cnt],
           sizeof(MPID_IOV) * (sreq->dev.iov_count - control_cnt));

    sreq->dev.iov_count -= control_cnt;

    /* MT - need some signalling to lock down our right to use the
       channel, thus insuring that the progress engine does also try to
       write */
    MPIDI_CH3_Prepare_rndv(vc, sreq);
    MPIDI_CH3I_MRAIL_REVERT_RPUT(sreq);
    if (MPIDI_CH3_PKT_PUT_RNDV == put_rndv->type) {
        MPIDI_CH3I_MRAIL_SET_PKT_RNDV(put_rndv, sreq);
    } else {
        MPIDI_CH3I_MRAIL_SET_PKT_RNDV(accum_rndv, sreq);
    }

    if (MPIDI_CH3_PKT_PUT_RNDV == put_rndv->type) {
        put_rndv->sender_req_id = sreq->handle;
    } else {
        accum_rndv->sender_req_id = sreq->handle;
    }

    if ((mpi_errno = MPIDI_CH3_iStartMsgv(vc, iov, control_cnt, &rts_sreq)) != MPI_SUCCESS)
    {
        MPIU_Object_set_ref(sreq, 0);
        MPIDI_CH3_Request_destroy(sreq);
        sreq = NULL;
        MPIU_ERR_POP(mpi_errno);
    }

    if (rts_sreq != NULL) {
        MPID_Request_release(rts_sreq);
    }
    MPIU_Free(iov);

fn_exit:
#if defined(CKPT)
    MPIDI_CH3I_CR_unlock();
#endif /* defined(CKPT) */
    DEBUG_PRINT("[send rts]successful complete\n");

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_ISTARTRMARNDV);
    return mpi_errno;

fn_fail:
    goto fn_exit;
}


#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_iStartGetRndv
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_iStartGetRndv(MPIDI_VC_t * vc,
                            MPIDI_CH3_Pkt_get_rndv_t * get_rndv,
                            MPID_Request * sreq,
                            MPID_IOV * control_iov, int num_control)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_ISTARTGETRNDV);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_ISTARTGETRNDV);
    int mpi_errno = MPI_SUCCESS;
    MPID_Request *send_req;

#ifdef CKPT
    MPIDI_CH3I_CR_lock();
#endif
        
    MPID_IOV* iov = MPIU_Malloc(sizeof(MPID_IOV) * (num_control + 1));

    if (iov == NULL)
    {
        MPIU_CHKMEM_SETERR(mpi_errno, sizeof(MPID_IOV) * (num_control + 1), "MPID IOV");
    }

    int n_iov = num_control + 1;
    iov[0].MPID_IOV_BUF = (void *) get_rndv;
    iov[0].MPID_IOV_LEN = sizeof(MPIDI_CH3_Pkt_get_rndv_t);
    MPIU_Memcpy((void *) &iov[1], (void *) control_iov,
           sizeof(MPID_IOV) * num_control);

    MPIDI_CH3_Prepare_rndv(vc, sreq);
    MPIDI_CH3I_MRAIL_REVERT_RPUT(sreq);

#ifdef _ENABLE_UD_
    if(rdma_enable_hybrid && sreq->mrail.protocol == MV2_RNDV_PROTOCOL_UD_ZCOPY) {
        sreq->mrail.protocol = MV2_RNDV_PROTOCOL_R3;
        MPIDI_CH3I_MRAIL_FREE_RNDV_BUFFER(sreq);
    }
#endif

    MPIDI_CH3I_MRAIL_SET_PKT_RNDV(get_rndv, sreq); 

    mpi_errno = MPIDI_CH3_iStartMsgv(vc, iov, n_iov, &send_req);
    if (NULL != send_req) {
        MPID_Request_release(send_req);
    }
    MPIU_Free(iov);

#ifdef CKPT
    MPIDI_CH3I_CR_unlock();
#endif

fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_ISTARTGETRNDV);
    return mpi_errno;
#ifndef CHANNEL_MRAIL
fn_fail:
#endif
    goto fn_exit;
}
