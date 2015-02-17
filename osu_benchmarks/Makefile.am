 #
 # Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 # (NBCL), The Ohio State University. 
 #
 # Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 #
 # For detailed copyright and licensing information, please refer to the
 # copyright file COPYRIGHT in the top level OMB directory.
 #

pkglibexec_PROGRAMS =		\
    osu_alltoallv		\
    osu_allgatherv		\
    osu_scatterv		\
    osu_gatherv			\
    osu_reduce_scatter		\
    osu_allgather		\
    osu_barrier			\
    osu_reduce			\
    osu_allreduce		\
    osu_gather			\
    osu_scatter			\
    osu_alltoall		\
    osu_bcast			\
    osu_bibw			\
    osu_bw			\
    osu_latency			\
    osu_mbw_mr			\
    osu_multi_lat		\
    osu_acc_latency		\
	osu_get_acc_latency \
	osu_cas_latency     \
	osu_fop_latency     \
    osu_get_bw			\
    osu_get_latency		\
    osu_latency_mt		\
    osu_put_bibw		\
    osu_put_bw			\
    osu_put_latency		

osu_scatter_SOURCES = osu_scatter.c osu_coll.c osu_coll.h
osu_scatterv_SOURCES = osu_scatterv.c osu_coll.c osu_coll.h
osu_gather_SOURCES = osu_gather.c osu_coll.c osu_coll.h
osu_gatherv_SOURCES = osu_gatherv.c osu_coll.c osu_coll.h
osu_alltoall_SOURCES = osu_alltoall.c osu_coll.c osu_coll.h
osu_alltoallv_SOURCES = osu_alltoallv.c osu_coll.c osu_coll.h
osu_allgather_SOURCES = osu_allgather.c osu_coll.c osu_coll.h
osu_allgatherv_SOURCES = osu_allgatherv.c osu_coll.c osu_coll.h
osu_reduce_SOURCES = osu_reduce.c osu_coll.c osu_coll.h
osu_allreduce_SOURCES = osu_allreduce.c osu_coll.c osu_coll.h
osu_reduce_scatter_SOURCES = osu_reduce_scatter.c osu_coll.c osu_coll.h
osu_bcast_SOURCES = osu_bcast.c osu_coll.c osu_coll.h

AM_CPPFLAGS =

if BUILD_MRAIL_CUDA
dist_pkglibexec_SCRIPTS = get_local_rank
AM_CPPFLAGS += -D_ENABLE_CUDA_
endif

if BUILD_MRAIL_OPENACC
AM_CPPFLAGS += -D_ENABLE_OPENACC_
AM_CFLAGS = -acc
endif

EXTRA_DIST = README CHANGES

if BUILD_PROFILING_LIB
AM_LDFLAGS = ../lib/lib@PMPILIBNAME@.la ../lib/lib@MPILIBNAME@.la
else 
AM_LDFLAGS = ../lib/lib@MPILIBNAME@.la
endif

