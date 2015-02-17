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

#include "psmpriv.h"
#include "psm_vbuf.h"

#define MAX_PREPOST 512
#define RCV_PREPOST 128

#define MAKE_1SIDED_SELECTOR(_rt, _rts) do {               \
    _rt = 0;                                               \
    MAKE_PSM_SELECTOR(_rt, MPID_CONTEXT_PSMCTRL, 0, 0); \
    _rts = (MQ_TAGSEL_ANY_SOURCE & MQ_TAGSEL_ANY_TAG);     \
} while (0)

static void psm_1sided_recv(MPID_Request *req, void *ptr);
static void *psm_gen_packbuf(MPID_Request *rreq, MPID_Request *dtreq);
static void psm_init_tag();
static MPID_Request *psm_1sc_putacc_rndvrecv(MPID_Request *, int, MPID_Request **, 
                                      void *, int, int, int, MPIDI_VC_t *);
static MPIDI_CH3_PktHandler_Fcn *psm_pkthndl[MPIDI_CH3_PKT_END_CH3+1];

int psm_get_rndvtag();

/* notes:
   psm does not support one-sided natively. one-sided msgs (put/get/acc)
   are sent on a control-context on which vbufs are pre-posted. If 
   msgs are small, the data is sent inline with the control packet.
   If data is large, the control-packet sends the (tag,src_rank) and the
   target posts a RNVD receive on that tag,rank on a special RNDV context.
   psm_1sided_input processes all incoming one-sided packets.
    
   the rank used in the one-sided call (MPI_Put,MPI_Get) is w.r.t to the
   communicator the window operates on. a rank-mapping array created at window
   creation time remaps the rank w.r.t the communicator to a rank w.r.t
   COMM_WORLD.    
  */ 
    
void psm_init_1sided()
{
    /* psm uses CH3 packet handlers for one-sided */
    MPIDI_CH3_PktHandler_Init(psm_pkthndl, MPIDI_CH3_PKT_END_CH3+1);
    psm_init_tag();
}

void psm_prepost_1sc()
{
    int i;
    MPID_Request *req;
    vbuf *ptr;

    if(psm_tot_pposted_recvs >= MAX_PREPOST)
        return;

    for(i = 0; i < RCV_PREPOST; i++) {
        req = psm_create_req();
        req->kind = MPID_REQUEST_RECV;
        req->psm_flags |= PSM_1SIDED_PREPOST;
        ptr = psm_get_vbuf();
        req->vbufptr = ptr;
        ptr->req = (void*) req;
        psm_1sided_recv(req, ptr->buffer); 
    }
    DBG("pre-posted recv buffers\n");
}

static void psm_1sided_recv(MPID_Request *req, void *ptr)
{
    uint64_t rtag, rtagsel;
    MAKE_1SIDED_SELECTOR(rtag, rtagsel);

    assert(req->psm_flags & PSM_1SIDED_PREPOST);
    req->psm_flags = 0;
    req->psm_flags |= PSM_1SIDED_PREPOST;
    ++psm_tot_pposted_recvs;
    _psm_enter_;
    psm_mq_irecv(psmdev_cw.mq, rtag, rtagsel, MQ_FLAGS_NONE, ptr,
                 PSM_VBUFSZ, req, &(req->mqreq));
    _psm_exit_;
}

static psm_error_t psm_iput(int dest, void *buf, MPIDI_msg_sz_t buflen, MPID_Request *req, int src)
{
    uint64_t stag = 0;
    psm_error_t psmerr;

    MAKE_PSM_SELECTOR(stag, MPID_CONTEXT_PSMCTRL, 0, src);
    _psm_enter_;
    if ((unlikely(buflen > ipath_max_transfer_size))) {
        psmerr = psm_large_msg_isend_pkt(&req, dest, buf, buflen,
                    stag, MQ_FLAGS_NONE);
    } else {
        psmerr = psm_mq_isend(psmdev_cw.mq, psmdev_cw.epaddrs[dest],
                    MQ_FLAGS_NONE, stag, buf, buflen, req, &(req->mqreq));
    }
    _psm_exit_;
    return psmerr;
}

static psm_error_t psm_iget_rndvsend(MPID_Request *req, int dest, void *buf, MPIDI_msg_sz_t buflen,
                       int tag, int src)
{
    uint64_t stag = 0;
    psm_error_t psmerr;

    MAKE_PSM_SELECTOR(stag, MPID_CONTEXT_RNDVPSM, tag, src);
    _psm_enter_;
    if ((unlikely(buflen > ipath_max_transfer_size))) {
        psmerr = psm_large_msg_isend_pkt(&req, dest, buf, buflen,
                    stag, MQ_FLAGS_NONE);
    } else {
        psmerr = psm_mq_isend(psmdev_cw.mq, psmdev_cw.epaddrs[dest],
                    MQ_FLAGS_NONE, stag, buf, buflen, req, &(req->mqreq));
    }
    _psm_exit_;
    return psmerr;
}

void psm_iput_rndv(int dest, void *buf, MPIDI_msg_sz_t buflen, int tag, int src, MPID_Request **rptr)
{
    uint64_t stag = 0;
    psm_error_t psmerr ATTRIBUTE((unused));
    MPID_Request *rndvreq = NULL;

    rndvreq = psm_create_req();
    rndvreq->kind = MPID_REQUEST_SEND;
    rndvreq->psm_flags |= PSM_RNDVSEND_REQ;
    *rptr = rndvreq;
    DBG("rndv send len %d tag %d dest %d I-am %d\n", buflen, tag, dest, src);
  
    MAKE_PSM_SELECTOR(stag, MPID_CONTEXT_RNDVPSM, tag, src);
    _psm_enter_;
    if ((unlikely(buflen > ipath_max_transfer_size))) {
        psmerr = psm_large_msg_isend_pkt(rptr, dest, buf, buflen,
                    stag, MQ_FLAGS_NONE);
    } else {
        psmerr = psm_mq_isend(psmdev_cw.mq, psmdev_cw.epaddrs[dest],
                MQ_FLAGS_NONE, stag, buf, buflen, rndvreq, &(rndvreq->mqreq));
    }
    _psm_exit_;
}

/* used for fop, cas, fop response, cas resposne */
int psm_1sided_atomicpkt(MPIDI_CH3_Pkt_t *pkt, MPID_IOV *iov, int iov_n, int rank,
                             int srank, MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    int mpi_errno = MPI_SUCCESS;
    int i;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req;

    req = psm_create_req();
    req->kind = MPID_REQUEST_SEND;
    req->psm_flags |= PSM_1SIDED_PUTREQ;
    *rptr = req;
    vptr = psm_get_vbuf();
    req->vbufptr = vptr;
    vptr->req = (void*) req;

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }

    if(buflen <= PSM_VBUFSZ) {
        off = vptr->buffer;
       
        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
        }
        psm_iput(rank, vptr->buffer, buflen, req, srank);
        ++psm_tot_eager_puts;
    } 
    return mpi_errno;
}

/* copy iov into a single vbuf, post send to target rank,
   using 1-sided context id */

int psm_1sided_putpkt(MPIDI_CH3_Pkt_put_t *pkt, MPID_IOV *iov, int iov_n,
                       MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    int mpi_errno = MPI_SUCCESS;
    int rank, i;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req;

    req = psm_create_req();
    req->kind = MPID_REQUEST_SEND;
    req->psm_flags |= PSM_1SIDED_PUTREQ;
    *rptr = req;
    vptr = psm_get_vbuf();
    req->vbufptr = vptr;
    vptr->req = (void*) req;
    rank = pkt->mapped_trank;

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }

    /* eager PUT */
    if(buflen <= PSM_VBUFSZ) {
        off = vptr->buffer;
        pkt->rndv_mode = 0;
       
        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
        }
        psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);
        ++psm_tot_eager_puts;
    } else { /* rndv PUT */
        off = vptr->buffer;
        pkt->rndv_mode = 1;
        pkt->rndv_tag = psm_get_rndvtag();
        pkt->rndv_len = iov[iov_n-1].MPID_IOV_LEN;
        buflen = 0;
        
        /* last iov is the packet */
        for(i = 0; i < (iov_n-1); i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
            buflen = buflen + len;
        }
        psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);
        iovp = (void *)iov[iov_n-1].MPID_IOV_BUF;
        len = iov[iov_n-1].MPID_IOV_LEN;
        psm_iput_rndv(rank, iovp, len, pkt->rndv_tag, pkt->mapped_srank, rptr);
        ++psm_tot_rndv_puts;
    }
    return mpi_errno;
}

/* copy iov into a single vbuf, post send to target rank,
   using 1-sided context id */

int psm_1sided_accumpkt(MPIDI_CH3_Pkt_accum_t *pkt, MPID_IOV *iov, int iov_n,
                       MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    int rank, i;
    int mpi_errno = MPI_SUCCESS;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req;

    req = psm_create_req();
    req->kind = MPID_REQUEST_SEND;
    req->psm_flags |= PSM_1SIDED_PUTREQ;
    *rptr = req;
    vptr = psm_get_vbuf();
    req->vbufptr = vptr;
    vptr->req = (void*) req;
    rank = pkt->mapped_trank;

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }

    /* eager PUT */
    if(buflen <= PSM_VBUFSZ) {
        off = vptr->buffer;
        pkt->rndv_mode = 0;
       
        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
        }
        psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);
    } else { /* rndv PUT */
        off = vptr->buffer;
        pkt->rndv_mode = 1;
        pkt->rndv_tag = psm_get_rndvtag();
        pkt->rndv_len = iov[iov_n-1].MPID_IOV_LEN;
        buflen = 0;
        
        /* last iov is the packet */
        for(i = 0; i < (iov_n-1); i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
            buflen = buflen + len;
        }
       psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);
        iovp = (void *)iov[iov_n-1].MPID_IOV_BUF;
        len = iov[iov_n-1].MPID_IOV_LEN;
        psm_iput_rndv(rank, iovp, len, pkt->rndv_tag, pkt->mapped_srank, rptr);
    }
    ++psm_tot_accs;
    return mpi_errno;
}

int psm_1sided_getaccumpkt(MPIDI_CH3_Pkt_accum_t *pkt, MPID_IOV *iov, int iov_n,
                       MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    int rank, i;
    int mpi_errno = MPI_SUCCESS;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req;
    uint64_t rtag, rtagsel;
    psm_error_t psmerr;

    req = psm_create_req();
    req->kind = MPID_REQUEST_SEND;
    req->psm_flags |= PSM_1SIDED_PUTREQ;
    *rptr = req;
    vptr = psm_get_vbuf();
    req->vbufptr = vptr;
    vptr->req = (void*) req;
    rank = pkt->mapped_trank;

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }

    /* eager PUT */
    if(buflen <= PSM_VBUFSZ) {
        off = vptr->buffer;
        pkt->rndv_mode = 0;
       
        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = (void *)((uintptr_t)off + len);
        }
        psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);
    } else { /* rndv GET ACCUM */
        off = vptr->buffer;
        pkt->rndv_mode = 1;
        pkt->rndv_tag = psm_get_rndvtag();
        pkt->rndv_len = iov[iov_n-1].MPID_IOV_LEN;

        /*tag for resp packet*/
        pkt->resp_rndv_tag = psm_get_rndvtag();

        /* last iov is the packet */
        buflen = 0;
        for(i = 0; i < (iov_n-1); i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
            buflen = buflen + len;
        }
        psm_iput(rank, vptr->buffer, buflen, req, pkt->mapped_srank);

        iovp = (void *)iov[iov_n-1].MPID_IOV_BUF;
        len = iov[iov_n-1].MPID_IOV_LEN;
        psm_iput_rndv(rank, iovp, len, pkt->rndv_tag, pkt->mapped_srank, rptr);

        /*post rndv recieve for response*/
        MPID_Request *resp_req = NULL, *orig_resp_req = NULL;

        MPID_Request_get_ptr(pkt->request_handle, orig_resp_req);
        if(!MPIR_DATATYPE_IS_PREDEFINED(orig_resp_req->dev.datatype)) {
            if(!orig_resp_req->dev.datatype_ptr->is_contig) {
                 MPI_Aint result_type_size;
                 MPID_Datatype_get_size_macro(orig_resp_req->dev.datatype, result_type_size);

                 orig_resp_req->dev.real_user_buf = orig_resp_req->dev.user_buf;
                 orig_resp_req->dev.user_buf = MPIU_Malloc(orig_resp_req->dev.user_count*result_type_size);
                 orig_resp_req->psm_flags |= PSM_RNDVRECV_GET_PACKED;
            }
        }

        resp_req = psm_create_req();
        resp_req->kind = MPID_REQUEST_RECV;
        resp_req->psm_flags |= PSM_RNDVRECV_GET_REQ;
        if(orig_resp_req->psm_flags & PSM_RNDVRECV_GET_PACKED) {
            resp_req->psm_flags |= PSM_RNDVRECV_GET_PACKED;
            orig_resp_req->psm_flags &= ~PSM_RNDVRECV_GET_PACKED;
        }
        resp_req->savedreq = orig_resp_req;

        rtag = 0;
        rtagsel = MQ_TAGSEL_ALL;
        MAKE_PSM_SELECTOR(rtag, MPID_CONTEXT_RNDVPSM, pkt->resp_rndv_tag,
                  pkt->mapped_trank);

        _psm_enter_;
        if ((unlikely(pkt->rndv_len > ipath_max_transfer_size))) {
            psmerr = psm_post_large_msg_irecv(orig_resp_req->dev.user_buf, pkt->rndv_len,
                        &resp_req, rtag, rtagsel);
        } else {
            psmerr = psm_mq_irecv(psmdev_cw.mq, rtag, rtagsel, MQ_FLAGS_NONE,
                        orig_resp_req->dev.user_buf, pkt->rndv_len, resp_req,
                        &(resp_req->mqreq));
        }
        _psm_exit_;
        if(unlikely(psmerr != PSM_OK)) {
            printf("ERROR: rndv recv failed\n");
        }
    }

    ++psm_tot_accs;
    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME psm_1sided_getaccumresppkt
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int psm_1sided_getaccumresppkt(MPIDI_CH3_Pkt_get_accum_resp_t *pkt, MPID_IOV *iov, int iov_n,
                       MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req = (*rptr);
    psm_error_t psmerr;
    int mpi_errno = MPI_SUCCESS, i;

    req->psm_flags |= PSM_GETACCUMRESP_REQ;

    if(!pkt->rndv_mode) {
        req->psm_flags |= PSM_CONTROL_PKTREQ;
        vptr = psm_get_vbuf();
        req->vbufptr = vptr;
        vptr->req = (void*) req;
        off = vptr->buffer;

        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = (void *) ((uintptr_t)off + len);
            buflen = buflen + len;
        }

        psmerr = psm_iput(pkt->mapped_trank, vptr->buffer, buflen, req,
                pkt->mapped_srank);
    } else {
        iovp = (void *)iov[iov_n-1].MPID_IOV_BUF;
        len = iov[iov_n-1].MPID_IOV_LEN;
        assert(len == pkt->rndv_len);

        psmerr = psm_iget_rndvsend(req, pkt->mapped_trank, iovp, len,
                                   pkt->rndv_tag, pkt->mapped_srank);
    }

    if(unlikely(psmerr != PSM_OK)) {
        MPIU_ERR_SET(mpi_errno, MPI_ERR_INTERN, "**fail");
    }

    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME psm_1sided_getpkt
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int psm_1sided_getpkt(MPIDI_CH3_Pkt_get_t *pkt, MPID_IOV *iov, int iov_n,
        MPID_Request **rptr) 
{
    MPIDI_msg_sz_t buflen = 0, len;
    int mpi_errno = MPI_SUCCESS, i;
    void *off, *iovp;
    psm_error_t psmerr;
    vbuf *vptr;
    MPID_Request *req = (*rptr);

    req->psm_flags |= (PSM_GETPKT_REQ | PSM_CONTROL_PKTREQ);
    MPIU_Object_add_ref(req);    

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }
    assert(buflen <= PSM_VBUFSZ);

    vptr = psm_get_vbuf();
    req->vbufptr = vptr;
    vptr->req = (void*) req;
    off = vptr->buffer;
    
    for(i = 0; i < iov_n; i++) {
        iovp = (void *)iov[i].MPID_IOV_BUF;
        len = iov[i].MPID_IOV_LEN;
        memcpy(off, iovp, len);
        off = off + len;
    }
    psmerr = psm_iput(pkt->mapped_trank, vptr->buffer, buflen, req,
            pkt->mapped_srank);
    if(unlikely(psmerr != PSM_OK)) {
        MPIU_ERR_SET(mpi_errno, MPI_ERR_INTERN, "**fail");
    }

    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME psm_1sided_getresppkt
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int psm_1sided_getresppkt(MPIDI_CH3_Pkt_get_resp_t *pkt, MPID_IOV *iov, int iov_n,
                       MPID_Request **rptr)
{
    vbuf *vptr;
    void *iovp, *off;
    MPIDI_msg_sz_t buflen = 0, len;
    MPID_Request *req = (*rptr);
    psm_error_t psmerr;
    int mpi_errno = MPI_SUCCESS, i;

    req->psm_flags |= PSM_GETRESP_REQ;

    for(i = 0; i < iov_n; i++) {
        buflen = buflen + iov[i].MPID_IOV_LEN;
    }

    DBG("get-resp packet length %d\n", buflen);
    /* eager get response */
    //if(buflen <= PSM_VBUFSZ) {
    if(!pkt->rndv_mode) {
        req->psm_flags |= PSM_CONTROL_PKTREQ;
        vptr = psm_get_vbuf();
        req->vbufptr = vptr;
        vptr->req = (void*) req;
        off = vptr->buffer;
       
        for(i = 0; i < iov_n; i++) {
            iovp = (void *)iov[i].MPID_IOV_BUF;
            len = iov[i].MPID_IOV_LEN;
            memcpy(off, iovp, len);
            off = off + len;
        }

        psmerr = psm_iput(pkt->mapped_trank, vptr->buffer, buflen, req,
                pkt->mapped_srank);
    } else {
        iovp = (void *)iov[iov_n-1].MPID_IOV_BUF;
        len = iov[iov_n-1].MPID_IOV_LEN;
        assert(len == pkt->rndv_len);
    
        psmerr = psm_iget_rndvsend(req, pkt->mapped_trank, iovp, len,
                                   pkt->rndv_tag, pkt->mapped_srank);
    }

    if(unlikely(psmerr != PSM_OK)) {
        MPIU_ERR_SET(mpi_errno, MPI_ERR_INTERN, "**fail");
    }

    return mpi_errno;
}

/*  incoming one-sided packet processing.
    Put:
        if(RNDV_PUT)
            create a new recv_req with the tag peer sent,
            post request on the RNDV context
        else
            call put-handler
            re-post the vbuf
        fi

    Accumulate:
        if(RNDV_ACCUM)
            we need to create a new request to receive the data.
            create a tmpbuf for this size and post a recv/
            Once, receive completes call packet handler, but with
            data-copied flag to do only the accumulate 
        else
            call accum-handler
            re-post the vbuf
        fi    

    Get:
        if(small_get)
            call the get_handler. It will send out data using iStartMsgv
            on 1-sided context using a vbuf.
        else


        fi
    
    Lock:
        Call lock-handler function
    Lock_granted:
        set win_ptr->lock_granted to 1. Someone is waiting for this.
            

        
*/

#define GET_VC(_vc, _whndl, _vcindex)    do {               \
    MPID_Win *win_ptr;                                      \
    MPID_Win_get_ptr(_whndl, win_ptr);                      \
    MPIDI_Comm_get_vc(win_ptr->comm_ptr, _vcindex, &_vc);   \
} while(0)

#define __check(_str, _tp)                     do { \
    if(_tp == MPIDI_CH3_PKT_##_str) goto do_##_str; \
} while(0)  

#define _SECTION(TP)                                        \
    DBG("Section handles"#TP"\n");                          \
    do_##TP:                                                  

int psm_1sided_input(MPID_Request *req, int inlen)
{
    MPIDI_CH3_Pkt_t *pkt;
    MPID_Request *temp_req;
    vbuf  *vbptr;
    void *ptr;
    MPIDI_msg_sz_t msg = inlen;
    MPIDI_VC_t *vc;

    vbptr = req->vbufptr;
    ptr = vbptr->buffer;
    pkt = (MPIDI_CH3_Pkt_t *) ptr;

    __check(PUT,            pkt->type);
    __check(GET,            pkt->type);
    __check(GET_RESP,       pkt->type);
    __check(ACCUMULATE,     pkt->type);
    __check(GET_ACCUM,      pkt->type);
    __check(GET_ACCUM_RESP, pkt->type);
    __check(ACCUM_IMMED,    pkt->type);
    __check(FOP,            pkt->type);
    __check(FOP_RESP,       pkt->type);
    __check(CAS,            pkt->type);
    __check(CAS_RESP,       pkt->type);
    __check(LOCK,           pkt->type);
    __check(UNLOCK,         pkt->type);
    __check(FLUSH,          pkt->type);
    __check(LOCK_GRANTED,   pkt->type);
    __check(PT_RMA_DONE,    pkt->type);
    goto errpkt;


    /* handle put 
       if data is small, it is received in vbuf. call the PUT packet handler to
       complete the operation. If the source-datatype was non-contiguous we
       would have packed it. In this case, the packet handler will unpack the
       data

       if data is large, the packet handler is called to parse datatype info.
       if target-datatype is non-contiguous create a packing buffer of required
       size and post the RNDV receive on this buffer. Once RNDV receive
       completes, unpack the data into correct address */
    {
        _SECTION(PUT);
        MPIDI_CH3_Pkt_put_t *putpkt = (MPIDI_CH3_Pkt_put_t *) pkt;
        if(!putpkt->rndv_mode) { /* eager put */
            GET_VC(vc, putpkt->target_win_handle, putpkt->source_rank);
            vc->ch.recv_active = req;
            DBG("put packet from %d\n", vc->pg_rank);
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
            goto end;
        } else {                /* large put */
            MPID_Request *nreq = NULL;
            MPID_Win *win_ptr = NULL;

            MPID_Win_get_ptr(putpkt->target_win_handle, win_ptr); 
            win_ptr->outstanding_rma++;

            GET_VC(vc, putpkt->target_win_handle, putpkt->source_rank);
            vc->ch.recv_active = req;
            DBG("large put packet from %d\n", vc->pg_rank);
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
            psm_1sc_putacc_rndvrecv(req, inlen, &nreq, putpkt->addr,
                            putpkt->rndv_tag, putpkt->mapped_srank,
                            putpkt->rndv_len, vc);
            nreq->psm_flags |= PSM_RNDVRECV_PUT_REQ;
            DBG("rndv_put request. posted recv %x\n", nreq);
            goto end_2;
        }
    }

    {
        _SECTION(GET);
        MPIDI_CH3_Pkt_get_t *getpkt = (MPIDI_CH3_Pkt_get_t *) pkt;
        GET_VC(vc, getpkt->target_win_handle, getpkt->source_rank);
        vc->ch.recv_active = req;
        DBG("get packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;
    }

    {
        _SECTION(GET_RESP);
        MPIDI_CH3_Pkt_get_resp_t *resppkt = (MPIDI_CH3_Pkt_get_resp_t *) pkt;
        GET_VC(vc, resppkt->target_win_handle, resppkt->source_rank);
        vc->ch.recv_active = req;
        req->dev.target_win_handle = resppkt->source_win_handle;
        req->dev.source_win_handle = resppkt->target_win_handle;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;
    }

    {
        _SECTION(ACCUM_IMMED); /*immediate accumulate*/
        MPIDI_CH3_Pkt_accum_immed_t *accimmpkt = (MPIDI_CH3_Pkt_accum_immed_t *) pkt;
        GET_VC(vc, accimmpkt->target_win_handle, accimmpkt->source_rank);
        vc->ch.recv_active = req;
        DBG("accum immed packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;           
    }

    {
        _SECTION(ACCUMULATE);
        MPIDI_CH3_Pkt_accum_t *acpkt = (MPIDI_CH3_Pkt_accum_t *) pkt;
        if(!acpkt->rndv_mode) { /* eager accumulate */
            GET_VC(vc, acpkt->target_win_handle, acpkt->source_rank);
            vc->ch.recv_active = req;
            DBG("accum packet from %d\n", vc->pg_rank);
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
            goto end;           /* large accumulate */
        } else {
            MPID_Request *nreq = NULL;
            MPID_Win *win_ptr = NULL; 

            MPID_Win_get_ptr(acpkt->target_win_handle, win_ptr); 
            win_ptr->outstanding_rma++; 

            GET_VC(vc, acpkt->target_win_handle, acpkt->source_rank);
            req->psm_flags |= PSM_RNDV_ACCUM_REQ;
            vc->ch.recv_active = req;
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
            nreq = vc->ch.recv_active;

            /* now we have a mpid_request with the user_buf set to tmpbuf */
            nreq = psm_1sc_putacc_rndvrecv(req, inlen, &nreq, 
                                    nreq->dev.user_buf, acpkt->rndv_tag,
                                    acpkt->mapped_srank, acpkt->rndv_len, vc);
            nreq->psm_flags |= PSM_RNDVRECV_ACCUM_REQ;
            DBG("rndv_accum request. posted recv %x\n", nreq);
            goto end_2;
        }
    }

    {
        _SECTION(GET_ACCUM);
        MPIDI_CH3_Pkt_accum_t *acpkt = (MPIDI_CH3_Pkt_accum_t *) pkt;

        if(!acpkt->rndv_mode) {
            GET_VC(vc, acpkt->target_win_handle, acpkt->source_rank);
            vc->ch.recv_active = req;
            DBG("get accum packet from %d\n", vc->pg_rank);
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
            goto end;           /* large accumulate */
        } else {
            MPID_Request *nreq = NULL;
            MPID_Win *win_ptr = NULL;

            MPID_Win_get_ptr(acpkt->target_win_handle, win_ptr);
            win_ptr->outstanding_rma++;
        
            GET_VC(vc, acpkt->target_win_handle, acpkt->source_rank);
            req->psm_flags |= PSM_RNDV_ACCUM_REQ;
            vc->ch.recv_active = req;
            psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));

            nreq = vc->ch.recv_active;
            nreq = psm_1sc_putacc_rndvrecv(req, inlen, &nreq,
                                    nreq->dev.user_buf, acpkt->rndv_tag,
                                    acpkt->mapped_srank, acpkt->rndv_len, vc);
            nreq->psm_flags |= PSM_RNDVRECV_ACCUM_REQ;

            nreq->resp_rndv_tag = acpkt->resp_rndv_tag;

            DBG("rndv_accum request. posted recv %x\n", nreq);
            goto end_2; 
        }
    }

    {
        _SECTION(GET_ACCUM_RESP);
        MPIDI_CH3_Pkt_get_accum_resp_t *acpkt = (MPIDI_CH3_Pkt_get_accum_resp_t *) pkt;
        MPIU_Assert(acpkt->rndv_mode != 1);

        GET_VC(vc, acpkt->target_win_handle, acpkt->source_rank);
        vc->ch.recv_active = req;
        DBG("get accum packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));

        goto end;          
    }

    {
        _SECTION(FOP);
        MPIDI_CH3_Pkt_fop_t *foppkt = (MPIDI_CH3_Pkt_fop_t *) pkt;
        GET_VC(vc, foppkt->target_win_handle, foppkt->source_rank);
        vc->ch.recv_active = req;
        DBG("fop packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;           
    }

    {
        _SECTION(FOP_RESP);
        temp_req = req;
        DBG("fop resp packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](NULL, pkt, &msg, &temp_req);
        goto end;           
    }

    {
        _SECTION(CAS);
        MPIDI_CH3_Pkt_cas_t *caspkt = (MPIDI_CH3_Pkt_cas_t *) pkt;
        GET_VC(vc, caspkt->target_win_handle, caspkt->source_rank);
        vc->ch.recv_active = req;
        DBG("cas packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;           
    }

    {
        _SECTION(CAS_RESP);
        temp_req = req;
        DBG("cas resp packet from %d\n", vc->pg_rank);
        psm_pkthndl[pkt->type](NULL, pkt, &msg, &temp_req);
        goto end;           
    }

    /* handle lock */
    {
        _SECTION(LOCK);
        MPIDI_CH3_Pkt_lock_t *lockpkt = (MPIDI_CH3_Pkt_lock_t *) pkt;
        GET_VC(vc, lockpkt->target_win_handle, lockpkt->source_rank);
        vc->ch.recv_active = req;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        DBG("lock request from [%d]\n", vc->pg_rank);
        goto end;
    }

    /* handle unlock */
    {
        _SECTION(UNLOCK);
        MPIDI_CH3_Pkt_unlock_t *unlockpkt = (MPIDI_CH3_Pkt_unlock_t *) pkt;
        GET_VC(vc, unlockpkt->target_win_handle, unlockpkt->source_rank);
        vc->ch.recv_active = req;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        DBG("ulock request from [%d]\n", vc->pg_rank);
        goto end;
    }


    /* handle flush */
    {
        _SECTION(FLUSH);
        MPIDI_CH3_Pkt_flush_t *flushpkt = (MPIDI_CH3_Pkt_flush_t *) pkt;
        if (flushpkt->target_win_handle != MPI_WIN_NULL) { 
            GET_VC(vc, flushpkt->target_win_handle, flushpkt->target_rank);
        }
        temp_req = req;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &temp_req);
        DBG("flush request from [%d]\n", vc->pg_rank);
        goto end;
    }


    /* handle lock granted */
    {
        _SECTION(LOCK_GRANTED);
        MPIDI_CH3_Pkt_lock_granted_t *grpkt = (MPIDI_CH3_Pkt_lock_granted_t *) pkt;
        MPID_Win *win_ptr;
        MPID_Win_get_ptr(grpkt->source_win_handle, win_ptr);
        MPIDI_Comm_get_vc(win_ptr->comm_ptr, grpkt->target_rank, &vc);
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        DBG("granted lock\n");
        goto end;
    }

    /* handle rma done */
    {
        _SECTION(PT_RMA_DONE);
        MPIDI_CH3_Pkt_pt_rma_done_t *dnpkt = (MPIDI_CH3_Pkt_pt_rma_done_t *) pkt;
        GET_VC(vc, dnpkt->target_win_handle, dnpkt->source_rank);
        vc->ch.recv_active = req;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
        goto end;
    }

errpkt:    
    fprintf(stderr, "Unknown packet type %d\n", pkt->type);
    fprintf(stderr, "Request flags are %x\n", req->psm_flags);
    fprintf(stderr, "Length of message was %d\n", inlen);
    fprintf(stderr, "I should not be here. Poof!\n");
    fflush(stderr);

end:    
    /* Reset req before re-posting */
    vbptr->req = req;

    /* re-post the vbuf */
    psm_1sided_recv(req, ptr);

end_2:    
    return MPI_SUCCESS;
}
#undef _SECTION
#undef __check
#undef GET_VC

/* a large request has completed */
int psm_complete_rndvrecv(MPID_Request *req, int inlen)
{
    /* the put pkt request was stored in tmpbuf */
    MPID_Request *putreq;
    MPIDI_CH3_Pkt_t *pkt;
    MPIDI_VC_t *vc;
    MPID_Win *win_ptr;
    vbuf *vbptr;
    MPIDI_msg_sz_t msg = req->pktlen;
    int complete = TRUE;

    putreq = req->savedreq;
    vbptr = putreq->vbufptr;
    pkt = (MPIDI_CH3_Pkt_t *) vbptr->buffer;
    if(req->psm_flags & PSM_RNDVRECV_PUT_REQ) {
        MPIDI_CH3_Pkt_put_t *putpkt;
        putpkt = (MPIDI_CH3_Pkt_put_t *) pkt;
        MPID_Win_get_ptr(putpkt->target_win_handle, win_ptr);
        MPIDI_Comm_get_vc(win_ptr->comm_ptr, putpkt->source_rank, &vc);
        vc->ch.recv_active = putreq;
        if(req->psm_flags & PSM_RNDVRECV_NC_REQ) {
            /* we've received it to a pack-buf. Unpack it now */
            MPID_Request *treq = req->pending_req;
            psm_do_unpack(treq->dev.user_count, treq->dev.datatype, NULL,
                    req->pkbuf, req->pksz, treq->dev.user_buf, inlen);
            /* treq had dataloop et al free it now */
            MPIDI_CH3U_Request_complete(treq);
        }
        putreq->psm_flags |= PSM_RNDVPUT_COMPLETED;
        win_ptr->outstanding_rma--;
        psm_pkthndl[pkt->type](vc, pkt, &msg, &(vc->ch.recv_active));
    } else if(req->psm_flags & PSM_RNDVRECV_ACCUM_REQ) {
        MPIDI_CH3_Pkt_accum_t *acpkt;
        acpkt = (MPIDI_CH3_Pkt_accum_t *) pkt;
        MPID_Win_get_ptr(acpkt->target_win_handle, win_ptr);
        MPIDI_Comm_get_vc(win_ptr->comm_ptr, acpkt->source_rank, &vc);
        vc->ch.recv_active = req;
        if(req->psm_flags & PSM_RNDVRECV_NC_REQ) {
            /* we've received it to a pack-buf. Unpack it now */
            MPID_Request *treq = req->pending_req;
            psm_do_unpack(treq->dev.user_count, treq->dev.datatype, NULL,
                    req->pkbuf, req->pksz, treq->dev.user_buf, inlen);
            MPIDI_CH3U_Request_complete(treq);
        }
        req->psm_flags |= PSM_RNDVPUT_COMPLETED;
        win_ptr->outstanding_rma--;
        MPIDI_CH3_ReqHandler_PutAccumRespComplete(vc, req, &complete);
    }

    /* free the rndv request */
    req->psm_flags &= ~PSM_RNDVPUT_COMPLETED;
    req->savedreq = NULL;
    MPIU_Object_set_ref(req, 0);
    MPIDI_CH3_Request_destroy(req);

    /* Reset req before re-posting */
    vbptr->req = putreq;

    /* repost the original put-vbuf*/
    psm_1sided_recv(putreq, vbptr->buffer);
    return MPI_SUCCESS;
}

#undef FUNCNAME
#define FUNCNAME psm_1sc_get_rndvrecv
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int psm_1sc_get_rndvrecv(MPID_Request *savreq, MPIDI_CH3_Pkt_t *pkt, int from_rank)
{
    MPIDI_CH3_Pkt_get_t *getpkt = (MPIDI_CH3_Pkt_get_t *) pkt;
    MPID_Request *req;
    uint64_t rtag, rtagsel;
    psm_error_t psmerr;
    int mpi_errno = MPI_SUCCESS;

    req = psm_create_req();
    req->kind = MPID_REQUEST_RECV;
    req->psm_flags |= PSM_RNDVRECV_GET_REQ;
    ++psm_tot_rndv_gets;
    
    /* save the get-request. GET is complete only when the original req is
     * completed. */
    req->savedreq = savreq;
    if(savreq->psm_flags & PSM_RNDVRECV_GET_PACKED) {
        DBG("GET: origin needs unpack\n");
        req->psm_flags |= PSM_RNDVRECV_GET_PACKED;
        savreq->psm_flags &= ~PSM_RNDVRECV_GET_PACKED;
    }

    rtag = 0;
    rtagsel = MQ_TAGSEL_ALL;
    MAKE_PSM_SELECTOR(rtag, MPID_CONTEXT_RNDVPSM, getpkt->rndv_tag,
                      from_rank);
    /* ch3u_rma_sync.c saved the origin_addr in dev.user_buf */
    _psm_enter_;
    if ((unlikely(getpkt->rndv_len > ipath_max_transfer_size))) {
        psmerr = psm_post_large_msg_irecv(savreq->dev.user_buf,
                        getpkt->rndv_len, &req, rtag, rtagsel);
    } else {
        psmerr = psm_mq_irecv(psmdev_cw.mq, rtag, rtagsel, MQ_FLAGS_NONE,
                          savreq->dev.user_buf, getpkt->rndv_len, req, &(req->mqreq));
    }
    _psm_exit_;
    if(unlikely(psmerr != PSM_OK)) {
        mpi_errno = psm_map_error(psmerr);
        MPIU_ERR_POP(mpi_errno);
    }

fn_fail:
    return mpi_errno;    
}

static MPID_Request *psm_1sc_putacc_rndvrecv(MPID_Request *putreq, int putlen, 
                     MPID_Request **nreq, void *useraddr, int rndv_tag, 
                     int source_rank, int rndv_len, MPIDI_VC_t *vc)
{
    MPID_Request *req = *nreq;
    MPID_Request *preq = vc->ch.recv_active;
    uint64_t rtag, rtagsel;
    psm_error_t psmerr;

    if(req == NULL) {
        req = psm_create_req();
        *nreq = req;
    }

    MPIU_Object_set_ref(req, 2);
    req->kind = MPID_REQUEST_RECV;
    req->savedreq = putreq;
    req->pktlen = putlen;

    rtag = 0;
    rtagsel = MQ_TAGSEL_ALL;
    /* pkt->target_rank is my rank */
    MAKE_PSM_SELECTOR(rtag, MPID_CONTEXT_RNDVPSM, rndv_tag,
                      source_rank);

    /* if we're receiving non-contig addtitional processing needed */
    if(!MPIR_DATATYPE_IS_PREDEFINED(preq->dev.datatype)) {
        if(!preq->dev.datatype_ptr->is_contig) {
            useraddr = psm_gen_packbuf(req, preq);
            rndv_len = req->pksz;
            req->psm_flags |= PSM_RNDVRECV_NC_REQ;
            /* we need the datatype info. keep the req pending */
            req->pending_req = preq;
        } else {
            /* its contiguous, we dont need the req anymore */
            MPIDI_CH3U_Request_complete(preq);
        }
    }
	 
    _psm_enter_;
    if ((unlikely(rndv_len > ipath_max_transfer_size))) {
        psmerr = psm_post_large_msg_irecv(useraddr, rndv_len,
                        &req, rtag, rtagsel);
    } else {
        psmerr = psm_mq_irecv(psmdev_cw.mq, rtag, rtagsel, MQ_FLAGS_NONE,
                        useraddr, rndv_len, req, &(req->mqreq));
    }
    _psm_exit_;
    if(unlikely(psmerr != PSM_OK)) {
        printf("ERROR: rndv recv failed\n");
    }
    return req;
}

#undef FUNCNAME
#define FUNCNAME psm_send_1sided_ctrlpkt
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int psm_send_1sided_ctrlpkt(MPID_Request **rptr, int dest, void *buf, 
                            MPIDI_msg_sz_t buflen, int src, int create_req)
{
    MPID_Request *req = *rptr;
    vbuf *vb;
    int mpi_errno = MPI_SUCCESS;
    psm_error_t psmerr;

    if(create_req) {
        req = psm_create_req();
        *rptr = req;
    }
    req->kind = MPID_REQUEST_SEND;
    req->psm_flags |= PSM_CONTROL_PKTREQ;
    
    vb = psm_get_vbuf();
    if(unlikely(vb == NULL)) {
        MPIU_ERR_SET(mpi_errno, MPI_ERR_NO_MEM, "**nomem");
        goto fn_fail;
    }

    req->vbufptr = vb;
    vb->req = (void*) req;
    memcpy(vb->buffer, buf, buflen);

    psmerr = psm_iput(dest, vb->buffer, buflen, req, src);
    if(unlikely(psmerr != PSM_OK)) {
        MPIU_ERR_SET(mpi_errno, MPI_ERR_INTERN, "**fail");
        goto fn_fail;
    }

fn_fail:    
    return mpi_errno;
}

/* if response is packed, unpack */

int psm_getresp_rndv_complete(MPID_Request *req, int inlen) 
{
    if(req->psm_flags & PSM_RNDVRECV_GET_PACKED) {
        DBG("GET RDNV: did unpack\n");
        MPID_Request *savq = req->savedreq;
        psm_do_unpack(savq->dev.user_count, savq->dev.datatype, NULL, savq->dev.user_buf,
                0, savq->dev.real_user_buf, inlen);
        MPID_cc_set(req->savedreq->cc_ptr, 0);
        MPIU_Free(savq->dev.user_buf);
        MPIU_Object_set_ref(req, 0);
        MPIDI_CH3_Request_destroy(req);
    }
    else {
        MPID_cc_set(req->savedreq->cc_ptr, 0);
        MPIU_Object_set_ref(req, 0);
        MPIDI_CH3_Request_destroy(req);
    }
    return MPI_SUCCESS;
}
/* get-response has completed. Call into receive handlers to decrement
 * my-counter */

int psm_getresp_complete(MPID_Request *req) 
{
    int complete = TRUE;
    MPIDI_VC_t *vc = (MPIDI_VC_t *) req->pkbuf;
    MPIDI_CH3_ReqHandler_GetSendRespComplete(vc, req, &complete);
    return MPI_SUCCESS;
}

int psm_getaccumresp_complete(MPID_Request *req) 
{
    int complete = TRUE;
    MPIDI_VC_t *vc = (MPIDI_VC_t *) req->pkbuf;
    MPIDI_CH3_ReqHandler_GetAccumRespComplete(vc, req, &complete);
    return MPI_SUCCESS;
}

static void *psm_gen_packbuf(MPID_Request *rreq, MPID_Request *dtreq) 
{
    int typesize;

    MPID_Datatype_get_size_macro(dtreq->dev.datatype, typesize);
    rreq->pksz = dtreq->dev.user_count * typesize;
    
    rreq->pkbuf = MPIU_Malloc(rreq->pksz);
    return rreq->pkbuf;
}

static int gbl_rndv_tag;
static pthread_spinlock_t taglock;
static void psm_init_tag()
{
    pthread_spin_init(&taglock, 0);
    gbl_rndv_tag = 0;
}
/* get a new tag for rndv message sending. rndv target will wait on this tag */
int psm_get_rndvtag()
{
    int an_alarmingly_long_variable;
    pthread_spin_lock(&taglock);
    ++gbl_rndv_tag;
    an_alarmingly_long_variable = gbl_rndv_tag;
    pthread_spin_unlock(&taglock);
    return an_alarmingly_long_variable;
}
