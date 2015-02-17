/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*  
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 * This file is automatically generated by buildiface 
 * DO NOT EDIT
 */
#include "mpi_fortimpl.h"


/* Begin MPI profiling block */
#if defined(USE_WEAK_SYMBOLS) && !defined(USE_ONLY_MPI_NAMES) 
#if defined(HAVE_MULTIPLE_PRAGMA_WEAK)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#if defined(F77_NAME_UPPER)
#pragma weak MPI_IALLTOALL = PMPI_IALLTOALL
#pragma weak mpi_ialltoall__ = PMPI_IALLTOALL
#pragma weak mpi_ialltoall_ = PMPI_IALLTOALL
#pragma weak mpi_ialltoall = PMPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak MPI_IALLTOALL = pmpi_ialltoall__
#pragma weak mpi_ialltoall__ = pmpi_ialltoall__
#pragma weak mpi_ialltoall_ = pmpi_ialltoall__
#pragma weak mpi_ialltoall = pmpi_ialltoall__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak MPI_IALLTOALL = pmpi_ialltoall_
#pragma weak mpi_ialltoall__ = pmpi_ialltoall_
#pragma weak mpi_ialltoall_ = pmpi_ialltoall_
#pragma weak mpi_ialltoall = pmpi_ialltoall_
#else
#pragma weak MPI_IALLTOALL = pmpi_ialltoall
#pragma weak mpi_ialltoall__ = pmpi_ialltoall
#pragma weak mpi_ialltoall_ = pmpi_ialltoall
#pragma weak mpi_ialltoall = pmpi_ialltoall
#endif



#elif defined(HAVE_PRAGMA_WEAK)

#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak MPI_IALLTOALL = PMPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_ialltoall__ = pmpi_ialltoall__
#elif !defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_ialltoall = pmpi_ialltoall
#else
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_ialltoall_ = pmpi_ialltoall_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(F77_NAME_UPPER)
#pragma _HP_SECONDARY_DEF PMPI_IALLTOALL  MPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma _HP_SECONDARY_DEF pmpi_ialltoall__  mpi_ialltoall__
#elif !defined(F77_NAME_LOWER_USCORE)
#pragma _HP_SECONDARY_DEF pmpi_ialltoall  mpi_ialltoall
#else
#pragma _HP_SECONDARY_DEF pmpi_ialltoall_  mpi_ialltoall_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(F77_NAME_UPPER)
#pragma _CRI duplicate MPI_IALLTOALL as PMPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma _CRI duplicate mpi_ialltoall__ as pmpi_ialltoall__
#elif !defined(F77_NAME_LOWER_USCORE)
#pragma _CRI duplicate mpi_ialltoall as pmpi_ialltoall
#else
#pragma _CRI duplicate mpi_ialltoall_ as pmpi_ialltoall_
#endif

#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));

#else
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));

#endif
#endif /* HAVE_PRAGMA_WEAK */
#endif /* USE_WEAK_SYMBOLS */
/* End MPI profiling block */


/* These definitions are used only for generating the Fortran wrappers */
#if defined(USE_WEAK_SYMBOLS) && defined(USE_ONLY_MPI_NAMES)
#if defined(HAVE_MULTIPLE_PRAGMA_WEAK)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#if defined(F77_NAME_UPPER)
#pragma weak mpi_ialltoall__ = MPI_IALLTOALL
#pragma weak mpi_ialltoall_ = MPI_IALLTOALL
#pragma weak mpi_ialltoall = MPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak MPI_IALLTOALL = mpi_ialltoall__
#pragma weak mpi_ialltoall_ = mpi_ialltoall__
#pragma weak mpi_ialltoall = mpi_ialltoall__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak MPI_IALLTOALL = mpi_ialltoall_
#pragma weak mpi_ialltoall__ = mpi_ialltoall_
#pragma weak mpi_ialltoall = mpi_ialltoall_
#else
#pragma weak MPI_IALLTOALL = mpi_ialltoall
#pragma weak mpi_ialltoall__ = mpi_ialltoall
#pragma weak mpi_ialltoall_ = mpi_ialltoall
#endif
#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_IALLTOALL")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall_")));

#else
extern FORT_DLL_SPEC void FORT_CALL MPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL mpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#endif
#endif

#endif

/* Map the name to the correct form */
#ifndef MPICH_MPI_FROM_PMPI
#if defined(USE_WEAK_SYMBOLS)
#if defined(HAVE_MULTIPLE_PRAGMA_WEAK)
/* Define the weak versions of the PMPI routine*/
#ifndef F77_NAME_UPPER
extern FORT_DLL_SPEC void FORT_CALL PMPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER_2USCORE
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER_USCORE
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#endif

#if defined(F77_NAME_UPPER)
#pragma weak pmpi_ialltoall__ = PMPI_IALLTOALL
#pragma weak pmpi_ialltoall_ = PMPI_IALLTOALL
#pragma weak pmpi_ialltoall = PMPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak PMPI_IALLTOALL = pmpi_ialltoall__
#pragma weak pmpi_ialltoall_ = pmpi_ialltoall__
#pragma weak pmpi_ialltoall = pmpi_ialltoall__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak PMPI_IALLTOALL = pmpi_ialltoall_
#pragma weak pmpi_ialltoall__ = pmpi_ialltoall_
#pragma weak pmpi_ialltoall = pmpi_ialltoall_
#else
#pragma weak PMPI_IALLTOALL = pmpi_ialltoall
#pragma weak pmpi_ialltoall__ = pmpi_ialltoall
#pragma weak pmpi_ialltoall_ = pmpi_ialltoall
#endif /* Test on name mapping */

#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_IALLTOALL")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL PMPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL PMPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall_")));

#else
extern FORT_DLL_SPEC void FORT_CALL PMPI_IALLTOALL( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall__( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_ialltoall_( void*, MPI_Fint *, MPI_Fint *, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_ialltoall")));

#endif /* Test on name mapping */
#endif /* HAVE_MULTIPLE_PRAGMA_WEAK */
#endif /* USE_WEAK_SYMBOLS */

#ifdef F77_NAME_UPPER
#define mpi_ialltoall_ PMPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#define mpi_ialltoall_ pmpi_ialltoall__
#elif !defined(F77_NAME_LOWER_USCORE)
#define mpi_ialltoall_ pmpi_ialltoall
#else
#define mpi_ialltoall_ pmpi_ialltoall_
#endif /* Test on name mapping */

/* This defines the routine that we call, which must be the PMPI version
   since we're renaming the Fortran entry as the pmpi version.  The MPI name
   must be undefined first to prevent any conflicts with previous renamings. */
#undef MPI_Ialltoall
#define MPI_Ialltoall PMPI_Ialltoall 

#else

#ifdef F77_NAME_UPPER
#define mpi_ialltoall_ MPI_IALLTOALL
#elif defined(F77_NAME_LOWER_2USCORE)
#define mpi_ialltoall_ mpi_ialltoall__
#elif !defined(F77_NAME_LOWER_USCORE)
#define mpi_ialltoall_ mpi_ialltoall
/* Else leave name alone */
#endif


#endif /* MPICH_MPI_FROM_PMPI */

/* Prototypes for the Fortran interfaces */
#include "fproto.h"
FORT_DLL_SPEC void FORT_CALL mpi_ialltoall_ ( void*v1, MPI_Fint *v2, MPI_Fint *v3, void*v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *v7, MPI_Fint *v8, MPI_Fint *ierr ){

#ifndef HAVE_MPI_F_INIT_WORKS_WITH_C
    if (MPIR_F_NeedInit){ mpirinitf_(); MPIR_F_NeedInit = 0; }
#endif
    if (v1 == MPIR_F_MPI_IN_PLACE) v1 = MPI_IN_PLACE;
    *ierr = MPI_Ialltoall( v1, (int)*v2, (MPI_Datatype)(*v3), v4, (int)*v5, (MPI_Datatype)(*v6), (MPI_Comm)(*v7), (MPI_Request *)(v8) );
}
