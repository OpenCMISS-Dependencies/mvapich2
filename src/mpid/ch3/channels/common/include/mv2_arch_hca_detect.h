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

#ifndef MV2_ARCH_HCA_DETECT_H
#define MV2_ARCH_HCA_DETECT_H

#include <stdint.h>

#if defined(HAVE_LIBIBVERBS)
#include <infiniband/verbs.h>
#endif

/* HCA Types */
#define MV2_HCA_UNKWN   0
#define MV2_HCA_ANY     (UINT32_MAX)

#define MV2_HCA_TYPE_IB

/* 
 * Layout:
 *
 * 1-4095 - IB Cards
 *         1 - 1000 - Mellanox Cards
 *      1001 - 2000 - Qlogic Cards
 *      2001 - 3000 - IBM Cards
 *
 * 4096-8191 - iWarp Cards 
 *      5001 - 6000 - Chelsio Cards
 *      6001 - 7000 - Intel iWarp Cards
 */

#define MV2_HCA_IB_TYPE_START       1
/* Mellanox Cards */
#define MV2_HCA_MLX_START           1
#define MV2_HCA_MLX_PCI_EX_SDR      2
#define MV2_HCA_MLX_PCI_EX_DDR      3
#define MV2_HCA_MLX_CX_SDR          4
#define MV2_HCA_MLX_CX_DDR          5
#define MV2_HCA_MLX_CX_QDR          6
#define MV2_HCA_MLX_CX_FDR          7
#define MV2_HCA_MLX_PCI_X           8
#define MV2_HCA_MLX_CX_CONNIB       9
#define MV2_HCA_MLX_END             1000

/* Qlogic Cards */
#define MV2_HCA_QLGIC_START     1001
#define MV2_HCA_QLGIC_PATH_HT   1002
#define MV2_HCA_QLGIC_QIB       1003
#define MV2_HCA_QLGIC_END       2000

/* IBM Cards */
#define MV2_HCA_IBM_START       2001
#define MV2_HCA_IBM_EHCA        2002
#define MV2_HCA_IBM_END         3000

#define MV2_HCA_IB_TYPE_END     4095

#define MV2_HCA_IWARP_TYPE_START    4096
/* Chelsio Cards */
#define MV2_HCA_CHLSIO_START    5001
#define MV2_HCA_CHELSIO_T3      5002
#define MV2_HCA_CHELSIO_T4      5003
#define MV2_HCA_CHLSIO_END      6000

/* Intel iWarp Cards */
#define MV2_HCA_INTEL_IWARP_START   6001
#define MV2_HCA_INTEL_NE020         6002
#define MV2_HCA_INTEL_IWARP_END     7000
#define MV2_HCA_IWARP_TYPE_END      8191


/* Check if given card is IB card or not */
#define MV2_IS_IB_CARD(_x) \
    ((_x) > MV2_HCA_IB_TYPE_START && (_x) < MV2_HCA_IB_TYPE_END)

/* Check if given card is iWarp card or not */
#define MV2_IS_IWARP_CARD(_x) \
    ((_x) > MV2_HCA_IWARP_TYPE_START && (_x) < MV2_HCA_IWARP_TYPE_END)

/* Check if given card is Chelsio iWarp card or not */
#define MV2_IS_CHELSIO_IWARP_CARD(_x) \
    ((_x) > MV2_HCA_CHLSIO_START && (_x) < MV2_HCA_CHLSIO_END)

/* Check if given card is QLogic card or not */
#define MV2_IS_QLE_CARD(_x) \
    ((_x) > MV2_HCA_QLGIC_START && (_x) < MV2_HCA_QLGIC_END)


/* Architecture Type 
 * Layout:
 *    1 - 1000 - Intel architectures
 * 1001 - 2000 - AMD architectures
 * 2001 - 3000 - IBM architectures
 */
#define MV2_ARCH_UNKWN  0
#define MV2_ARCH_ANY    (UINT32_MAX)

/* Intel Architectures */
#define MV2_ARCH_INTEL_START            1
#define MV2_ARCH_INTEL_GENERIC          2
#define MV2_ARCH_INTEL_CLOVERTOWN_8     3
#define MV2_ARCH_INTEL_NEHALEM_8        4
#define MV2_ARCH_INTEL_NEHALEM_16       5
#define MV2_ARCH_INTEL_HARPERTOWN_8     6
#define MV2_ARCH_INTEL_XEON_DUAL_4      7
#define MV2_ARCH_INTEL_XEON_E5630_8     8
#define MV2_ARCH_INTEL_XEON_X5650_12    9
#define MV2_ARCH_INTEL_XEON_E5_2670_16  10
#define MV2_ARCH_INTEL_XEON_E5_2680_16  11
#define MV2_ARCH_INTEL_XEON_E5_2670_V2_2S_20 12
#define MV2_ARCH_INTEL_XEON_E5_2630_V2_2S_12 13
#define MV2_ARCH_INTEL_XEON_E5_2680_V2_2S_20 14
#define MV2_ARCH_INTEL_XEON_E5_2690_V2_2S_20 15
#define MV2_ARCH_INTEL_XEON_E5_2698_V3_2S_32 16
#define MV2_ARCH_INTEL_XEON_E5_2660_V3_2S_20 17
#define MV2_ARCH_INTEL_XEON_E5_2680_V3_2S_24 18
#define MV2_ARCH_INTEL_END              1000

/* AMD Architectures */
#define MV2_ARCH_AMD_START                  1001
#define MV2_ARCH_AMD_GENERIC                1002
#define MV2_ARCH_AMD_BARCELONA_16           1003
#define MV2_ARCH_AMD_MAGNY_COURS_24         1004
#define MV2_ARCH_AMD_OPTERON_DUAL_4         1005
#define MV2_ARCH_AMD_OPTERON_6136_32        1006
#define MV2_ARCH_AMD_OPTERON_6276_64        1007
#define MV2_ARCH_AMD_BULLDOZER_4274HE_16    1008
#define MV2_ARCH_AMD_END                    2000
    
/* IBM Architectures */
#define MV2_ARCH_IBM_START  3001
#define MV2_ARCH_IBM_PPC    3002
#define MV2_ARCH_IBM_END    4000

typedef uint64_t mv2_arch_hca_type;
typedef uint32_t mv2_arch_type;
typedef uint32_t mv2_hca_type;

#define NUM_HCA_BITS (32)
#define NUM_ARCH_BITS (32)

#define MV2_GET_ARCH(_arch_hca) ((_arch_hca) >> NUM_HCA_BITS)
#define MV2_GET_HCA(_arch_hca) (UINT32_MAX & (_arch_hca))

/* CPU Family */
typedef enum{
    MV2_CPU_FAMILY_NONE=0,
    MV2_CPU_FAMILY_INTEL,
    MV2_CPU_FAMILY_AMD,
}mv2_cpu_family_type;

/* Multi-rail info */
typedef enum{
    mv2_num_rail_unknown = 0,
    mv2_num_rail_1,
    mv2_num_rail_2,
    mv2_num_rail_3,
    mv2_num_rail_4,
} mv2_multirail_info_type;

#define MV2_IS_ARCH_HCA_TYPE(_arch_hca, _arch, _hca) \
    mv2_is_arch_hca_type(_arch_hca, _arch, _hca)


/* ************************ FUNCTION DECLARATIONS ************************** */

/* Check arch-hca type */
int mv2_is_arch_hca_type(mv2_arch_hca_type arch_hca_type, 
        mv2_arch_type arch_type, mv2_hca_type hca_type);

/* Get architecture-hca type */
#if defined(HAVE_LIBIBVERBS)
mv2_arch_hca_type mv2_get_arch_hca_type (struct ibv_device *dev);
mv2_arch_hca_type mv2_new_get_arch_hca_type (struct ibv_context *ctx,
                                                struct ibv_device *dev,
                                                uint64_t *guid);
#else
mv2_arch_hca_type mv2_get_arch_hca_type (void *dev);
#endif

/* Check if the host has multiple rails or not */
mv2_multirail_info_type mv2_get_multirail_info(void);

/* Get architecture type */
mv2_arch_type mv2_get_arch_type(void);

/* Get card type */
#if defined(HAVE_LIBIBVERBS)
mv2_hca_type mv2_get_hca_type(struct ibv_device *dev);
mv2_hca_type mv2_new_get_hca_type(struct ibv_context *ctx, struct ibv_device *dev, uint64_t *guid);
#else
mv2_hca_type mv2_get_hca_type(void *dev);
#endif

/* Get number of cpus */
int mv2_get_num_cpus(void);

/* Get the CPU model */
int mv2_get_cpu_model(void);

/* Get CPU family */
mv2_cpu_family_type mv2_get_cpu_family(void);

/* Log arch-hca type */
void mv2_log_arch_hca_type(mv2_arch_hca_type arch_hca);

char* mv2_get_hca_name(mv2_hca_type hca_type);
char* mv2_get_arch_name(mv2_arch_type arch_type);
char *mv2_get_cpu_family_name(mv2_cpu_family_type cpu_family_type);

#if defined(_SMP_LIMIC_)
/*Detecting number of cores in a socket, and number of sockets*/
void hwlocSocketDetection(int print_details);

/*Returns the socket where the process is bound*/
int getProcessBinding(pid_t pid);

/*Returns the number of cores in the socket*/
int numOfCoresPerSocket(int socket);

/*Returns the total number of sockets within the node*/
int numofSocketsPerNode(void);

/*Return socket bind to */
int get_socket_bound(void);
#endif /* defined(_SMP_LIMIC_) */

#endif /*  #ifndef MV2_ARCH_HCA_DETECT_H */

