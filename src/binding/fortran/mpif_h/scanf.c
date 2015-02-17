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
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#if defined(F77_NAME_UPPER)
#pragma weak MPI_SCAN = PMPI_SCAN
#pragma weak mpi_scan__ = PMPI_SCAN
#pragma weak mpi_scan_ = PMPI_SCAN
#pragma weak mpi_scan = PMPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak MPI_SCAN = pmpi_scan__
#pragma weak mpi_scan__ = pmpi_scan__
#pragma weak mpi_scan_ = pmpi_scan__
#pragma weak mpi_scan = pmpi_scan__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak MPI_SCAN = pmpi_scan_
#pragma weak mpi_scan__ = pmpi_scan_
#pragma weak mpi_scan_ = pmpi_scan_
#pragma weak mpi_scan = pmpi_scan_
#else
#pragma weak MPI_SCAN = pmpi_scan
#pragma weak mpi_scan__ = pmpi_scan
#pragma weak mpi_scan_ = pmpi_scan
#pragma weak mpi_scan = pmpi_scan
#endif



#elif defined(HAVE_PRAGMA_WEAK)

#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak MPI_SCAN = PMPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_scan__ = pmpi_scan__
#elif !defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_scan = pmpi_scan
#else
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#pragma weak mpi_scan_ = pmpi_scan_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(F77_NAME_UPPER)
#pragma _HP_SECONDARY_DEF PMPI_SCAN  MPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma _HP_SECONDARY_DEF pmpi_scan__  mpi_scan__
#elif !defined(F77_NAME_LOWER_USCORE)
#pragma _HP_SECONDARY_DEF pmpi_scan  mpi_scan
#else
#pragma _HP_SECONDARY_DEF pmpi_scan_  mpi_scan_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(F77_NAME_UPPER)
#pragma _CRI duplicate MPI_SCAN as PMPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma _CRI duplicate mpi_scan__ as pmpi_scan__
#elif !defined(F77_NAME_LOWER_USCORE)
#pragma _CRI duplicate mpi_scan as pmpi_scan
#else
#pragma _CRI duplicate mpi_scan_ as pmpi_scan_
#endif

#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));

#else
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));

#endif
#endif /* HAVE_PRAGMA_WEAK */
#endif /* USE_WEAK_SYMBOLS */
/* End MPI profiling block */


/* These definitions are used only for generating the Fortran wrappers */
#if defined(USE_WEAK_SYMBOLS) && defined(USE_ONLY_MPI_NAMES)
#if defined(HAVE_MULTIPLE_PRAGMA_WEAK)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#if defined(F77_NAME_UPPER)
#pragma weak mpi_scan__ = MPI_SCAN
#pragma weak mpi_scan_ = MPI_SCAN
#pragma weak mpi_scan = MPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak MPI_SCAN = mpi_scan__
#pragma weak mpi_scan_ = mpi_scan__
#pragma weak mpi_scan = mpi_scan__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak MPI_SCAN = mpi_scan_
#pragma weak mpi_scan__ = mpi_scan_
#pragma weak mpi_scan = mpi_scan_
#else
#pragma weak MPI_SCAN = mpi_scan
#pragma weak mpi_scan__ = mpi_scan
#pragma weak mpi_scan_ = mpi_scan
#endif
#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("MPI_SCAN")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan_")));

#else
extern FORT_DLL_SPEC void FORT_CALL MPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("mpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL mpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#endif
#endif

#endif

/* Map the name to the correct form */
#ifndef MPICH_MPI_FROM_PMPI
#if defined(USE_WEAK_SYMBOLS)
#if defined(HAVE_MULTIPLE_PRAGMA_WEAK)
/* Define the weak versions of the PMPI routine*/
#ifndef F77_NAME_UPPER
extern FORT_DLL_SPEC void FORT_CALL PMPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER_2USCORE
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER_USCORE
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );
#endif
#ifndef F77_NAME_LOWER
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * );

#endif

#if defined(F77_NAME_UPPER)
#pragma weak pmpi_scan__ = PMPI_SCAN
#pragma weak pmpi_scan_ = PMPI_SCAN
#pragma weak pmpi_scan = PMPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#pragma weak PMPI_SCAN = pmpi_scan__
#pragma weak pmpi_scan_ = pmpi_scan__
#pragma weak pmpi_scan = pmpi_scan__
#elif defined(F77_NAME_LOWER_USCORE)
#pragma weak PMPI_SCAN = pmpi_scan_
#pragma weak pmpi_scan__ = pmpi_scan_
#pragma weak pmpi_scan = pmpi_scan_
#else
#pragma weak PMPI_SCAN = pmpi_scan
#pragma weak pmpi_scan__ = pmpi_scan
#pragma weak pmpi_scan_ = pmpi_scan
#endif /* Test on name mapping */

#elif defined(HAVE_WEAK_ATTRIBUTE)
#if defined(F77_NAME_UPPER)
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("PMPI_SCAN")));

#elif defined(F77_NAME_LOWER_2USCORE)
extern FORT_DLL_SPEC void FORT_CALL PMPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan__")));

#elif defined(F77_NAME_LOWER_USCORE)
extern FORT_DLL_SPEC void FORT_CALL PMPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan_")));

#else
extern FORT_DLL_SPEC void FORT_CALL PMPI_SCAN( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan__( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));
extern FORT_DLL_SPEC void FORT_CALL pmpi_scan_( void*, void*, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint *, MPI_Fint * ) __attribute__((weak,alias("pmpi_scan")));

#endif /* Test on name mapping */
#endif /* HAVE_MULTIPLE_PRAGMA_WEAK */
#endif /* USE_WEAK_SYMBOLS */

#ifdef F77_NAME_UPPER
#define mpi_scan_ PMPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#define mpi_scan_ pmpi_scan__
#elif !defined(F77_NAME_LOWER_USCORE)
#define mpi_scan_ pmpi_scan
#else
#define mpi_scan_ pmpi_scan_
#endif /* Test on name mapping */

/* This defines the routine that we call, which must be the PMPI version
   since we're renaming the Fortran entry as the pmpi version.  The MPI name
   must be undefined first to prevent any conflicts with previous renamings. */
#undef MPI_Scan
#define MPI_Scan PMPI_Scan 

#else

#ifdef F77_NAME_UPPER
#define mpi_scan_ MPI_SCAN
#elif defined(F77_NAME_LOWER_2USCORE)
#define mpi_scan_ mpi_scan__
#elif !defined(F77_NAME_LOWER_USCORE)
#define mpi_scan_ mpi_scan
/* Else leave name alone */
#endif


#endif /* MPICH_MPI_FROM_PMPI */

/* Prototypes for the Fortran interfaces */
#include "fproto.h"
FORT_DLL_SPEC void FORT_CALL mpi_scan_ ( void*v1, void*v2, MPI_Fint *v3, MPI_Fint *v4, MPI_Fint *v5, MPI_Fint *v6, MPI_Fint *ierr ){

#ifndef HAVE_MPI_F_INIT_WORKS_WITH_C
    if (MPIR_F_NeedInit){ mpirinitf_(); MPIR_F_NeedInit = 0; }
#endif
    if (v1 == MPIR_F_MPI_IN_PLACE) v1 = MPI_IN_PLACE;
    *ierr = MPI_Scan( v1, v2, (int)*v3, (MPI_Datatype)(*v4), (MPI_Op)*v5, (MPI_Comm)(*v6) );
}
