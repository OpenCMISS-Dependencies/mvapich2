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
#include "mpid_mrail_rndv.h"
#include "rdma_impl.h"
#include "mem_hooks.h"
#include "coll_shmem.h"
#include "hwloc_bind.h"
#include "cm.h"
#if defined(_MCST_SUPPORT_)
#include "ibv_mcast.h"
#endif

#define MPIDI_CH3I_HOST_DESCRIPTION_KEY "description"

MPIDI_CH3I_Process_t MPIDI_CH3I_Process;
int (*check_cq_overflow) (MPIDI_VC_t *c, int rail);
int (*perform_blocking_progress) (int hca_num, int num_cqs);
void (*handle_multiple_cqs) (int num_cqs, int cq_choice, int is_send_completion);

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3I_set_affinity
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3I_set_affinity(MPIDI_PG_t * pg, int pg_rank)
{
    char *value;
    int mpi_errno = MPI_SUCCESS;
    int my_local_id;
    MPIDI_VC_t *vc;

    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3I_SET_AFFINITY);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3I_SET_AFFINITY);

    if ((value = getenv("MV2_ENABLE_AFFINITY")) != NULL) {
        mv2_enable_affinity = atoi(value);
        #if defined(_SMP_LIMIC_)
        g_use_limic2_coll = atoi(value);
        #endif /*#if defined(_SMP_LIMIC_)*/
    }

    if (mv2_enable_affinity && (value = getenv("MV2_CPU_MAPPING")) != NULL) {
        /* Affinity is on and the user has supplied a cpu mapping string */
        int linelen = strlen(value);
        if (linelen < s_cpu_mapping_line_max) {
            s_cpu_mapping_line_max = linelen;
        }
        s_cpu_mapping =
            (char *) MPIU_Malloc(sizeof(char) * (s_cpu_mapping_line_max + 1));
        strncpy(s_cpu_mapping, value, s_cpu_mapping_line_max);
        s_cpu_mapping[s_cpu_mapping_line_max] = '\0';
    }

    if (mv2_enable_affinity && (value = getenv("MV2_CPU_MAPPING")) == NULL) {
        /* Affinity is on and the user has not specified a mapping string */
        if ((value = getenv("MV2_CPU_BINDING_POLICY")) != NULL) {
            /* User has specified a binding policy */
            if (!strcmp(value, "bunch") || !strcmp(value, "BUNCH")) {
                policy = POLICY_BUNCH;
            } else if (!strcmp(value, "scatter") || !strcmp(value, "SCATTER")) {
                policy = POLICY_SCATTER;
            } else {
                MPIU_ERR_SETFATALANDJUMP1(mpi_errno, MPI_ERR_OTHER,
                                          "**fail", "**fail %s",
                                          "CPU_BINDING_PRIMITIVE: Policy should be bunch or scatter.");
            }
        } else {
            /* User has not specified a binding policy.
             * We are going to do "bunch" binding, by default  */
            policy = POLICY_BUNCH;
        }
    }

    if (mv2_enable_affinity && (value = getenv("MV2_CPU_MAPPING")) == NULL) {
        /* Affinity is on and the user has not specified a mapping string */
        if ((value = getenv("MV2_CPU_BINDING_LEVEL")) != NULL) {
            /* User has specified a binding level */
            if (!strcmp(value, "core") || !strcmp(value, "CORE")) {
                level = LEVEL_CORE;
            } else if (!strcmp(value, "socket") || !strcmp(value, "SOCKET")) {
                level = LEVEL_SOCKET;
            } else if (!strcmp(value, "numanode") || !strcmp(value, "NUMANODE")) {
                level = LEVEL_NUMANODE;
            } else {
                MPIU_ERR_SETFATALANDJUMP1(mpi_errno, MPI_ERR_OTHER,
                                          "**fail", "**fail %s",
                                          "CPU_BINDING_PRIMITIVE: Level should be core, socket, or numanode.");
            }
        } else {
            /* User has not specified a binding level.
             * We are going to do "core" binding, by default  */
            level = LEVEL_CORE;
        }
    }

    /* Get my VC */
    MPIDI_PG_Get_vc(pg, pg_rank, &vc);
    my_local_id = vc->smp.local_rank;

    if (mv2_enable_affinity) {
        mpi_errno = smpi_setaffinity(my_local_id);
        if (mpi_errno != MPI_SUCCESS) {
            MPIU_ERR_POP(mpi_errno);
        }
    }
  fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3I_SET_AFFINITY);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME split_type
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
static int split_type(MPID_Comm * comm_ptr, int stype, int key,
                      MPID_Info *info_ptr, MPID_Comm ** newcomm_ptr)
{
    MPID_Node_id_t id;
    MPIR_Rank_t nid;
    int mpi_errno = MPI_SUCCESS;

    mpi_errno = MPID_Get_node_id(comm_ptr, comm_ptr->rank, &id);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);

    nid = (stype == MPI_COMM_TYPE_SHARED) ? id : MPI_UNDEFINED;
    mpi_errno = MPIR_Comm_split_impl(comm_ptr, nid, key, newcomm_ptr);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);

  fn_exit:
    return mpi_errno;

    /* --BEGIN ERROR HANDLING-- */
  fn_fail:
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

static MPID_CommOps comm_fns = {
    split_type
};

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Init
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_Init(int has_parent, MPIDI_PG_t * pg, int pg_rank)
{
    int mpi_errno = MPI_SUCCESS;
    int pg_size, threshold, dpm = 0, p;
    char *dpm_str, *value, *conn_info = NULL;
    int mv2_rdma_init_timers = 0;
    MPIDI_VC_t *vc;

    /* Override split_type */
    MPID_Comm_fns = &comm_fns;

    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_INIT);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_INIT);

    /* Explicitly initializing RDMA_FP to 0 */
    mv2_MPIDI_CH3I_RDMA_Process.has_adaptive_fast_path = 0;

    if (MPIDI_CH3_Pkt_size_index[MPIDI_CH3_PKT_CLOSE] !=
        sizeof(MPIDI_CH3_Pkt_close_t)) {
        MPIU_ERR_SETFATALANDJUMP1(mpi_errno, MPI_ERR_OTHER, "**fail",
                                  "**fail %s",
                                  "Failed sanity check! Packet size table mismatch");
    }

    pg_size = MPIDI_PG_Get_size(pg);

    /* Allocate PMI Key Value Pair */
    mv2_allocate_pmi_keyval();

    mpi_errno = MPIDI_CH3U_Comm_register_create_hook(MPIDI_CH3I_comm_create, NULL);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);
    
    /*Determine to use which connection management */
    threshold = MPIDI_CH3I_CM_DEFAULT_ON_DEMAND_THRESHOLD;

    /*check ON_DEMAND_THRESHOLD */
    value = getenv("MV2_ON_DEMAND_THRESHOLD");
    if (value) {
        threshold = atoi(value);
    }

    dpm_str = getenv("MV2_SUPPORT_DPM");
    if (dpm_str) {
        dpm = !!atoi(dpm_str);
    }
    MPIDI_CH3I_Process.has_dpm = dpm;
    if (MPIDI_CH3I_Process.has_dpm) {
        setenv("MV2_ENABLE_AFFINITY", "0", 1);
    }

    value = getenv("MV2_USE_XRC");
    if (value) {
#ifdef _ENABLE_XRC_
        USE_XRC = !!atoi(value);
        if (atoi(value)) {
            /* Enable on-demand */
            threshold = 1;
        }
#else
        if (atoi(value)) {
            MPIU_Error_printf
                ("XRC support is not configured. Please retry with "
                 "MV2_USE_XRC=0 (or) Reconfigure MVAPICH2 library without --disable-xrc.\n");
            MPIU_ERR_SETFATALANDJUMP(mpi_errno, MPI_ERR_OTHER, "**fail");
        }
#endif
    }

    if ((value = getenv("MV2_USE_CUDA")) != NULL) {
        if (atoi(value)) {
#ifdef _ENABLE_CUDA_
            rdma_enable_cuda = atoi(value);
            if (rdma_enable_cuda) {
                cuda_get_user_parameters();
            }
#else
            MPIU_Error_printf("GPU CUDA support is not configured. "
                              "Please reconfigure MVAPICH2 library with --enable-cuda option.\n");
            MPIU_ERR_SETFATALANDJUMP(mpi_errno, MPI_ERR_OTHER, "**fail");
#endif
        }
    }
#ifdef _ENABLE_UD_
    int i = 0;
    for (i = 0; i < MAX_NUM_HCAS; ++i) {
        mv2_MPIDI_CH3I_RDMA_Process.ud_rails[i] = NULL;
    }
    mv2_MPIDI_CH3I_RDMA_Process.remote_ud_info = NULL;

    if ((value = getenv("MV2_HYBRID_ENABLE_THRESHOLD")) != NULL) {
        rdma_hybrid_enable_threshold = atoi(value);
    }
    if ((value = getenv("MV2_USE_UD_HYBRID")) != NULL) {
        rdma_enable_hybrid = atoi(value);
    }
    if ((value = getenv("MV2_USE_ONLY_UD")) != NULL) {
        rdma_enable_hybrid = atoi(value);
        rdma_enable_only_ud = atoi(value);
        if ((value = getenv("MV2_HYBRID_ENABLE_THRESHOLD")) != NULL) {
            if (atoi(value) > 0) {
                PRINT_INFO((pg_rank == 0),
                           "User requested only UD. Resetting MV2_HYBRID_ENABLE_THRESHOLD to 0.\n");
            }
        }
        rdma_hybrid_enable_threshold = 0;
    }
    if (pg_size < rdma_hybrid_enable_threshold) {
        rdma_enable_hybrid = 0;
    }
    if(rdma_enable_hybrid == 1) { 
        /* The zero-copy bcast design is disabled when 
         * hybrid is used */ 
        mv2_enable_zcpy_bcast = 0; 
        mv2_enable_zcpy_reduce = 0; 
        mv2_rdma_init_timers = 1;
    } 
#endif

#if defined(RDMA_CM)
    if (((value = getenv("MV2_USE_RDMA_CM")) != NULL
         || (value = getenv("MV2_USE_IWARP_MODE")) != NULL)
        && atoi(value) && !dpm) {
        MPIDI_CH3I_Process.cm_type = MPIDI_CH3I_CM_RDMA_CM;
#ifdef _ENABLE_XRC_
        USE_XRC = 0;
        value = getenv("MV2_USE_XRC");
        if (value && (pg_rank == 0)) {
            if (atoi(value)) {
                MPIU_Error_printf("Error: XRC does not work with RDMA CM. "
                                  "Proceeding without XRC support.\n");
            }
        }
#endif
    } else {
        rdma_cm_get_hca_type(&mv2_MPIDI_CH3I_RDMA_Process);
    }
#endif /* defined(RDMA_CM) */

    if (MPIDI_CH3I_Process.cm_type != MPIDI_CH3I_CM_RDMA_CM) {
        if (pg_size > threshold || dpm
#ifdef _ENABLE_XRC_
            || USE_XRC
#endif /* _ENABLE_XRC_ */
#ifdef _ENABLE_UD_
            || rdma_enable_hybrid
#endif
    ) {
            MPIDI_CH3I_Process.cm_type = MPIDI_CH3I_CM_ON_DEMAND;
            MPIDI_CH3I_Process.num_conn = 0;
        } else {
            MPIDI_CH3I_Process.cm_type = MPIDI_CH3I_CM_BASIC_ALL2ALL;
        }
    }

    MPIDI_PG_GetConnKVSname(&pg->ch.kvs_name);

#if defined(CKPT)
#if defined(RDMA_CM)
    if (MPIDI_CH3I_Process.cm_type == MPIDI_CH3I_CM_RDMA_CM) {
        MPIU_Error_printf("Error: Checkpointing does not work with RDMA CM.\n"
                          "Please configure and compile MVAPICH2 with checkpointing disabled "
                          "or without support for RDMA CM.\n");
        MPIU_ERR_SETFATALANDJUMP(mpi_errno, MPI_ERR_OTHER, "**fail");
    }
#endif /* defined(RDMA_CM) */

    // Always use CM_ON_DEMAND for Checkpoint/Restart and Migration
    MPIDI_CH3I_Process.cm_type = MPIDI_CH3I_CM_ON_DEMAND;

#endif /* defined(CKPT) */
#ifdef _ENABLE_UD_
    if (rdma_enable_hybrid) {
        MPIU_Assert(MPIDI_CH3I_Process.cm_type == MPIDI_CH3I_CM_ON_DEMAND);
    }
#endif

    if (MPIDI_CH3I_Process.cm_type == MPIDI_CH3I_CM_RDMA_CM) {
        check_cq_overflow           = check_cq_overflow_for_iwarp;
        handle_multiple_cqs         = handle_multiple_cqs_for_iwarp;
        MPIDI_CH3I_MRAILI_Cq_poll   = MPIDI_CH3I_MRAILI_Cq_poll_iwarp;
        perform_blocking_progress   = perform_blocking_progress_for_iwarp;
    } else {
        check_cq_overflow           = check_cq_overflow_for_ib;
        handle_multiple_cqs         = handle_multiple_cqs_for_ib;
        MPIDI_CH3I_MRAILI_Cq_poll   = MPIDI_CH3I_MRAILI_Cq_poll_ib;
        perform_blocking_progress   = perform_blocking_progress_for_ib;
    }

    /* save my vc_ptr for easy access */
    MPIDI_PG_Get_vc(pg, pg_rank, &MPIDI_CH3I_Process.vc);

    /* Initialize Progress Engine */
    if ((mpi_errno = MPIDI_CH3I_Progress_init())) {
        MPIU_ERR_POP(mpi_errno);
    }

    /* get parameters from the job-launcher */
    rdma_get_pm_parameters(&mv2_MPIDI_CH3I_RDMA_Process);

    /* Check for SMP only */
    MPIDI_CH3I_set_smp_only();

    if (!SMP_ONLY) {

#if !defined(DISABLE_PTMALLOC)
        if (mvapich2_minit()) {
            if (pg_rank == 0) {
                MPIU_Error_printf("WARNING: Error in initializing MVAPICH2 ptmalloc library."
                "Continuing without InfiniBand registration cache support.\n");
            }
            mv2_MPIDI_CH3I_RDMA_Process.has_lazy_mem_unregister = 0;
        }
#else /* !defined(DISABLE_PTMALLOC) */
        mallopt(M_TRIM_THRESHOLD, -1);
        mallopt(M_MMAP_MAX, 0);
        mv2_MPIDI_CH3I_RDMA_Process.has_lazy_mem_unregister = 0;
#endif /* !defined(DISABLE_PTMALLOC) */

        switch (MPIDI_CH3I_Process.cm_type) {
                /* allocate rmda memory and set up the queues */
#if defined(RDMA_CM)
            case MPIDI_CH3I_CM_RDMA_CM:
                mpi_errno = MPIDI_CH3I_RDMA_CM_Init(pg, pg_rank, &conn_info);
                if (mpi_errno != MPI_SUCCESS) {
                    MPIU_ERR_POP(mpi_errno);
                }
                break;
#endif /* defined(RDMA_CM) */
            case MPIDI_CH3I_CM_ON_DEMAND:
                mpi_errno = MPIDI_CH3I_CM_Init(pg, pg_rank, &conn_info);
                if (mpi_errno != MPI_SUCCESS) {
                    MPIU_ERR_POP(mpi_errno);
                }
                break;
            default:
                /*call old init to setup all connections */
                if ((mpi_errno =
                     MPIDI_CH3I_RDMA_init(pg, pg_rank)) != MPI_SUCCESS) {
                    MPIU_ERR_POP(mpi_errno);
                }

                /* All vc should be connected */
                for (p = 0; p < pg_size; ++p) {
                    MPIDI_PG_Get_vc(pg, p, &vc);
                    vc->ch.state = MPIDI_CH3I_VC_STATE_IDLE;
                }
                break;
        }
        /* Read RDMA FAST Path related params */
        rdma_set_rdma_fast_path_params(pg_size);
    }
#if defined(CKPT)
#if defined(DISABLE_PTMALLOC)
    MPIU_Error_printf("Error: Checkpointing does not work without registration "
                      "caching enabled.\nPlease configure and compile MVAPICH2 without checkpointing "
                      " or enable registration caching.\n");
    MPIU_ERR_SETFATALANDJUMP(mpi_errno, MPI_ERR_OTHER, "**fail");
#endif /* defined(DISABLE_PTMALLOC) */

    if ((mpi_errno = MPIDI_CH3I_CR_Init(pg, pg_rank, pg_size))) {
        MPIU_ERR_POP(mpi_errno);
    }
#endif /* defined(CKPT) */

    if (conn_info) {
        /* set connection info for dynamic process management */
        if (dpm) {
	        mpi_errno = MPIDI_PG_SetConnInfo(pg_rank, (const char *) conn_info);
	        if (mpi_errno != MPI_SUCCESS) {
	            MPIU_ERR_POP(mpi_errno);
	        }
        }
        MPIU_Free(conn_info);
    }

    MV2_collectives_arch_init(mv2_MPIDI_CH3I_RDMA_Process.heterogeneity);

    if (MPIDI_CH3I_set_affinity(pg, pg_rank) != MPI_SUCCESS) {
        MPIU_ERR_POP(mpi_errno);
    }

    /* Initialize the smp channel */
    if ((mpi_errno = MPIDI_CH3I_SMP_init(pg))) {
        MPIU_ERR_POP(mpi_errno);
    }

    if (SMP_INIT) {
        for (p = 0; p < pg_size; ++p) {
            MPIDI_PG_Get_vc(pg, p, &vc);
            /* Mark the SMP VC as Idle */
            if (vc->smp.local_nodes >= 0) {
                vc->ch.state = MPIDI_CH3I_VC_STATE_IDLE;
                if (SMP_ONLY) {
                    MPIDI_CH3I_SMP_Init_VC(vc);
                }
#ifdef _ENABLE_XRC_
                VC_XST_SET(vc, XF_SMP_VC);
#endif
            }
        }
    } else {
        extern int mv2_enable_shmem_collectives;
        mv2_enable_shmem_collectives = SMP_INIT;
    }

    /* Set the eager max msg size now that we know SMP and RDMA are initialized.
     * The max message size is also set during VC initialization, but the state
     * of SMP is unknown at that time.
     */
    for (p = 0; p < pg_size; ++p) {
        MPIDI_PG_Get_vc(pg, p, &vc);
        vc->eager_max_msg_sz = MPIDI_CH3_EAGER_MAX_MSG_SIZE(vc);
    }

    if ((value = getenv("MV2_SHOW_ENV_INFO")) != NULL) {
        mv2_show_env_info = atoi(value);
    }
    if (pg_rank == 0 && mv2_show_env_info) {
        mv2_print_env_info(&mv2_MPIDI_CH3I_RDMA_Process);
    }

#if defined(_MCST_SUPPORT_)
    if (rdma_enable_mcast) {
        mv2_rdma_init_timers = 1;
        /* TODO : Is there a better way to seed? */
        srand(time(NULL) * pg_rank);

        /* initialize comm table */
        for (p = 0; p <= MV2_MCAST_MAX_COMMS; p++) {
            comm_table[p] = NULL;
        }
        /* init mcast context */
        mcast_ctx = MPIU_Malloc (sizeof(mcast_context_t));
        mcast_ctx->init_list = NULL;
        mcast_ctx->ud_ctx = mv2_mcast_prepare_ud_ctx();
        if (mcast_ctx->ud_ctx == NULL) {
            PRINT_ERROR("Error in create multicast UD context for multicast\n");
            exit(EXIT_FAILURE);
        }
    }
#endif

    if (mv2_rdma_init_timers) {
        mv2_init_timers();
    }

    mpi_errno = MPIDI_CH3U_Comm_register_destroy_hook(MPIDI_CH3I_comm_destroy, NULL);
    if (mpi_errno) MPIU_ERR_POP(mpi_errno);


  fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_INIT);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_VC_Init
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_VC_Init(MPIDI_VC_t * vc)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_VC_INIT);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_VC_INIT);
    int mpi_errno = MPI_SUCCESS;
    vc->smp.local_nodes = -1;
#if !defined (CHANNEL_PSM)
    vc->smp.sendq_head = NULL;
    vc->smp.sendq_tail = NULL;
    vc->smp.recv_active = NULL;
    vc->smp.send_active = NULL;
    vc->ch.req = NULL;
    vc->mrail.rails = NULL;
    vc->mrail.srp.credits = NULL;
    vc->mrail.cmanager.msg_channels = NULL;
#endif /* #if !defined (CHANNEL_PSM) */
    vc->ch.sendq_head = NULL;
    vc->ch.sendq_tail = NULL;
    vc->ch.req = (MPID_Request *) MPIU_Malloc(sizeof(MPID_Request));
    if (!vc->ch.req) {
        MPIU_CHKMEM_SETERR(mpi_errno, sizeof(MPID_Request), "MPID Request");
    }
    /* vc->ch.state = MPIDI_CH3I_VC_STATE_IDLE; */
    vc->ch.state = MPIDI_CH3I_VC_STATE_UNCONNECTED;
    vc->ch.read_state = MPIDI_CH3I_READ_STATE_IDLE;
    vc->ch.recv_active = NULL;
    vc->ch.send_active = NULL;
    vc->ch.cm_sendq_head = NULL;
    vc->ch.cm_sendq_tail = NULL;
    vc->ch.cm_1sc_sendq_head = NULL;
    vc->ch.cm_1sc_sendq_tail = NULL;
    vc->ch.pending_r3_data = 0;
    vc->ch.received_r3_data = 0;
#ifdef _ENABLE_XRC_
    vc->ch.xrc_flags = 0;
    vc->ch.xrc_conn_queue = NULL;
    vc->ch.orig_vc = NULL;
    memset(vc->ch.xrc_srqn, 0, sizeof(uint32_t) * MAX_NUM_HCAS);
    memset(vc->ch.xrc_rqpn, 0, sizeof(uint32_t) * MAX_NUM_SUBRAILS);
    memset(vc->ch.xrc_my_rqpn, 0, sizeof(uint32_t) * MAX_NUM_SUBRAILS);
#endif

    vc->smp.hostid = -1;
    vc->force_rndv = 0;

    vc->rndvSend_fn = MPID_MRAIL_RndvSend;
    vc->rndvRecv_fn = MPID_MRAIL_RndvRecv;

#if defined(CKPT)
    vc->ch.rput_stop = 0;
#endif /* defined(CKPT) */

#ifdef USE_RDMA_UNEX
    vc->ch.unex_finished_next = NULL;
    vc->ch.unex_list = NULL;
#endif
    /* It is needed for temp vc */
    vc->eager_max_msg_sz = rdma_iba_eager_threshold;

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_VC_INIT);
    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_PortFnsInit
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_PortFnsInit(MPIDI_PortFns * portFns)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_RDMA_PORTFNSINIT);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_RDMA_PORTFNSINIT);

    if (!MPIDI_CH3I_Process.has_dpm) {
        portFns->OpenPort = 0;
        portFns->ClosePort = 0;
        portFns->CommAccept = 0;
        portFns->CommConnect = 0;
    } else
        MPIU_UNREFERENCED_ARG(portFns);

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_RDMA_PORTFNSINIT);
    return MPI_SUCCESS;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Connect_to_root
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_Connect_to_root(const char *port_name, MPIDI_VC_t ** new_vc)
{
    int mpi_errno = MPI_SUCCESS;
    int str_errno;
    char ifname[MAX_HOST_DESCRIPTION_LEN];
    MPIDI_VC_t *vc;
    MPIDI_CH3_Pkt_cm_establish_t pkt;
    MPID_Request *sreq;
    int seqnum;

    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_CONNECT_TO_ROOT);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_CONNECT_TO_ROOT);

    *new_vc = NULL;
    if (!MPIDI_CH3I_Process.has_dpm)
        return MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_FATAL, FCNAME,
                                    __LINE__, MPI_ERR_OTHER, "**notimpl", 0);

    str_errno = MPIU_Str_get_string_arg(port_name,
                                        MPIDI_CH3I_HOST_DESCRIPTION_KEY,
                                        ifname, MAX_HOST_DESCRIPTION_LEN);
    if (str_errno != MPIU_STR_SUCCESS) {
        /* --BEGIN ERROR HANDLING */
        if (str_errno == MPIU_STR_FAIL) {
            MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER,
                                "**argstr_missinghost");
        } else {
            /* MPIU_STR_TRUNCATED or MPIU_STR_NONEM */
            MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**argstr_hostd");
        }
        /* --END ERROR HANDLING-- */
    }

    vc = MPIU_Malloc(sizeof(MPIDI_VC_t));
    if (!vc) {
        MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**nomem");
    }
    MPIDI_VC_Init(vc, NULL, 0);

    mpi_errno = MPIDI_CH3I_CM_Connect_raw_vc(vc, ifname);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

    while (vc->ch.state != MPIDI_CH3I_VC_STATE_IDLE) {
        mpi_errno = MPID_Progress_test();
        /* --BEGIN ERROR HANDLING-- */
        if (mpi_errno != MPI_SUCCESS) {
            MPIU_ERR_POP(mpi_errno);
        }
    }

    /* fprintf(stderr, "[###] vc state to idel, now send cm_establish msg\n") */
    /* Now a connection is created, send a cm_establish message */
    /* FIXME: vc->mrail.remote_vc_addr is used to find remote vc
     * A more elegant way is needed */
    MPIDI_Pkt_init(&pkt, MPIDI_CH3_PKT_CM_ESTABLISH);
    MPIDI_VC_FAI_send_seqnum(vc, seqnum);
    MPIDI_Pkt_set_seqnum(&pkt, seqnum);
    pkt.vc_addr = vc->mrail.remote_vc_addr;
    mpi_errno = MPIDI_GetTagFromPort(port_name, &pkt.port_name_tag);
    if (mpi_errno != MPIU_STR_SUCCESS) {
        MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**argstr_port_name_tag");
    }

    mpi_errno = MPIDI_CH3_iStartMsg(vc, &pkt, sizeof(pkt), &sreq);
    if (mpi_errno != MPI_SUCCESS) {
        MPIU_ERR_SETANDJUMP1(mpi_errno, MPI_ERR_OTHER, "**fail", "**fail %s",
                             "Failed to send cm establish message");
    }

    if (sreq != NULL) {
        if (sreq->status.MPI_ERROR != MPI_SUCCESS) {
            mpi_errno = MPIR_Err_create_code(sreq->status.MPI_ERROR,
                                             MPIR_ERR_FATAL, FCNAME, __LINE__,
                                             MPI_ERR_OTHER, "**fail", 0);
            MPID_Request_release(sreq);
            goto fn_fail;
        }
        MPID_Request_release(sreq);
    }

    *new_vc = vc;

  fn_fail:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_CONNECT_TO_ROOT);

    return mpi_errno;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_Get_business_card
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_Get_business_card(int myRank, char *value, int length)
{
    char ifname[MAX_HOST_DESCRIPTION_LEN];
    int mpi_errno;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_GET_BUSINESS_CARD);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_GET_BUSINESS_CARD);

    mpi_errno = MPIDI_CH3I_CM_Get_port_info(ifname, MAX_HOST_DESCRIPTION_LEN);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

    mpi_errno = MPIU_Str_add_string_arg(&value, &length,
                                        MPIDI_CH3I_HOST_DESCRIPTION_KEY,
                                        ifname);
    if (mpi_errno != MPIU_STR_SUCCESS) {
        if (mpi_errno == MPIU_STR_NOMEM) {
            MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**buscard_len");
        } else {
            MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**buscard");
        }
    }

  fn_fail:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_GET_BUSINESS_CARD);
    return mpi_errno;
}

/* This routine is a hook for initializing information for a process
   group before the MPIDI_CH3_VC_Init routine is called */
#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_PG_Init
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_PG_Init(MPIDI_PG_t * pg)
{
    return MPIDI_CH3I_MRAIL_PG_Init(pg);
}

/* This routine is a hook for any operations that need to be performed before
   freeing a process group */
#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_PG_Destroy
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_PG_Destroy(struct MPIDI_PG *pg)
{
    return MPIDI_CH3I_MRAIL_PG_Destroy(pg);
}

/* This routine is a hook for any operations that need to be performed before
   freeing a virtual connection */
#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_VC_Destroy
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_VC_Destroy(struct MPIDI_VC *vc)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_VC_DESTROY);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_VC_DESTROY);

#if !defined (CHANNEL_PSM)
    if (vc->smp.sendq_head != NULL) {
        MPIU_Free(vc->smp.sendq_head);
    }
    if (vc->smp.sendq_tail != NULL) {
        MPIU_Free(vc->smp.sendq_tail);
    }
    if (vc->smp.recv_active != NULL) {
        MPIU_Free(vc->smp.recv_active);
    }
    if (vc->smp.send_active != NULL) {
        MPIU_Free(vc->smp.send_active);
    }
    if (vc->ch.req != NULL) {
        MPIU_Free(vc->ch.req);
    }
    if (vc->mrail.cmanager.msg_channels != NULL) {
        MPIU_Free(vc->mrail.cmanager.msg_channels);
    }
    if (vc->mrail.srp.credits != NULL) {
        MPIU_Free(vc->mrail.srp.credits);
    }
    if (vc->mrail.rails != NULL) {
        MPIU_Free(vc->mrail.rails);
    }
#endif /* #if !defined (CHANNEL_PSM) */


    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_VC_DESTROY);
    return MPI_SUCCESS;
}

/* A dummy function so that all channels provide the same set of functions,
   enabling dll channels */
#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_InitCompleted
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_InitCompleted(void)
{
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_INITCOMPLETED);
    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_INITCOMPLETED);

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_INITCOMPLETED);
    return MPI_SUCCESS;
}


void rdma_process_hostid(MPIDI_PG_t * pg, int *host_ids, int my_rank,
                         int pg_size)
{
    int i;
    int my_host_id;;
    MPIDI_VC_t *vc = NULL;

    pg->ch.local_process_id = 0;
    pg->ch.num_local_processes = 0;

    my_host_id = host_ids[my_rank];
    for (i = 0; i < pg_size; ++i) {
        MPIDI_PG_Get_vc(pg, i, &vc);
        if (host_ids[i] == my_host_id) {
            vc->smp.local_rank = pg->ch.num_local_processes++;
            if (i == my_rank) {
                pg->ch.local_process_id = vc->smp.local_rank;
            }
        } else {
            vc->smp.local_rank = -1;
        }
    }
}

/* vi: set sw=4 */
