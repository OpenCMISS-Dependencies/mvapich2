/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* Copyright (c) 2001-2014, The Ohio State University. All rights
* reserved.
*
* This file is part of the MVAPICH2 software package developed by the
* team members of The Ohio State University's Network-Based Computing
* Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
*
* For detailed copyright and licensing information, please refer to the
* copyright file COPYRIGHT in the top level MVAPICH2 directory.
*/
/*
*
*  (C) 2001 by Argonne National Laboratory.
*      See COPYRIGHT in top-level directory.
*/

#include "mpiimpl.h"

#include "datatype.h"
#include "coll_shmem.h"
#include "allgather_tuning.h"
extern struct coll_runtime mv2_coll_param;
extern int allgather_tuning_algo;
extern int allgather_algo_num;
extern int use_2lvl_allgather;

#define FGP_SWITCH_FACTOR 4     /*Used to determine switch between naive and FGP
                                   design */

int (*MV2_Allgather_function)(const void *sendbuf,
                             int sendcount,
                             MPI_Datatype sendtype,
                             void *recvbuf,
                             int recvcount,
                             MPI_Datatype recvtype, MPID_Comm * comm_ptr,
                             int *errflag);

int MPIR_Allgather_RD_Allgather_Comm_MV2(const void *sendbuf,
                                 int sendcount,
                                 MPI_Datatype sendtype,
                                 void *recvbuf,
                                 int recvcount,
                                 MPI_Datatype recvtype, MPID_Comm * comm_ptr,
                                 int *errflag)
{
    return 0;
}

int allgather_tuning(int comm_size, int pof2)
{

    char *value;
    if (pof2 == 1 && (value = getenv("MV2_ALLGATHER_RD_THRESHOLD")) != NULL) {
        /* pof2 case. User has set the run-time parameter
           "MV2_ALLGATHER_RD_THRESHOLD".
           * Just use that value */
        return mv2_coll_param.allgather_rd_threshold;
    }
    if (pof2 == 0 && (value = getenv("MV2_ALLGATHER_BRUCK_THRESHOLD")) != NULL) {
        /* Non-pof2 case. User has set the run-time parameter
           "MV2_ALLGATHER_BRUCK_THRESHOLD".
           * Just use that value */
        return mv2_coll_param.allgather_bruck_threshold;
    } else {
        /* User has not used any run-time parameters.
         */
        if (comm_size <= MV2_ALLGATHER_SMALL_SYSTEM_SIZE) {
            return mv2_tuning_table[ALLGATHER_ID][SMALL];
        } else if (comm_size > MV2_ALLGATHER_SMALL_SYSTEM_SIZE
                   && comm_size <= MV2_ALLGATHER_MEDIUM_SYSTEM_SIZE) {
            return mv2_tuning_table[ALLGATHER_ID][MEDIUM];
        } else {
            return mv2_tuning_table[ALLGATHER_ID][LARGE];
        }
    }
}

/* This is the default implementation of allgather. The algorithm is:

Algorithm: MPI_Allgather

For short messages and non-power-of-two no. of processes, we use
the algorithm from the Jehoshua Bruck et al IEEE TPDS Nov 97
paper. It is a variant of the disemmination algorithm for
barrier. It takes ceiling(lg p) steps.

Cost = lgp.alpha + n.((p-1)/p).beta
where n is total size of data gathered on each process.

For short or medium-size messages and power-of-two no. of
processes, we use the recursive doubling algorithm.

Cost = lgp.alpha + n.((p-1)/p).beta

TODO: On TCP, we may want to use recursive doubling instead of the Bruck
algorithm in all cases because of the pairwise-exchange property of
recursive doubling (see Benson et al paper in Euro PVM/MPI
2003).

It is interesting to note that either of the above algorithms for
MPI_Allgather has the same cost as the tree algorithm for MPI_Gather!

For long messages or medium-size messages and non-power-of-two
no. of processes, we use a ring algorithm. In the first step, each
process i sends its contribution to process i+1 and receives
the contribution from process i-1 (with wrap-around). From the
second step onwards, each process i forwards to process i+1 the
data it received from process i-1 in the previous step. This takes
a total of p-1 steps.

Cost = (p-1).alpha + n.((p-1)/p).beta

We use this algorithm instead of recursive doubling for long
messages because we find that this communication pattern (nearest
neighbor) performs twice as fast as recursive doubling for long
messages (on Myrinet and IBM SP).

Possible improvements:

End Algorithm: MPI_Allgather
*/
/* begin:nested */
/* not declared static because a machine-specific function may call this
one in some cases */

#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_RD_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_RD_MV2(const void *sendbuf,
                          int sendcount,
                          MPI_Datatype sendtype,
                          void *recvbuf,
                          int recvcount,
                          MPI_Datatype recvtype, MPID_Comm * comm_ptr,
                          int *errflag)
{
    int comm_size, rank;
    int mpi_errno = MPI_SUCCESS;
    int mpi_errno_ret = MPI_SUCCESS;
    MPI_Aint recvtype_extent;
    int j, i;
    int curr_cnt, dst;
    MPI_Comm comm;
    MPI_Status status;
    int mask, dst_tree_root, my_tree_root, is_homogeneous,
        send_offset, recv_offset, last_recv_cnt = 0, nprocs_completed, k,
        offset, tmp_mask, tree_root;
#ifdef MPID_HAS_HETERO
    int position, tmp_buf_size, nbytes;
#endif

    comm = comm_ptr->handle;
    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;

    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);

    is_homogeneous = 1;
#ifdef MPID_HAS_HETERO
    if (comm_ptr->is_hetero) {
        is_homogeneous = 0;
    }
#endif

    if (is_homogeneous) {
        /* homogeneous. no need to pack into tmp_buf on each node. copy
         * local data into recvbuf */
        if (sendbuf != MPI_IN_PLACE) {
            mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                                       ((char *) recvbuf +
                                        rank * recvcount * recvtype_extent),
                                       recvcount, recvtype);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }
        }

        curr_cnt = recvcount;

        mask = 0x1;
        i = 0;
        while (mask < comm_size) {
            dst = rank ^ mask;

            /* find offset into send and recv buffers. zero out
             * the least significant "i" bits of rank and dst to
             * find root of src and dst subtrees. Use ranks of
             * roots as index to send from and recv into buffer */

            dst_tree_root = dst >> i;
            dst_tree_root <<= i;

            my_tree_root = rank >> i;
            my_tree_root <<= i;

            /* FIXME: saving an MPI_Aint into an int */
            send_offset = my_tree_root * recvcount * recvtype_extent;
            recv_offset = dst_tree_root * recvcount * recvtype_extent;

            if (dst < comm_size) {
                mpi_errno =
                    MPIC_Sendrecv(((char *) recvbuf + send_offset),
                                     curr_cnt, recvtype, dst,
                                     MPIR_ALLGATHER_TAG,
                                     ((char *) recvbuf + recv_offset),
                                     (comm_size -
                                      dst_tree_root) * recvcount, recvtype,
                                     dst, MPIR_ALLGATHER_TAG, comm, &status, errflag);
                if (mpi_errno) {
                    /* for communication errors, just record the error but
                       continue */
                    *errflag = TRUE;
                    MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                    MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                    last_recv_cnt = 0;
                }

                MPIR_Get_count_impl(&status, recvtype, &last_recv_cnt);
                curr_cnt += last_recv_cnt;
            }

            /* if some processes in this process's subtree in this step
             * did not have any destination process to communicate with
             * because of non-power-of-two, we need to send them the
             * data that they would normally have received from those
             * processes. That is, the haves in this subtree must send to
             * the havenots. We use a logarithmic recursive-halfing algorithm
             * for this. */

            /* This part of the code will not currently be
             * executed because we are not using recursive
             * doubling for non power of two. Mark it as experimental
             * so that it doesn't show up as red in the coverage
             * tests. */

            /* --BEGIN EXPERIMENTAL-- */
            if (dst_tree_root + mask > comm_size) {
                nprocs_completed = comm_size - my_tree_root - mask;
                /* nprocs_completed is the number of processes in this
                 * subtree that have all the data. Send data to others
                 * in a tree fashion. First find root of current tree
                 * that is being divided into two. k is the number of
                 * least-significant bits in this process's rank that
                 * must be zeroed out to find the rank of the root */
                j = mask;
                k = 0;
                while (j) {
                    j >>= 1;
                    k++;
                }
                k--;

                /* FIXME: saving an MPI_Aint into an int */
                offset = recvcount * (my_tree_root + mask) * recvtype_extent;
                tmp_mask = mask >> 1;

                while (tmp_mask) {
                    dst = rank ^ tmp_mask;

                    tree_root = rank >> k;
                    tree_root <<= k;

                    /* send only if this proc has data and destination
                     * doesn't have data. at any step, multiple processes
                     * can send if they have the data */
                    if ((dst > rank) && (rank < tree_root + nprocs_completed)
                        && (dst >= tree_root + nprocs_completed)) {
                        mpi_errno =
                            MPIC_Send(((char *) recvbuf + offset),
                                         last_recv_cnt, recvtype, dst,
                                         MPIR_ALLGATHER_TAG, comm, errflag);
                        /* last_recv_cnt was set in the previous
                         * receive. that's the amount of data to be
                         * sent now. */
                        if (mpi_errno) {
                            /* for communication errors, just record the error
                               but continue */
                            *errflag = TRUE;
                            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                        }
                    }
                    /* recv only if this proc. doesn't have data and sender
                     * has data */
                    else if ((dst < rank) &&
                             (dst < tree_root + nprocs_completed) &&
                             (rank >= tree_root + nprocs_completed)) {
                        mpi_errno =
                            MPIC_Recv(((char *) recvbuf + offset),
                                         (comm_size -
                                          (my_tree_root +
                                           mask)) * recvcount, recvtype,
                                         dst, MPIR_ALLGATHER_TAG, comm, &status, errflag);
                        /* nprocs_completed is also equal to the
                         * no. of processes whose data we don't have */
                        if (mpi_errno) {
                            /* for communication errors, just record the error
                               but continue */
                            *errflag = TRUE;
                            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                            last_recv_cnt = 0;
                        }
                        MPIR_Get_count_impl(&status, recvtype, &last_recv_cnt);
                        curr_cnt += last_recv_cnt;
                    }
                    tmp_mask >>= 1;
                    k--;
                }
            }
            /* --END EXPERIMENTAL-- */

            mask <<= 1;
            i++;
        }
    }
#ifdef MPID_HAS_HETERO
    else {
        /* heterogeneous. need to use temp. buffer. */

        MPIR_Pack_size_impl(recvcount * comm_size, recvtype, &tmp_buf_size);

        tmp_buf = MPIU_Malloc(tmp_buf_size);
        /* --BEGIN ERROR HANDLING-- */
        if (!tmp_buf) {
            mpi_errno =
                MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                     FCNAME, __LINE__, MPI_ERR_OTHER, "**nomem", 0);
            return mpi_errno;
        }
        /* --END ERROR HANDLING-- */

        /* calculate the value of nbytes, the number of bytes in packed
         * representation that each process contributes. We can't simply divide
         * tmp_buf_size by comm_size because tmp_buf_size is an upper
         * bound on the amount of memory required. (For example, for
         * a single integer, MPICH-1 returns pack_size=12.) Therefore, we
         * actually pack some data into tmp_buf and see by how much
         * 'position' is incremented. */

        position = 0;
        MPIR_Pack_impl(recvbuf, 1, recvtype, tmp_buf, tmp_buf_size, &position);
        nbytes = position * recvcount;

        /* pack local data into right location in tmp_buf */
        position = rank * nbytes;
        if (sendbuf != MPI_IN_PLACE) {
            MPIR_Pack_impl(sendbuf, sendcount, sendtype, tmp_buf,
                           tmp_buf_size, &position);
        } else {
            /* if in_place specified, local data is found in recvbuf */
            MPIR_Pack_impl(((char *) recvbuf + recvtype_extent * rank),
                           recvcount, recvtype, tmp_buf, tmp_buf_size, &position);
        }

        curr_cnt = nbytes;

        mask = 0x1;
        i = 0;
        while (mask < comm_size) {
            dst = rank ^ mask;

            /* find offset into send and recv buffers. zero out
             * the least significant "i" bits of rank and dst to
             * find root of src and dst subtrees. Use ranks of
             * roots as index to send from and recv into buffer. */

            dst_tree_root = dst >> i;
            dst_tree_root <<= i;

            my_tree_root = rank >> i;
            my_tree_root <<= i;

            send_offset = my_tree_root * nbytes;
            recv_offset = dst_tree_root * nbytes;

            if (dst < comm_size) {
                mpi_errno =
                    MPIC_Sendrecv(((char *) tmp_buf + send_offset),
                                     curr_cnt, MPI_BYTE, dst,
                                     MPIR_ALLGATHER_TAG,
                                     ((char *) tmp_buf + recv_offset),
                                     tmp_buf_size - recv_offset, MPI_BYTE,
                                     dst, MPIR_ALLGATHER_TAG, comm, &status, errflag);
                if (mpi_errno) {
                    /* for communication errors, just record the error but
                       continue */
                    *errflag = TRUE;
                    MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                    MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                    last_recv_cnt = 0;
                }

                MPIR_Get_count_impl(&status, MPI_BYTE, &last_recv_cnt);
                curr_cnt += last_recv_cnt;
            }

            /* if some processes in this process's subtree in this step
             * did not have any destination process to communicate with
             * because of non-power-of-two, we need to send them the
             * data that they would normally have received from those
             * processes. That is, the haves in this subtree must send to
             * the havenots. We use a logarithmic recursive-halfing
             * algorithm for this. */

            if (dst_tree_root + mask > comm_size) {
                nprocs_completed = comm_size - my_tree_root - mask;
                /* nprocs_completed is the number of processes in this
                 * subtree that have all the data. Send data to others
                 * in a tree fashion. First find root of current tree
                 * that is being divided into two. k is the number of
                 * least-significant bits in this process's rank that
                 * must be zeroed out to find the rank of the root */
                j = mask;
                k = 0;
                while (j) {
                    j >>= 1;
                    k++;
                }
                k--;

                offset = nbytes * (my_tree_root + mask);
                tmp_mask = mask >> 1;

                while (tmp_mask) {
                    dst = rank ^ tmp_mask;
                    tree_root = rank >> k;
                    tree_root <<= k;

                    /* send only if this proc has data and destination
                     * doesn't have data. at any step, multiple processes
                     * can send if they have the data */
                    if ((dst > rank) && (rank < tree_root + nprocs_completed)
                        && (dst >= tree_root + nprocs_completed)) {

                        mpi_errno =
                            MPIC_Send(((char *) tmp_buf + offset),
                                         last_recv_cnt, MPI_BYTE, dst,
                                         MPIR_ALLGATHER_TAG, comm, errflag);
                        /* last_recv_cnt was set in the previous
                         * receive. that's the amount of data to be
                         * sent now. */
                        if (mpi_errno) {
                            /* for communication errors, just record the error
                               but continue */
                            *errflag = TRUE;
                            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                        }
                    }
                    /* recv only if this proc. doesn't have data and sender
                     * has data */
                    else if ((dst < rank) &&
                             (dst < tree_root + nprocs_completed) &&
                             (rank >= tree_root + nprocs_completed)) {
                        mpi_errno = MPIC_Recv(((char *) tmp_buf + offset),
                                              tmp_buf_size - offset,
                                              MPI_BYTE, dst,
                                              MPIR_ALLGATHER_TAG, comm, &status, errflag);
                        /* nprocs_completed is also equal to the
                         * no. of processes whose data we don't have */
                        if (mpi_errno) {
                            /* for communication errors, just record the error
                               but continue */
                            *errflag = TRUE;
                            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
                            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
                            last_recv_cnt = 0;
                        }
                        MPIR_Get_count_impl(&status, MPI_BYTE, &last_recv_cnt);
                        curr_cnt += last_recv_cnt;
                    }
                    tmp_mask >>= 1;
                    k--;
                }
            }
            mask <<= 1;
            i++;
        }

        position = 0;
        MPIR_Unpack_impl(tmp_buf, tmp_buf_size, &position, recvbuf,
                         recvcount * comm_size, recvtype);

        MPIU_Free(tmp_buf);
    }
#endif                          /* MPID_HAS_HETERO */

  fn_fail:
    return (mpi_errno);
}

#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_Bruck_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_Bruck_MV2(const void *sendbuf,
                             int sendcount,
                             MPI_Datatype sendtype,
                             void *recvbuf,
                             int recvcount,
                             MPI_Datatype recvtype, MPID_Comm * comm_ptr,
                             int *errflag)
{

    int comm_size, rank;
    int mpi_errno = MPI_SUCCESS;
    int mpi_errno_ret = MPI_SUCCESS;
    MPI_Aint recvtype_extent;
    MPI_Aint recvtype_true_extent, recvbuf_extent, recvtype_true_lb;
    int src, rem;
    void *tmp_buf;
    int curr_cnt, dst;
    MPI_Comm comm;
    int pof2 = 0;

    comm = comm_ptr->handle;
    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;

    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);

    /* get true extent of recvtype */
    MPIR_Type_get_true_extent_impl(recvtype, &recvtype_true_lb, &recvtype_true_extent);
    recvbuf_extent =
        recvcount * comm_size * (MPIR_MAX(recvtype_true_extent, recvtype_extent));
    /* allocate a temporary buffer of the same size as recvbuf. */
    tmp_buf = MPIU_Malloc(recvbuf_extent);
    /* --BEGIN ERROR HANDLING-- */
    if (!tmp_buf) {
        mpi_errno =
            MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE, FCNAME,
                                 __LINE__, MPI_ERR_OTHER, "**nomem", 0);
        return mpi_errno;
    }
    /* --END ERROR HANDLING-- */

    /* adjust for potential negative lower bound in datatype */
    tmp_buf = (void *) ((char *) tmp_buf - recvtype_true_lb);

    /* copy local data to the top of tmp_buf */
    if (sendbuf != MPI_IN_PLACE) {
        mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                                   tmp_buf, recvcount, recvtype);
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    } else {
        mpi_errno = MPIR_Localcopy(((char *) recvbuf +
                                    rank * recvcount * recvtype_extent),
                                   recvcount, recvtype, tmp_buf, recvcount, recvtype);
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }

    /* do the first \floor(\lg p) steps */

    curr_cnt = recvcount;
    pof2 = 1;
    while (pof2 <= comm_size / 2) {
        src = (rank + pof2) % comm_size;
        dst = (rank - pof2 + comm_size) % comm_size;

        mpi_errno = MPIC_Sendrecv(tmp_buf, curr_cnt, recvtype, dst,
                                     MPIR_ALLGATHER_TAG,
                                     ((char *) tmp_buf +
                                      curr_cnt * recvtype_extent), curr_cnt,
                                     recvtype, src, MPIR_ALLGATHER_TAG,
                                     comm, MPI_STATUS_IGNORE, errflag);
        if (mpi_errno) {
            /* for communication errors, just record the error but continue */
            *errflag = TRUE;
            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
        }
        curr_cnt *= 2;
        pof2 *= 2;
    }

    /* if comm_size is not a power of two, one more step is needed */

    rem = comm_size - pof2;
    if (rem) {
        src = (rank + pof2) % comm_size;
        dst = (rank - pof2 + comm_size) % comm_size;

        mpi_errno = MPIC_Sendrecv(tmp_buf, rem * recvcount, recvtype,
                                     dst, MPIR_ALLGATHER_TAG,
                                     ((char *) tmp_buf +
                                      curr_cnt * recvtype_extent),
                                     rem * recvcount, recvtype, src,
                                     MPIR_ALLGATHER_TAG, comm,
                                     MPI_STATUS_IGNORE, errflag);
        if (mpi_errno) {
            /* for communication errors, just record the error but continue */
            *errflag = TRUE;
            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
        }
    }

    /* Rotate blocks in tmp_buf down by (rank) blocks and store
     * result in recvbuf. */

    mpi_errno = MPIR_Localcopy(tmp_buf, (comm_size - rank) * recvcount,
                               recvtype,
                               (char *) recvbuf +
                               rank * recvcount * recvtype_extent,
                               (comm_size - rank) * recvcount, recvtype);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

    if (rank) {
        mpi_errno = MPIR_Localcopy((char *) tmp_buf +
                                   (comm_size -
                                    rank) * recvcount * recvtype_extent,
                                   rank * recvcount, recvtype, recvbuf,
                                   rank * recvcount, recvtype);
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }

    MPIU_Free((char *) tmp_buf + recvtype_true_lb);

  fn_fail:
    return (mpi_errno);
}

#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_Ring_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_Ring_MV2(const void *sendbuf,
                            int sendcount,
                            MPI_Datatype sendtype,
                            void *recvbuf,
                            int recvcount,
                            MPI_Datatype recvtype, MPID_Comm * comm_ptr,
                            int *errflag)
{

    int comm_size, rank;
    int mpi_errno = MPI_SUCCESS;
    int mpi_errno_ret = MPI_SUCCESS;
    MPI_Aint recvtype_extent;
    int j, i;
    int left, right, jnext;
    MPI_Comm comm;

    comm = comm_ptr->handle;
    comm_size = comm_ptr->local_size;
    rank = comm_ptr->rank;

    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);

    /* First, load the "local" version in the recvbuf. */
    if (sendbuf != MPI_IN_PLACE) {
        mpi_errno = MPIR_Localcopy(sendbuf, sendcount, sendtype,
                                   ((char *) recvbuf +
                                    rank * recvcount * recvtype_extent),
                                   recvcount, recvtype);
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }

    /*
     * Now, send left to right.  This fills in the receive area in
     * reverse order.
     */
    left = (comm_size + rank - 1) % comm_size;
    right = (rank + 1) % comm_size;

    j = rank;
    jnext = left;
    for (i = 1; i < comm_size; i++) {
        mpi_errno = MPIC_Sendrecv(((char *) recvbuf +
                                      j * recvcount * recvtype_extent),
                                     recvcount, recvtype, right,
                                     MPIR_ALLGATHER_TAG,
                                     ((char *) recvbuf +
                                      jnext * recvcount * recvtype_extent),
                                     recvcount, recvtype, left,
                                     MPIR_ALLGATHER_TAG, comm,
                                     MPI_STATUS_IGNORE, errflag);
        if (mpi_errno) {
            /* for communication errors, just record the error but continue */
            *errflag = TRUE;
            MPIU_ERR_SET(mpi_errno, MPI_ERR_OTHER, "**fail");
            MPIU_ERR_ADD(mpi_errno_ret, mpi_errno);
        }
        j = jnext;
        jnext = (comm_size + jnext - 1) % comm_size;
    }

  fn_fail:
    return (mpi_errno);
}


#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_intra_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_intra_MV2(const void *sendbuf,
                             int sendcount,
                             MPI_Datatype sendtype,
                             void *recvbuf,
                             int recvcount,
                             MPI_Datatype recvtype, MPID_Comm * comm_ptr, int *errflag)
{
    int comm_size;
    int mpi_errno = MPI_SUCCESS;
    int type_size;
    int comm_size_is_pof2 = 0;

    if (((sendcount == 0) && (sendbuf != MPI_IN_PLACE)) || (recvcount == 0)) {
        return MPI_SUCCESS;
    }

    comm_size = comm_ptr->local_size;

    MPID_Datatype_get_size_macro(recvtype, type_size);

    /* check if comm_size is a power of two */
    comm_size_is_pof2 = comm_ptr->dev.ch.is_pof2;

    /* check if multiple threads are calling this collective function */
    MPIDU_ERR_CHECK_MULTIPLE_THREADS_ENTER(comm_ptr);

    if ((recvcount * type_size <= allgather_tuning(comm_size, comm_size_is_pof2))
        && (comm_size_is_pof2 == 1)) {
        /* Short or medium size message and power-of-two no. of processes. Use
         * recursive doubling algorithm */
        mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype, recvbuf,
                                              recvcount, recvtype, comm_ptr, errflag);   
                    
    } else if (recvcount * type_size <= allgather_tuning(comm_size, comm_size_is_pof2)) {
        /* Short message and non-power-of-two no. of processes. Use
        * Bruck algorithm (see description above). */
        mpi_errno = MPIR_Allgather_Bruck_MV2(sendbuf, sendcount, sendtype, recvbuf,
                                                 recvcount, recvtype, comm_ptr, errflag);

    } else {                
        /* long message or medium-size message and non-power-of-two
        * no. of processes. use ring algorithm. */
        mpi_errno = MPIR_Allgather_Ring_MV2(sendbuf, sendcount, sendtype, recvbuf,
                                                recvcount, recvtype, comm_ptr, errflag);

    }
    
    /* check if multiple threads are calling this collective function */
    MPIDU_ERR_CHECK_MULTIPLE_THREADS_EXIT(comm_ptr);

    return (mpi_errno);
}

int MPIR_2lvl_Allgather_MV2(const void *sendbuf,int sendcnt, MPI_Datatype sendtype,
                            void *recvbuf, int recvcnt,MPI_Datatype recvtype,
                            MPID_Comm * comm_ptr, int *errflag)
{
    int rank, size;
    int local_rank, local_size;
    int leader_comm_size = 0; 
    int mpi_errno = MPI_SUCCESS;
    MPI_Aint recvtype_extent = 0;  /* Datatype extent */
    MPI_Comm shmem_comm, leader_comm;
    MPID_Comm *shmem_commptr=NULL, *leader_commptr = NULL;

    if (recvcnt == 0) {
        return MPI_SUCCESS;
    }

    rank = comm_ptr->rank;
    size = comm_ptr->local_size; 

    /* extract the rank,size information for the intra-node
     * communicator */
    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);
    
    shmem_comm = comm_ptr->dev.ch.shmem_comm;
    MPID_Comm_get_ptr(shmem_comm, shmem_commptr);
    local_rank = shmem_commptr->rank;
    local_size = shmem_commptr->local_size;

    if (local_rank == 0) {
        /* Node leader. Extract the rank, size information for the leader
         * communicator */
        leader_comm = comm_ptr->dev.ch.leader_comm;
        MPID_Comm_get_ptr(leader_comm, leader_commptr);
        leader_comm_size = leader_commptr->local_size;
    }

    /*If there is just one node, after gather itself,
     * root has all the data and it can do bcast*/
    if(local_rank == 0) {
        mpi_errno = MPIR_Gather_impl(sendbuf, sendcnt,sendtype, 
                                    (void*)((char*)recvbuf + (rank * recvcnt * recvtype_extent)), 
                                     recvcnt, recvtype,
                                     0, shmem_commptr, errflag);
    } else {
        /*Since in allgather all the processes could have 
         * its own data in place*/
        if(sendbuf == MPI_IN_PLACE) {
            mpi_errno = MPIR_Gather_impl((void*)((char*)recvbuf + (rank * recvcnt * recvtype_extent)), 
                                         recvcnt , recvtype, 
                                         recvbuf, recvcnt, recvtype,
                                         0, shmem_commptr, errflag);
        } else {
            mpi_errno = MPIR_Gather_impl(sendbuf, sendcnt,sendtype, 
                                         recvbuf, recvcnt, recvtype,
                                         0, shmem_commptr, errflag);
        }
    }

    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

    /* Exchange the data between the node leaders*/
    if (local_rank == 0 && (leader_comm_size > 1)) {
        /*When data in each socket is different*/
        if (comm_ptr->dev.ch.is_uniform != 1) {

            int *displs = NULL;
            int *recvcnts = NULL;
            int *node_sizes;
            int i = 0;

            node_sizes = comm_ptr->dev.ch.node_sizes;

            displs = MPIU_Malloc(sizeof (int) * leader_comm_size);
            recvcnts = MPIU_Malloc(sizeof (int) * leader_comm_size);
            if (!displs || !recvcnts) {
                mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                        MPIR_ERR_RECOVERABLE,
                        FCNAME, __LINE__,
                        MPI_ERR_OTHER,
                        "**nomem", 0);
                return mpi_errno;
            }
            recvcnts[0] = node_sizes[0] * recvcnt;
            displs[0] = 0;

            for (i = 1; i < leader_comm_size; i++) {
                displs[i] = displs[i - 1] + node_sizes[i - 1] * recvcnt;
                recvcnts[i] = node_sizes[i] * recvcnt;
            }


            mpi_errno = MPIR_Allgatherv(MPI_IN_PLACE,
                                       (recvcnt*local_size),
                                       recvtype, 
                                       recvbuf, recvcnts,
                                       displs, recvtype,
                                       leader_commptr, errflag);
            MPIU_Free(displs);
            MPIU_Free(recvcnts);
        } else {
            mpi_errno = MV2_Allgather_function(MPI_IN_PLACE, 
                                               (recvcnt*local_size),
                                               recvtype,
                                               recvbuf, (recvcnt*local_size), recvtype,
                                             leader_commptr, errflag);

        }

        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }

    /*Bcast the entire data from node leaders to all other cores*/
    mpi_errno = MPIR_Bcast_impl (recvbuf, recvcnt * size, recvtype, 0, shmem_commptr, errflag);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }
  
  fn_fail:
    return (mpi_errno);
}
/* end:nested */

#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_index_tuned_intra_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_index_tuned_intra_MV2(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                       void *recvbuf, int recvcount, MPI_Datatype recvtype,
                       MPID_Comm * comm_ptr, int *errflag)
{

    int mpi_errno = MPI_SUCCESS;
    int nbytes = 0, comm_size, recvtype_size;
    int comm_size_index = 0;
    int inter_node_algo_index = 0;
    int local_size = 0;
    int partial_sub_ok = 0;
    int conf_index = 0;
    int table_min_comm_size = 0;
    int table_max_comm_size = 0;
    int table_min_inter_size = 0;
    int table_max_inter_size = 0;
    int last_inter;
    int lp2ltn; // largest power of 2 less than n
    MPI_Comm shmem_comm;
    MPID_Comm *shmem_commptr=NULL;

    /* Get the size of the communicator */
    comm_size = comm_ptr->local_size;

    MPID_Datatype_get_size_macro(recvtype, recvtype_size);
    nbytes = recvtype_size * recvcount;

    int i, rank;
    MPI_Aint recvtype_extent;
    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);
    mpi_errno = PMPI_Comm_rank(comm_ptr->handle, &rank);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }
#ifdef _ENABLE_CUDA_
    int send_mem_type = 0;
    int recv_mem_type = 0;
    int snbytes = INT_MAX;
    MPI_Aint sendtype_extent;
    if (rdma_enable_cuda) {
        send_mem_type = is_device_buffer(sendbuf);
        recv_mem_type = is_device_buffer(recvbuf);
    }

    /*Handling Non-contig datatypes */
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type)) {
        cuda_coll_pack((void **)&sendbuf, &sendcount, &sendtype,
                       &recvbuf, &recvcount, &recvtype,
                       rank * recvcount * recvtype_extent, 1, comm_size);
    }

    MPID_Datatype_get_extent_macro(sendtype, sendtype_extent);
    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);
    if (sendbuf != MPI_IN_PLACE) {
        snbytes = sendtype_extent * sendcount;
    }
    MPID_Datatype_get_size_macro(recvtype, recvtype_size);
    nbytes = recvtype_size * recvcount;

    if (rdma_enable_cuda && rdma_cuda_allgather_fgp &&
        send_mem_type && recv_mem_type &&
        snbytes >
        rdma_cuda_allgather_naive_limit / (FGP_SWITCH_FACTOR * comm_size) &&
        nbytes > rdma_cuda_allgather_naive_limit / (FGP_SWITCH_FACTOR * comm_size)) {
        if (sendbuf != MPI_IN_PLACE) {
            mpi_errno =
                MPIR_Allgather_cuda_intra_MV2(sendbuf, sendcount, sendtype,
                                              recvbuf, recvcount, recvtype,
                                              comm_ptr, errflag);
        } else {
            mpi_errno =
                MPIR_Allgather_cuda_intra_MV2(recvbuf +
                                              rank * recvcount *
                                              recvtype_extent, recvcount,
                                              recvtype, recvbuf, recvcount,
                                              recvtype, comm_ptr, errflag);
        }
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
        goto fn_exit;
    } else if (rdma_enable_cuda && (send_mem_type || recv_mem_type) &&
               rdma_cuda_use_naive && (nbytes <= rdma_cuda_allgather_naive_limit)) {
        if (sendbuf != MPI_IN_PLACE) {
            mpi_errno = cuda_stage_alloc((void **)&sendbuf, sendcount * sendtype_extent,
                                         &recvbuf,
                                         recvcount * recvtype_extent *
                                         comm_size, send_mem_type, recv_mem_type, 0);
        } else {
            mpi_errno = cuda_stage_alloc((void **)&sendbuf, recvcount * recvtype_extent,
                                         &recvbuf,
                                         recvcount * recvtype_extent *
                                         comm_size, send_mem_type,
                                         recv_mem_type,
                                         rank * recvcount * recvtype_extent);
        }
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */

    if (mv2_use_old_allgather == 1) {
	MPIR_Allgather_intra_MV2(sendbuf, sendcount, sendtype, recvbuf, recvcount,
				 recvtype, comm_ptr, errflag);
	goto fn_exit;
    }
    
    /* check if safe to use partial subscription mode */
    if (comm_ptr->dev.ch.shmem_coll_ok == 1 && comm_ptr->dev.ch.is_uniform) {
    
        shmem_comm = comm_ptr->dev.ch.shmem_comm;
        MPID_Comm_get_ptr(shmem_comm, shmem_commptr);
        local_size = shmem_commptr->local_size;
        i = 0;
        if (mv2_allgather_indexed_table_ppn_conf[0] == -1) {
            // Indicating user defined tuning
            conf_index = 0;
            goto conf_check_end;
        }
        do {
            if (local_size == mv2_allgather_indexed_table_ppn_conf[i]) {
                conf_index = i;
                partial_sub_ok = 1;
                break;
            }
            i++;
        } while(i < mv2_allgather_indexed_num_ppn_conf);
    }

  conf_check_end:
    if (partial_sub_ok != 1) {
        conf_index = 0;
    }
        
    /* Search for the corresponding system size inside the tuning table */
    /*
     * Comm sizes progress in powers of 2. Therefore comm_size can just be indexed instead
     */
    table_min_comm_size = mv2_allgather_indexed_thresholds_table[conf_index][0].numproc;
    table_max_comm_size =
	mv2_allgather_indexed_thresholds_table[conf_index][mv2_size_allgather_indexed_tuning_table[conf_index] - 1].numproc;
    
    if (comm_size < table_min_comm_size) {
	/* Comm size smaller than smallest configuration in table: use smallest available */
	comm_size_index = 0;
    }
    else if (comm_size > table_max_comm_size) {
	/* Comm size larger than largest configuration in table: use largest available */
	comm_size_index = mv2_size_allgather_indexed_tuning_table[conf_index] - 1;
    }
    else {
	/* Comm size in between smallest and largest configuration: find closest match */
	if (comm_ptr->dev.ch.is_pof2) {
	    comm_size_index = log2( comm_size / table_min_comm_size );
	}
	else {
	    lp2ltn = pow(2, (int)log2(comm_size));
	    comm_size_index = (lp2ltn < table_min_comm_size) ? 0 : log2( lp2ltn / table_min_comm_size );
	}
    }

    last_inter = mv2_allgather_indexed_thresholds_table[conf_index][comm_size_index].size_inter_table - 1;
    table_min_inter_size = mv2_allgather_indexed_thresholds_table[conf_index][comm_size_index].inter_leader[0].msg_sz;
    table_max_inter_size = mv2_allgather_indexed_thresholds_table[conf_index][comm_size_index].inter_leader[last_inter].msg_sz;
    
    if (nbytes < table_min_inter_size) {
	/* Msg size smaller than smallest configuration in table: use smallest available */
	inter_node_algo_index = 0;
    }
    else if (nbytes > table_max_inter_size) {
	/* Msg size larger than largest configuration in table: use largest available */
	inter_node_algo_index = last_inter;
    }
    else {
	/* Msg size in between smallest and largest configuration: find closest match */
	if (pow(2, (int)log2(nbytes)) == nbytes) {
	    inter_node_algo_index = log2( nbytes / table_min_inter_size );
	}
	else {
	    lp2ltn = pow(2, (int)log2(nbytes));
	    inter_node_algo_index = (lp2ltn < table_min_inter_size) ? 0 : log2( lp2ltn / table_min_inter_size );
	}
    }

    /* Set inter-leader pt */
    MV2_Allgather_function =
                          mv2_allgather_indexed_thresholds_table[conf_index][comm_size_index].
	inter_leader[inter_node_algo_index].MV2_pt_Allgather_function;

    if(MV2_Allgather_function == &MPIR_Allgather_RD_Allgather_Comm_MV2) {
        if(comm_ptr->dev.ch.allgather_comm_ok == 1) {
            int sendtype_iscontig = 0, recvtype_iscontig = 0;
            void *tmp_recv_buf = NULL;
            MPIR_T_PVAR_COUNTER_INC(MV2, mv2_num_shmem_coll_calls, 1);
            if (sendtype != MPI_DATATYPE_NULL && recvtype != MPI_DATATYPE_NULL) {
                MPIR_Datatype_iscontig(sendtype, &sendtype_iscontig);
                MPIR_Datatype_iscontig(recvtype, &recvtype_iscontig);
            }

            MPID_Comm *allgather_comm_ptr;
            MPID_Comm_get_ptr(comm_ptr->dev.ch.allgather_comm, allgather_comm_ptr);

            /*creation of a temporary recvbuf */
            tmp_recv_buf = MPIU_Malloc(recvcount * comm_size * recvtype_extent);
            if (!tmp_recv_buf) {
                mpi_errno = MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                                 FCNAME, __LINE__, MPI_ERR_OTHER,
                                                 "**nomem", 0);
                return mpi_errno;
            }
            /* Calling Allgather with temporary buffer and allgather communicator */
            if (sendbuf != MPI_IN_PLACE) {
                mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype,
                                                     tmp_recv_buf, recvcount,
                                                     recvtype, allgather_comm_ptr, errflag);
            } else {
                mpi_errno = MPIR_Allgather_RD_MV2(recvbuf + rank * recvcount *
                                                     recvtype_extent, recvcount,
                                                     recvtype, tmp_recv_buf,
                                                     recvcount, recvtype,
                                                     allgather_comm_ptr, errflag);
            }

            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }
            /* Reordering data into recvbuf */
            if (sendtype_iscontig == 1 && recvtype_iscontig == 1
#if defined(_ENABLE_CUDA_)
                && rdma_enable_cuda == 0
#endif
            ){
                for (i = 0; i < comm_size; i++) {
                    MPIUI_Memcpy((void *) ((char *) recvbuf +
                                           (comm_ptr->dev.ch.allgather_new_ranks[i]) *
                                           nbytes),
                                           (char *) tmp_recv_buf + i * nbytes, nbytes);
                }
            } else {
                for (i = 0; i < comm_size; i++) {
                    mpi_errno = MPIR_Localcopy((void *) ((char *) tmp_recv_buf +
                                                i * recvcount *
                                                recvtype_extent),
                                                recvcount, recvtype,
                                                (void *) ((char *) recvbuf +
                                                (comm_ptr->dev.ch.allgather_new_ranks[i])
                                                * recvcount * recvtype_extent),
                                           recvcount, recvtype);
                    if (mpi_errno) {
                        MPIU_ERR_POP(mpi_errno);
                    }
                }
            }
            MPIU_Free(tmp_recv_buf);
        } else {
            mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype,
                                                recvbuf, recvcount, recvtype,
                                                comm_ptr, errflag);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }
        } 
    } else if(MV2_Allgather_function == &MPIR_Allgather_Bruck_MV2 
            || MV2_Allgather_function == &MPIR_Allgather_RD_MV2
            || MV2_Allgather_function == &MPIR_Allgather_Ring_MV2) {
            mpi_errno = MV2_Allgather_function(sendbuf, sendcount, sendtype,
                                          recvbuf, recvcount, recvtype,
                                          comm_ptr, errflag);
    } else {
        mpi_errno = MPIR_Allgather_intra(sendbuf, sendcount, sendtype,
                                         recvbuf, recvcount, recvtype, comm_ptr, errflag);
    }

#ifdef _ENABLE_CUDA_
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type) &&
        rdma_cuda_use_naive && (nbytes <= rdma_cuda_allgather_naive_limit)) {
        cuda_stage_free((void **)&sendbuf,
                        &recvbuf, recvcount * recvtype_extent * comm_size,
                        send_mem_type, recv_mem_type);
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */

    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

  fn_exit:
#ifdef _ENABLE_CUDA_
    /*Handling Non-Contig datatypes */
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type)) {
        cuda_coll_unpack(&recvcount, comm_size);
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}


#undef FUNCNAME
#define FUNCNAME MPIR_Allgather_MV2
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)
int MPIR_Allgather_MV2(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                       void *recvbuf, int recvcount, MPI_Datatype recvtype,
                       MPID_Comm * comm_ptr, int *errflag)
{

    int mpi_errno = MPI_SUCCESS;
    int nbytes = 0, comm_size, recvtype_size;
    int range = 0;
    int partial_sub_ok = 0;
    int conf_index = 0;
    int range_threshold = 0;
    int is_two_level = 0;
    int local_size = -1;
    MPI_Comm shmem_comm;
    MPID_Comm *shmem_commptr=NULL;
    
    if (mv2_use_indexed_tuning || mv2_use_indexed_allgather_tuning) {
	mpi_errno = MPIR_Allgather_index_tuned_intra_MV2(sendbuf, sendcount, sendtype, recvbuf, recvcount,
				 recvtype, comm_ptr, errflag);
	goto fn_exit;
    }

    /* Get the size of the communicator */
    comm_size = comm_ptr->local_size;

    MPID_Datatype_get_size_macro(recvtype, recvtype_size);
    nbytes = recvtype_size * recvcount;

    int i, rank;
    MPI_Aint recvtype_extent;
    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);
    mpi_errno = PMPI_Comm_rank(comm_ptr->handle, &rank);
    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }
#ifdef _ENABLE_CUDA_
    int send_mem_type = 0;
    int recv_mem_type = 0;
    int snbytes = INT_MAX;
    MPI_Aint sendtype_extent;
    if (rdma_enable_cuda) {
        send_mem_type = is_device_buffer(sendbuf);
        recv_mem_type = is_device_buffer(recvbuf);
    }

    /*Handling Non-contig datatypes */
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type)) {
        cuda_coll_pack((void **)&sendbuf, &sendcount, &sendtype,
                       &recvbuf, &recvcount, &recvtype,
                       rank * recvcount * recvtype_extent, 1, comm_size);
    }

    MPID_Datatype_get_extent_macro(sendtype, sendtype_extent);
    MPID_Datatype_get_extent_macro(recvtype, recvtype_extent);
    if (sendbuf != MPI_IN_PLACE) {
        snbytes = sendtype_extent * sendcount;
    }
    MPID_Datatype_get_size_macro(recvtype, recvtype_size);
    nbytes = recvtype_size * recvcount;

    if (rdma_enable_cuda && rdma_cuda_allgather_fgp &&
        send_mem_type && recv_mem_type &&
        snbytes >
        rdma_cuda_allgather_naive_limit / (FGP_SWITCH_FACTOR * comm_size) &&
        nbytes > rdma_cuda_allgather_naive_limit / (FGP_SWITCH_FACTOR * comm_size)) {
        if (sendbuf != MPI_IN_PLACE) {
            mpi_errno =
                MPIR_Allgather_cuda_intra_MV2(sendbuf, sendcount, sendtype,
                                              recvbuf, recvcount, recvtype,
                                              comm_ptr, errflag);
        } else {
            mpi_errno =
                MPIR_Allgather_cuda_intra_MV2(recvbuf +
                                              rank * recvcount *
                                              recvtype_extent, recvcount,
                                              recvtype, recvbuf, recvcount,
                                              recvtype, comm_ptr, errflag);
        }
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
        goto fn_exit;
    } else if (rdma_enable_cuda && (send_mem_type || recv_mem_type) &&
               rdma_cuda_use_naive && (nbytes <= rdma_cuda_allgather_naive_limit)) {
        if (sendbuf != MPI_IN_PLACE) {
            mpi_errno = cuda_stage_alloc((void **)&sendbuf, sendcount * sendtype_extent,
                                         &recvbuf,
                                         recvcount * recvtype_extent *
                                         comm_size, send_mem_type, recv_mem_type, 0);
        } else {
            mpi_errno = cuda_stage_alloc((void **)&sendbuf, recvcount * recvtype_extent,
                                         &recvbuf,
                                         recvcount * recvtype_extent *
                                         comm_size, send_mem_type,
                                         recv_mem_type,
                                         rank * recvcount * recvtype_extent);
        }
        if (mpi_errno) {
            MPIU_ERR_POP(mpi_errno);
        }
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */

    if (mv2_use_old_allgather == 1) {
	MPIR_Allgather_intra_MV2(sendbuf, sendcount, sendtype, recvbuf, recvcount,
				 recvtype, comm_ptr, errflag);
	goto fn_exit;
    }
    
    /* check if safe to use partial subscription mode */
    if (comm_ptr->dev.ch.shmem_coll_ok == 1 && comm_ptr->dev.ch.is_uniform) {
    
        shmem_comm = comm_ptr->dev.ch.shmem_comm;
        MPID_Comm_get_ptr(shmem_comm, shmem_commptr);
        local_size = shmem_commptr->local_size;
        i = 0;
        if (mv2_allgather_table_ppn_conf[0] == -1) {
            // Indicating user defined tuning
            conf_index = 0;
            goto conf_check_end;
        }
        do {
            if (local_size == mv2_allgather_table_ppn_conf[i]) {
                conf_index = i;
                partial_sub_ok = 1;
                break;
            }
            i++;
        } while(i < mv2_allgather_num_ppn_conf);
    }

  conf_check_end:
    if (partial_sub_ok != 1) {
        conf_index = 0;
    }
    /* Search for the corresponding system size inside the tuning table */
    while ((range < (mv2_size_allgather_tuning_table[conf_index] - 1)) &&
           (comm_size >
            mv2_allgather_thresholds_table[conf_index][range].numproc)) {
        range++;
    }
    /* Search for corresponding inter-leader function */
    while ((range_threshold <
         (mv2_allgather_thresholds_table[conf_index][range].size_inter_table - 1))
           && (nbytes > mv2_allgather_thresholds_table[conf_index][range].inter_leader[range_threshold].max)
           && (mv2_allgather_thresholds_table[conf_index][range].inter_leader[range_threshold].max !=
               -1)) {
        range_threshold++;
    }

    /* Set inter-leader pt */
    MV2_Allgather_function =
                          mv2_allgather_thresholds_table[conf_index][range].inter_leader[range_threshold].
                          MV2_pt_Allgather_function;

    is_two_level =  mv2_allgather_thresholds_table[conf_index][range].two_level[range_threshold];

    /* intracommunicator */
    if(is_two_level ==1){
        
        if(comm_ptr->dev.ch.shmem_coll_ok == 1){
            MPIR_T_PVAR_COUNTER_INC(MV2, mv2_num_shmem_coll_calls, 1);
	   if (1 == comm_ptr->dev.ch.is_blocked) {
                mpi_errno = MPIR_2lvl_Allgather_MV2(sendbuf, sendcount, sendtype,
						    recvbuf, recvcount, recvtype,
						    comm_ptr, errflag);
	   }
	   else {
	       mpi_errno = MPIR_Allgather_intra(sendbuf, sendcount, sendtype,
						recvbuf, recvcount, recvtype,
						comm_ptr, errflag);
	   }
        } else {
            mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype,
                                                recvbuf, recvcount, recvtype,
                                                comm_ptr, errflag);
        }
    } else if(MV2_Allgather_function == &MPIR_Allgather_RD_Allgather_Comm_MV2){
        if(comm_ptr->dev.ch.allgather_comm_ok == 1) {
            int sendtype_iscontig = 0, recvtype_iscontig = 0;
            void *tmp_recv_buf = NULL;
            MPIR_T_PVAR_COUNTER_INC(MV2, mv2_num_shmem_coll_calls, 1);
            if (sendtype != MPI_DATATYPE_NULL && recvtype != MPI_DATATYPE_NULL) {
                MPIR_Datatype_iscontig(sendtype, &sendtype_iscontig);
                MPIR_Datatype_iscontig(recvtype, &recvtype_iscontig);
            }

            MPID_Comm *allgather_comm_ptr;
            MPID_Comm_get_ptr(comm_ptr->dev.ch.allgather_comm, allgather_comm_ptr);

            /*creation of a temporary recvbuf */
            tmp_recv_buf = MPIU_Malloc(recvcount * comm_size * recvtype_extent);
            if (!tmp_recv_buf) {
                mpi_errno = MPIR_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
                                                 FCNAME, __LINE__, MPI_ERR_OTHER,
                                                 "**nomem", 0);
                return mpi_errno;
            }
            /* Calling Allgather with temporary buffer and allgather communicator */
            if (sendbuf != MPI_IN_PLACE) {
                mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype,
                                                     tmp_recv_buf, recvcount,
                                                     recvtype, allgather_comm_ptr, errflag);
            } else {
                mpi_errno = MPIR_Allgather_RD_MV2(recvbuf + rank * recvcount *
                                                     recvtype_extent, recvcount,
                                                     recvtype, tmp_recv_buf,
                                                     recvcount, recvtype,
                                                     allgather_comm_ptr, errflag);
            }

            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }
            /* Reordering data into recvbuf */
            if (sendtype_iscontig == 1 && recvtype_iscontig == 1
#if defined(_ENABLE_CUDA_)
                && rdma_enable_cuda == 0
#endif
            ){
                for (i = 0; i < comm_size; i++) {
                    MPIUI_Memcpy((void *) ((char *) recvbuf +
                                           (comm_ptr->dev.ch.allgather_new_ranks[i]) *
                                           nbytes),
                                           (char *) tmp_recv_buf + i * nbytes, nbytes);
                }
            } else {
                for (i = 0; i < comm_size; i++) {
                    mpi_errno = MPIR_Localcopy((void *) ((char *) tmp_recv_buf +
                                                i * recvcount *
                                                recvtype_extent),
                                                recvcount, recvtype,
                                                (void *) ((char *) recvbuf +
                                                (comm_ptr->dev.ch.allgather_new_ranks[i])
                                                * recvcount * recvtype_extent),
                                           recvcount, recvtype);
                    if (mpi_errno) {
                        MPIU_ERR_POP(mpi_errno);
                    }
                }
            }
            MPIU_Free(tmp_recv_buf);
        } else {
            mpi_errno = MPIR_Allgather_RD_MV2(sendbuf, sendcount, sendtype,
                                                recvbuf, recvcount, recvtype,
                                                comm_ptr, errflag);
            if (mpi_errno) {
                MPIU_ERR_POP(mpi_errno);
            }
        } 
    } else if(MV2_Allgather_function == &MPIR_Allgather_Bruck_MV2 
            || MV2_Allgather_function == &MPIR_Allgather_RD_MV2
            || MV2_Allgather_function == &MPIR_Allgather_Ring_MV2) {
            mpi_errno = MV2_Allgather_function(sendbuf, sendcount, sendtype,
                                          recvbuf, recvcount, recvtype,
                                          comm_ptr, errflag);
    } else {
        mpi_errno = MPIR_Allgather_intra(sendbuf, sendcount, sendtype,
                                         recvbuf, recvcount, recvtype, comm_ptr, errflag);
    }

#ifdef _ENABLE_CUDA_
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type) &&
        rdma_cuda_use_naive && (nbytes <= rdma_cuda_allgather_naive_limit)) {
        cuda_stage_free((void **)&sendbuf,
                        &recvbuf, recvcount * recvtype_extent * comm_size,
                        send_mem_type, recv_mem_type);
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */

    if (mpi_errno) {
        MPIU_ERR_POP(mpi_errno);
    }

  fn_exit:
#ifdef _ENABLE_CUDA_
    /*Handling Non-Contig datatypes */
    if (rdma_enable_cuda && (send_mem_type || recv_mem_type)) {
        cuda_coll_unpack(&recvcount, comm_size);
    }
#endif                          /*#ifdef _ENABLE_CUDA_ */
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}
