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

#include <stdio.h>
#include <string.h>

#include "mpichconf.h"

#if defined(HAVE_LIBIBUMAD)
#include <infiniband/umad.h>
#endif

#include "mv2_arch_hca_detect.h"

#include "debug_utils.h"

static mv2_multirail_info_type g_mv2_multirail_info = mv2_num_rail_unknown;

#define MV2_STR_MLX          "mlx"
#define MV2_STR_MLX4         "mlx4"
#define MV2_STR_MLX5         "mlx5"
#define MV2_STR_MTHCA        "mthca"
#define MV2_STR_IPATH        "ipath"
#define MV2_STR_QIB          "qib"
#define MV2_STR_EHCA         "ehca"
#define MV2_STR_CXGB3        "cxgb3"
#define MV2_STR_CXGB4        "cxgb4"
#define MV2_STR_NES0         "nes0"

typedef struct _mv2_hca_types_log_t{
    uint64_t hca_type;
    char *hca_name;
}mv2_hca_types_log_t;

#define MV2_HCA_LAST_ENTRY -1
static mv2_hca_types_log_t mv2_hca_types_log[] = {

    /* Mellanox Cards */
    {MV2_HCA_MLX_PCI_EX_SDR,"MV2_HCA_MLX_PCI_EX_SDR"},
    {MV2_HCA_MLX_PCI_EX_DDR,"MV2_HCA_MLX_PCI_EX_DDR"},
    {MV2_HCA_MLX_CX_SDR,    "MV2_HCA_MLX_CX_SDR"},
    {MV2_HCA_MLX_CX_DDR,    "MV2_HCA_MLX_CX_DDR"},
    {MV2_HCA_MLX_CX_QDR,    "MV2_HCA_MLX_CX_QDR"},
    {MV2_HCA_MLX_CX_FDR,    "MV2_HCA_MLX_CX_FDR"},
    {MV2_HCA_MLX_CX_CONNIB,    "MV2_HCA_MLX_CX_CONNIB"},
    {MV2_HCA_MLX_PCI_X,     "MV2_HCA_MLX_PCI_X"},

    /* Qlogic Cards */
    {MV2_HCA_QLGIC_PATH_HT, "MV2_HCA_QLGIC_PATH_HT"},
    {MV2_HCA_QLGIC_QIB,     "MV2_HCA_QLGIC_QIB"},

    /* IBM Cards */
    {MV2_HCA_IBM_EHCA,      "MV2_HCA_IBM_EHCA"},
    
    /* Chelsio Cards */
    {MV2_HCA_CHELSIO_T3,    "MV2_HCA_CHELSIO_T3"},
    {MV2_HCA_CHELSIO_T4,    "MV2_HCA_CHELSIO_T4"},

    /* Intel iWarp Cards */
    {MV2_HCA_INTEL_NE020,   "MV2_HCA_INTEL_NE020"},

    /*Unknown */
    {MV2_HCA_UNKWN,         "MV2_HCA_UNKWN"},
    {MV2_HCA_LAST_ENTRY,    "MV2_HCA_LAST_ENTRY"},
};

char* mv2_get_hca_name(mv2_hca_type hca_type)
{
    int i=0;
    while(mv2_hca_types_log[i].hca_type != MV2_HCA_LAST_ENTRY){

        if(mv2_hca_types_log[i].hca_type == hca_type){
            return(mv2_hca_types_log[i].hca_name);
        }
        i++;
    }
    return("MV2_HCA_UNKWN");
}

#if defined(HAVE_LIBIBUMAD)
static int get_rate(umad_ca_t *umad_ca)
{
    int i;
    char *value;

    if ((value = getenv("MV2_DEFAULT_PORT")) != NULL) {
        int default_port = atoi(value);
        
        if(default_port <= umad_ca->numports){
            return umad_ca->ports[default_port]->rate;
        }
    }

    for (i = 1; i <= umad_ca->numports; i++) {
        if (IBV_PORT_ACTIVE == umad_ca->ports[i]->state) {
            return umad_ca->ports[i]->rate;
        }
    }    
    return 0;
}
#endif

static const int get_link_width(uint8_t width)
{
    switch (width) {
    case 1:  return 1;
    case 2:  return 4;
    case 4:  return 8;
    case 8:  return 12;
    default: return 0;
    }   
}

static const float get_link_speed(uint8_t speed)
{
    switch (speed) {
    case 1:  return 2.5;  /* SDR */
    case 2:  return 5.0;  /* DDR */

    case 4:  /* fall through */
    case 8:  return 10.0; /* QDR */

    case 16: return 14.0; /* FDR */
    case 32: return 25.0; /* EDR */
    default: return 0;    /* Invalid speed */
    }   
}

#if defined(HAVE_LIBIBVERBS)
mv2_hca_type mv2_new_get_hca_type(struct ibv_context *ctx,
                                    struct ibv_device *ib_dev,
                                    uint64_t *guid)
{
    int rate=0;
    char *dev_name = NULL;
    struct ibv_device_attr device_attr;
    int max_ports = 0;
    mv2_hca_type hca_type = MV2_HCA_UNKWN;

    dev_name = (char*) ibv_get_device_name( ib_dev );

    if (!dev_name) {
        return MV2_HCA_UNKWN;
    }

    memset(&device_attr, 0, sizeof(struct ibv_device_attr));
    if(!ibv_query_device(ctx, &device_attr)){
        max_ports = device_attr.phys_port_cnt;
        *guid = device_attr.node_guid;
    }

    if (!strncmp(dev_name, MV2_STR_MLX, 3)
        || !strncmp(dev_name, MV2_STR_MTHCA, 5)) {

        hca_type = MV2_HCA_MLX_PCI_X;

        int query_port = 1;
        char *value;
        struct ibv_port_attr port_attr;

        /* honor MV2_DEFAULT_PORT, if set */
        if ((value = getenv("MV2_DEFAULT_PORT")) != NULL) {

            int default_port = atoi(value);
            query_port = (default_port <= max_ports) ? default_port : 1;
        }

        if (!ibv_query_port(ctx, query_port, &port_attr)) {
            rate = (int) (get_link_width(port_attr.active_width)
                    * get_link_speed(port_attr.active_speed));
            PRINT_DEBUG(0, "rate : %d\n", rate);
        }
        /* mlx4, mlx5 */ 
        switch(rate) {
            case 56:
                hca_type = MV2_HCA_MLX_CX_FDR;
                break;

            case 40:
                hca_type = MV2_HCA_MLX_CX_QDR;
                break;

            case 20:
                hca_type = MV2_HCA_MLX_CX_DDR;
                break;

            case 10:
                hca_type = MV2_HCA_MLX_CX_SDR;
                break;

            default:
                hca_type = MV2_HCA_MLX_CX_FDR;
                break;
        }
        if (!strncmp(dev_name, MV2_STR_MLX5, 4) && rate == 56)
                hca_type = MV2_HCA_MLX_CX_CONNIB; 
    } else if(!strncmp(dev_name, MV2_STR_IPATH, 5)) {
        hca_type = MV2_HCA_QLGIC_PATH_HT;

    } else if(!strncmp(dev_name, MV2_STR_QIB, 3)) {
        hca_type = MV2_HCA_QLGIC_QIB;

    } else if(!strncmp(dev_name, MV2_STR_EHCA, 4)) {
        hca_type = MV2_HCA_IBM_EHCA;

    } else if (!strncmp(dev_name, MV2_STR_CXGB3, 5)) {
        hca_type = MV2_HCA_CHELSIO_T3;

    } else if (!strncmp(dev_name, MV2_STR_CXGB4, 5)) {
        hca_type = MV2_HCA_CHELSIO_T4;

    } else if (!strncmp(dev_name, MV2_STR_NES0, 4)) {
        hca_type = MV2_HCA_INTEL_NE020;

    } else {
        hca_type = MV2_HCA_UNKWN;
    }    

    return hca_type;
}

mv2_hca_type mv2_get_hca_type( struct ibv_device *dev )
{
    int rate=0;
    char *dev_name;
    mv2_hca_type hca_type = MV2_HCA_UNKWN;

    dev_name = (char*) ibv_get_device_name( dev );

    if (!dev_name) {
        return MV2_HCA_UNKWN;
    }

    if (!strncmp(dev_name, MV2_STR_MLX4, 4)
        || !strncmp(dev_name, MV2_STR_MLX5, 4) 
        || !strncmp(dev_name, MV2_STR_MTHCA, 5)) {

        hca_type = MV2_HCA_MLX_PCI_X;
#if !defined(HAVE_LIBIBUMAD)
        int query_port = 1;
        char *value;
        struct ibv_context *ctx= NULL;
        struct ibv_port_attr port_attr;


        ctx = ibv_open_device(dev);
        if (!ctx) {
            return MV2_HCA_UNKWN;
        }

        /* honor MV2_DEFAULT_PORT, if set */
        if ((value = getenv("MV2_DEFAULT_PORT")) != NULL) {

            int max_ports = 1;
            struct ibv_device_attr device_attr;
            int default_port = atoi(value);
            
            memset(&device_attr, 0, sizeof(struct ibv_device_attr));
            if(!ibv_query_device(ctx, &device_attr)){
                max_ports = device_attr.phys_port_cnt;
            }
            query_port = (default_port <= max_ports) ? default_port : 1;
        }
        
        if (!ibv_query_port(ctx, query_port, &port_attr)) {
            rate = (int) (get_link_width( port_attr.active_width)
                    * get_link_speed( port_attr.active_speed));
            PRINT_DEBUG(0, "rate : %d\n", rate);
        }
#else
        umad_ca_t umad_ca;
        if (umad_init() < 0) {
            return hca_type;
        }

        memset(&umad_ca, 0, sizeof(umad_ca_t));

        if (umad_get_ca(dev_name, &umad_ca) < 0) {
            return hca_type;
        }

        if (!getenv("MV2_USE_RoCE")) {
            rate = get_rate(&umad_ca);
            if (!rate) {
                umad_release_ca(&umad_ca);
                umad_done();
                return hca_type;
            }
        }

        umad_release_ca(&umad_ca);
        umad_done();

        if (!strncmp(dev_name, MV2_STR_MTHCA, 5)) {
            hca_type = MV2_HCA_MLX_PCI_X;


            if (!strncmp(umad_ca.ca_type, "MT25", 4)) {
                switch (rate) {
                    case 20:
                        hca_type = MV2_HCA_MLX_PCI_EX_DDR;
                        break;

                    case 10:
                        hca_type = MV2_HCA_MLX_PCI_EX_SDR;
                        break;

                    default:
                        hca_type = MV2_HCA_MLX_PCI_EX_SDR;
                        break;
                }

            } else if (!strncmp(umad_ca.ca_type, "MT23", 4)) {
                hca_type = MV2_HCA_MLX_PCI_X;

            } else {
                hca_type = MV2_HCA_MLX_PCI_EX_SDR; 
            }
        } else 
#endif
        { /* mlx4, mlx5 */ 
            switch(rate) {
                case 56:
                    hca_type = MV2_HCA_MLX_CX_FDR;
                    break;

                case 40:
                    hca_type = MV2_HCA_MLX_CX_QDR;
                    break;

                case 20:
                    hca_type = MV2_HCA_MLX_CX_DDR;
                    break;

                case 10:
                    hca_type = MV2_HCA_MLX_CX_SDR;
                    break;

                default:
                    hca_type = MV2_HCA_MLX_CX_SDR;
                    break;
            }
            if (!strncmp(dev_name, MV2_STR_MLX5, 4) && rate == 56)
                    hca_type = MV2_HCA_MLX_CX_CONNIB; 
        }

    } else if(!strncmp(dev_name, MV2_STR_IPATH, 5)) {
        hca_type = MV2_HCA_QLGIC_PATH_HT;

    } else if(!strncmp(dev_name, MV2_STR_QIB, 3)) {
        hca_type = MV2_HCA_QLGIC_QIB;

    } else if(!strncmp(dev_name, MV2_STR_EHCA, 4)) {
        hca_type = MV2_HCA_IBM_EHCA;

    } else if (!strncmp(dev_name, MV2_STR_CXGB3, 5)) {
        hca_type = MV2_HCA_CHELSIO_T3;

    } else if (!strncmp(dev_name, MV2_STR_CXGB4, 5)) {
        hca_type = MV2_HCA_CHELSIO_T4;

    } else if (!strncmp(dev_name, MV2_STR_NES0, 4)) {
        hca_type = MV2_HCA_INTEL_NE020;

    } else {
        hca_type = MV2_HCA_UNKWN;
    }    
    return hca_type;
}
#else
mv2_hca_type mv2_get_hca_type(void *dev)
{
    return MV2_HCA_UNKWN;
}
#endif

#if defined(HAVE_LIBIBVERBS)
mv2_arch_hca_type mv2_new_get_arch_hca_type (struct ibv_context *ctx,
                                    struct ibv_device *ib_dev, uint64_t *guid)
{
    mv2_arch_hca_type arch_hca = mv2_get_arch_type();
    arch_hca = arch_hca << 32 | mv2_new_get_hca_type(ctx, ib_dev, guid);
    return arch_hca;
}

mv2_arch_hca_type mv2_get_arch_hca_type (struct ibv_device *dev)
{
    mv2_arch_hca_type arch_hca = mv2_get_arch_type();
    arch_hca = arch_hca << 32 | mv2_get_hca_type(dev);
    return arch_hca;
}
#else 
mv2_arch_hca_type mv2_get_arch_hca_type (void *dev)
{
    mv2_arch_hca_type arch_hca = mv2_get_arch_type();
    arch_hca = arch_hca << 32 | mv2_get_hca_type(dev);
    return arch_hca;
}
#endif

#if defined(HAVE_LIBIBVERBS)
mv2_multirail_info_type mv2_get_multirail_info()
{
    if ( mv2_num_rail_unknown == g_mv2_multirail_info ) {
        int num_devices;
        struct ibv_device **dev_list = NULL;

        /* Get the number of rails */
        dev_list = ibv_get_device_list(&num_devices);

        switch (num_devices){
            case 1:
                g_mv2_multirail_info = mv2_num_rail_1;
                break;
            case 2:
                g_mv2_multirail_info = mv2_num_rail_2;
                break;
            case 3:
                g_mv2_multirail_info = mv2_num_rail_3;
                break;
            case 4:
                g_mv2_multirail_info = mv2_num_rail_4;
                break;
            default:
                g_mv2_multirail_info = mv2_num_rail_unknown;
                break;
        }
        ibv_free_device_list(dev_list);
    }
    return g_mv2_multirail_info;
}
#else
mv2_multirail_info_type mv2_get_multirail_info()
{
    return mv2_num_rail_unknown;
}

#endif

