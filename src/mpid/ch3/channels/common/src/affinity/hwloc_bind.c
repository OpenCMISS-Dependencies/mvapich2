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
#include "mpichconf.h"
#include "mpidi_ch3_impl.h"
#include <mpimem.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "upmi.h"
#include "smp_smpi.h"
#include "mpiutil.h"
#include "hwloc_bind.h"
#include "rdma_impl.h"
#include <hwloc/openfabrics-verbs.h>

/* CPU Mapping related definitions */

#define CONFIG_FILE "/proc/cpuinfo"
#define MAX_LINE_LENGTH 512
#define MAX_NAME_LENGTH 64


unsigned int mv2_enable_affinity = 1;
unsigned int mv2_enable_leastload = 0;

typedef enum {
    CPU_FAMILY_NONE = 0,
    CPU_FAMILY_INTEL,
    CPU_FAMILY_AMD,
} cpu_type_t;

int CLOVERTOWN_MODEL = 15;
int HARPERTOWN_MODEL = 23;
int NEHALEM_MODEL = 26;

int ip = 0;
unsigned long *core_mapping = NULL;
int *obj_tree = NULL;

policy_type_t policy;
level_type_t level;
hwloc_topology_t topology;

static int INTEL_XEON_DUAL_MAPPING[] = { 0, 1, 0, 1 };

/* ((0,1),(4,5))((2,3),(6,7)) */
static int INTEL_CLOVERTOWN_MAPPING[] = { 0, 0, 1, 1, 0, 0, 1, 1 };

/* legacy ((0,2),(4,6))((1,3),(5,7)) */
static int INTEL_HARPERTOWN_LEG_MAPPING[] = { 0, 1, 0, 1, 0, 1, 0, 1 };

/* common ((0,1),(2,3))((4,5),(6,7)) */
static int INTEL_HARPERTOWN_COM_MAPPING[] = { 0, 0, 0, 0, 1, 1, 1, 1 };

/* legacy (0,2,4,6)(1,3,5,7) with hyperthreading */
static int INTEL_NEHALEM_LEG_MAPPING[] =
    { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 };

/* common (0,1,2,3)(4,5,6,7) with hyperthreading */
static int INTEL_NEHALEM_COM_MAPPING[] =
    { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 };

static int AMD_OPTERON_DUAL_MAPPING[] = { 0, 0, 1, 1 };
static int AMD_BARCELONA_MAPPING[] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3 };

extern int use_hwloc_cpu_binding;

char *s_cpu_mapping = NULL;
static char *custom_cpu_mapping = NULL;
int s_cpu_mapping_line_max = _POSIX2_LINE_MAX;
static int custom_cpu_mapping_line_max = _POSIX2_LINE_MAX;
char *cpu_mapping = NULL;

int ib_socket_bind = 0;

int get_ib_socket(struct ibv_device * ibdev)
{
    hwloc_cpuset_t set = hwloc_bitmap_alloc();
    hwloc_obj_t osdev = NULL;
    char string[256];
    int socket_id = 0;

    if (NULL == set) {
        goto free_and_return;
    }

    if (hwloc_ibv_get_device_cpuset(topology, ibdev, set)) {
        goto free_and_return;
    }

    osdev = hwloc_get_obj_inside_cpuset_by_type(topology, set,
            HWLOC_OBJ_SOCKET, 0);

    if (NULL == osdev) {
        goto free_and_return;
    }

    /*
     * The hwloc object "string" will have the form "Socket#n" so we are
     * looking at the 8th char to detect which socket is.
     */
    hwloc_obj_snprintf(string, sizeof(string), topology, osdev, "#", 1);
    return atoi(&string[7]);

free_and_return:
    hwloc_bitmap_free(set);
    return socket_id;
}

static int first_num_from_str(char **str)
{
    int val = atoi(*str);
    while (isdigit(**str)) {
        (*str)++;
    }
    return val;
}

static inline int compare_float(const float a, const float b)
{
    const float precision = 0.00001;
    if ((a - precision) < b && (a + precision) > b) {
        return 1;
    } else {
        return 0;
    }
}

static int pid_filter(const struct dirent *dir_obj)
{
    int i;
    int length = strlen(dir_obj->d_name);

    for (i = 0; i < length; i++) {
        if (!isdigit(dir_obj->d_name[i])) {
            return 0;
        }
    }
    return 1;
}

static void find_parent(hwloc_obj_t obj, hwloc_obj_type_t type, hwloc_obj_t * parent)
{
    if ((type == HWLOC_OBJ_CORE) || (type == HWLOC_OBJ_SOCKET)
        || (type == HWLOC_OBJ_NODE)) {
        if (obj->parent->type == type) {
            *parent = obj->parent;
            return;
        } else {
            find_parent(obj->parent, type, parent);
        }
    } else {
        return;
    }
}

static void find_leastload_node(obj_attribute_type * tree, hwloc_obj_t original,
                                hwloc_obj_t * result)
{
    int i, j, k, per, ix, depth_nodes, num_nodes, depth_sockets, num_sockets;
    hwloc_obj_t obj, tmp;

    depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
    num_nodes = hwloc_get_nbobjs_by_depth(topology, depth_nodes);

    /* One socket includes multi numanodes. */
    if ((original->type == HWLOC_OBJ_SOCKET)) {
        depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
        num_sockets = hwloc_get_nbobjs_by_depth(topology, depth_sockets);
        per = num_nodes / num_sockets;
        ix = (original->logical_index) * per;
        if (per == 1) {
            *result = tree[depth_nodes * num_nodes + ix].obj;
        } else {
            i = depth_nodes * num_nodes + ix;
            for (k = 0; k < (per - 1); k++) {
                j = i + k + 1;
                i = (tree[i].load > tree[j].load) ? j : i;
            }
            *result = tree[i].obj;
        }
    } else if (original->type == HWLOC_OBJ_MACHINE) {
        tmp = NULL;
        for (k = 0; k < num_nodes; k++) {
            obj = hwloc_get_obj_by_depth(topology, depth_nodes, k);
            if (tmp == NULL) {
                tmp = obj;
            } else {
                i = depth_nodes * num_nodes + tmp->logical_index;
                j = depth_nodes * num_nodes + obj->logical_index;
                if (tree[i].load > tree[j].load)
                    tmp = obj;
            }
        }
        *result = tmp;
    } else {
        *result = NULL;
    }
    return;
}

static void find_leastload_socket(obj_attribute_type * tree, hwloc_obj_t original,
                                  hwloc_obj_t * result)
{
    int i, j, k, per, ix, depth_sockets, num_sockets, depth_nodes, num_nodes;
    hwloc_obj_t obj, tmp;

    depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
    num_sockets = hwloc_get_nbobjs_by_depth(topology, depth_sockets);

    /* One numanode includes multi sockets. */
    if ((original->type == HWLOC_OBJ_NODE)) {
        depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
        num_nodes = hwloc_get_nbobjs_by_depth(topology, depth_nodes);
        per = num_sockets / num_nodes;
        ix = (original->logical_index) * per;
        if (per == 1) {
            *result = tree[depth_sockets * num_sockets + ix].obj;
        } else {
            i = depth_sockets * num_sockets + ix;
            for (k = 0; k < (per - 1); k++) {
                j = i + k + 1;
                i = (tree[i].load > tree[j].load) ? j : i;
            }
            *result = tree[i].obj;
        }
    } else if (original->type == HWLOC_OBJ_MACHINE) {
        tmp = NULL;
        for (k = 0; k < num_sockets; k++) {
            obj = hwloc_get_obj_by_depth(topology, depth_sockets, k);
            if (tmp == NULL) {
                tmp = obj;
            } else {
                i = depth_sockets * num_sockets + tmp->logical_index;
                j = depth_sockets * num_sockets + obj->logical_index;
                if (tree[i].load > tree[j].load)
                    tmp = obj;
            }
        }
        *result = tmp;
    } else {
        *result = NULL;
    }
    return;
}

static void find_leastload_core(obj_attribute_type * tree, hwloc_obj_t original,
                                hwloc_obj_t * result)
{
    int i, j, k, per, ix;
    int depth_cores, num_cores, depth_sockets, num_sockets, depth_nodes, num_nodes;

    depth_cores = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
    num_cores = hwloc_get_nbobjs_by_depth(topology, depth_cores);

    /* Core may have Socket or Numanode as direct parent. */
    if ((original->type == HWLOC_OBJ_NODE)) {
        depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
        num_nodes = hwloc_get_nbobjs_by_depth(topology, depth_nodes);
        per = num_cores / num_nodes;
        ix = (original->logical_index) * per;
        if (per == 1) {
            *result = tree[depth_cores * num_cores + ix].obj;
        } else {
            i = depth_cores * num_cores + ix;
            for (k = 0; k < (per - 1); k++) {
                j = i + k + 1;
                i = (tree[i].load > tree[j].load) ? j : i;
            }
            *result = tree[i].obj;
        }
    } else if (original->type == HWLOC_OBJ_SOCKET) {
        depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
        num_sockets = hwloc_get_nbobjs_by_depth(topology, depth_sockets);
        per = num_cores / num_sockets;
        ix = (original->logical_index) * per;
        if (per == 1) {
            *result = tree[depth_cores * num_cores + ix].obj;
        } else {
            i = depth_cores * num_cores + ix;
            for (k = 0; k < (per - 1); k++) {
                j = i + k + 1;
                i = (tree[i].load > tree[j].load) ? j : i;
            }
            *result = tree[i].obj;
        }
    } else {
        *result = NULL;
    }
    return;
}

static void find_leastload_pu(obj_attribute_type * tree, hwloc_obj_t original,
                              hwloc_obj_t * result)
{
    int i, j, k, per, ix, depth_pus, num_pus, depth_cores, num_cores;

    depth_pus = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
    num_pus = hwloc_get_nbobjs_by_depth(topology, depth_pus);

    /* Assume: pu only has core as direct parent. */
    if ((original->type == HWLOC_OBJ_CORE)) {
        depth_cores = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
        num_cores = hwloc_get_nbobjs_by_depth(topology, depth_cores);
        per = num_pus / num_cores;
        ix = (original->logical_index) * per;
        if (per == 1) {
            *result = tree[depth_pus * num_pus + ix].obj;
        } else {
            i = depth_pus * num_pus + ix;
            for (k = 0; k < (per - 1); k++) {
                j = i + k + 1;
                i = (tree[i].load > tree[j].load) ? j : i;
            }
            *result = tree[i].obj;
        }
    } else {
        *result = NULL;
    }
    return;
}


static void update_obj_attribute(obj_attribute_type * tree, int ix, hwloc_obj_t obj,
                                 int cpuset, float load)
{
    tree[ix].obj = obj;
    if (!(cpuset < 0)) {
        CPU_SET(cpuset, &(tree[ix].cpuset));
    }
    tree[ix].load += load;
}

static void insert_load(obj_attribute_type * tree, hwloc_obj_t pu, int cpuset, float load)
{
    int k, depth_pus, num_pus = 0;
    int depth_cores, depth_sockets, depth_nodes, num_cores = 0, num_sockets =
        0, num_nodes = 0;
    hwloc_obj_t parent;

    depth_pus = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_PU);
    num_pus = hwloc_get_nbobjs_by_depth(topology, depth_pus);

    depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
    if (depth_nodes != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_nodes = hwloc_get_nbobjs_by_depth(topology, depth_nodes);
    }
    depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
    if (depth_sockets != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_sockets = hwloc_get_nbobjs_by_depth(topology, depth_sockets);
    }
    depth_cores = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
    if (depth_cores != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_cores = hwloc_get_nbobjs_by_depth(topology, depth_cores);
    }

    /* Add obj, cpuset and load for HWLOC_OBJ_PU */
    k = depth_pus * num_pus + pu->logical_index;
    update_obj_attribute(tree, k, pu, cpuset, load);
    /* Add cpuset and load for HWLOC_OBJ_CORE */
    if (depth_cores != HWLOC_TYPE_DEPTH_UNKNOWN) {
        find_parent(pu, HWLOC_OBJ_CORE, &parent);
        k = depth_cores * num_cores + parent->logical_index;
        update_obj_attribute(tree, k, parent, cpuset, load);
    }
    /* Add cpuset and load for HWLOC_OBJ_SOCKET */
    if (depth_sockets != HWLOC_TYPE_DEPTH_UNKNOWN) {
        find_parent(pu, HWLOC_OBJ_SOCKET, &parent);
        k = depth_sockets * num_sockets + parent->logical_index;
        update_obj_attribute(tree, k, parent, cpuset, load);
    }
    /* Add cpuset and load for HWLOC_OBJ_NODE */
    if (depth_nodes != HWLOC_TYPE_DEPTH_UNKNOWN) {
        find_parent(pu, HWLOC_OBJ_NODE, &parent);
        k = depth_nodes * num_nodes + parent->logical_index;
        update_obj_attribute(tree, k, parent, cpuset, load);
    }
    return;
}

static void cac_load(obj_attribute_type * tree, cpu_set_t cpuset)
{
    int i, j, depth_pus, num_pus;
    float proc_load;
    int num_processes = 0;
    hwloc_obj_t obj;

    depth_pus = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_PU);
    num_pus = hwloc_get_nbobjs_by_depth(topology, depth_pus);

    for (i = 0; i < num_pus; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            num_processes++;
        }
    }

    /* Process is running on num_processes cores; for each core, the load is proc_load. */
    proc_load = 1 / num_processes;

    /*
     * num_objs is HWLOC_OBJ_PU number, and system CPU number;
     * also HWLOC_OBJ_CORE number when HT disabled or without HT.
     */

    for (i = 0; i < num_pus; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            for (j = 0; j < num_pus; j++) {
                obj = hwloc_get_obj_by_depth(topology, depth_pus, j);
                if (obj->os_index == i) {
                    insert_load(tree, obj, i, proc_load);
                }
            }
        }
    }
    return;
}

static void insert_core_mapping(int ix, hwloc_obj_t pu, obj_attribute_type * tree)
{
    core_mapping[ix] = pu->os_index;
    /* This process will be binding to one pu/core.
     * The load for this pu/core is 1; and not update cpuset.
     */
    insert_load(tree, pu, -1, 1);
    return;
}

void map_scatter_load(obj_attribute_type * tree)
{
    int k;
    int depth_cores, depth_sockets, depth_nodes, num_cores = 0;
    hwloc_obj_t root, node, sockets, core_parent, core, result;

    root = hwloc_get_root_obj(topology);

    depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);

    depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);

    depth_cores = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
    if (depth_cores != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_cores = hwloc_get_nbobjs_by_depth(topology, depth_cores);
    }

    k = 0;
    /*Assume: there is always existing SOCKET, but not always existing NUMANODE(like Clovertown). */
    while (k < num_cores) {
        if (depth_nodes == HWLOC_TYPE_DEPTH_UNKNOWN) {
            find_leastload_socket(tree, root, &result);
        } else {
            if ((depth_nodes) < (depth_sockets)) {
                find_leastload_node(tree, root, &result);
                node = result;
                find_leastload_socket(tree, node, &result);
            } else {
                find_leastload_socket(tree, root, &result);
                sockets = result;
                find_leastload_node(tree, sockets, &result);
            }
        }
        core_parent = result;
        find_leastload_core(tree, core_parent, &result);
        core = result;
        find_leastload_pu(tree, core, &result);
        insert_core_mapping(k, result, tree);
        k++;
    }
}

void map_bunch_load(obj_attribute_type * tree)
{
    int i, j, k, per = 0;
    int per_socket_node, depth_pus, num_pus = 0;
    float current_socketornode_load = 0, current_core_load = 0;
    int depth_cores, depth_sockets, depth_nodes, num_cores = 0, num_sockets =
        0, num_nodes = 0;
    hwloc_obj_t root, node, sockets, core_parent, core, pu, result;

    root = hwloc_get_root_obj(topology);

    depth_nodes = hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
    if (depth_nodes != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_nodes = hwloc_get_nbobjs_by_depth(topology, depth_nodes);
    }

    depth_sockets = hwloc_get_type_depth(topology, HWLOC_OBJ_SOCKET);
    if (depth_sockets != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_sockets = hwloc_get_nbobjs_by_depth(topology, depth_sockets);
    }

    depth_cores = hwloc_get_type_depth(topology, HWLOC_OBJ_CORE);
    if (depth_cores != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_cores = hwloc_get_nbobjs_by_depth(topology, depth_cores);
    }

    depth_pus = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
    if (depth_pus != HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_pus = hwloc_get_nbobjs_by_depth(topology, depth_pus);
    }

    k = 0;
    /*Assume: there is always existing SOCKET, but not always existing NUMANODE(like Clovertown). */
    while (k < num_cores) {
        if (depth_nodes == HWLOC_TYPE_DEPTH_UNKNOWN) {
            find_leastload_socket(tree, root, &result);
            core_parent = result;
            per = num_cores / num_sockets;
            for (i = 0; (i < per) && (k < num_cores); i++) {
                find_leastload_core(tree, core_parent, &result);
                core = result;
                find_leastload_pu(tree, core, &result);
                pu = result;
                if (i == 0) {
                    current_core_load =
                        tree[depth_pus * num_pus + pu->logical_index].load;
                    insert_core_mapping(k, pu, tree);
                    k++;
                } else {
                    if (compare_float
                        (tree[depth_pus * num_pus + pu->logical_index].load,
                         current_core_load)) {
                        insert_core_mapping(k, pu, tree);
                        k++;
                    }
                }
            }
        } else {
            if ((depth_nodes) < (depth_sockets)) {
                find_leastload_node(tree, root, &result);
                node = result;
                per_socket_node = num_sockets / num_nodes;
                for (j = 0; (j < per_socket_node) && (k < num_cores); j++) {
                    find_leastload_socket(tree, node, &result);
                    sockets = result;
                    if (j == 0) {
                        current_socketornode_load =
                            tree[depth_sockets * num_sockets +
                                 sockets->logical_index].load;
                        per = num_cores / num_sockets;
                        for (i = 0; (i < per) && (k < num_cores); i++) {
                            find_leastload_core(tree, sockets, &result);
                            core = result;
                            find_leastload_pu(tree, core, &result);
                            pu = result;
                            if (i == 0) {
                                current_core_load =
                                    tree[depth_pus * num_pus + pu->logical_index].load;
                                insert_core_mapping(k, pu, tree);
                                k++;
                            } else {
                                if (compare_float
                                    (tree[depth_pus * num_pus + pu->logical_index].load,
                                     current_core_load)) {
                                    insert_core_mapping(k, pu, tree);
                                    k++;
                                }
                            }
                        }
                    } else {
                        if (compare_float
                            (tree
                             [depth_sockets * num_sockets + sockets->logical_index].load,
                             current_socketornode_load)) {
                            for (i = 0; (i < per) && (k < num_cores); i++) {
                                find_leastload_core(tree, sockets, &result);
                                core = result;
                                find_leastload_pu(tree, core, &result);
                                pu = result;
                                if (i == 0) {
                                    current_core_load =
                                        tree[depth_pus * num_pus +
                                             pu->logical_index].load;
                                    insert_core_mapping(k, pu, tree);
                                    k++;
                                } else {
                                    if (compare_float
                                        (tree
                                         [depth_pus * num_pus + pu->logical_index].load,
                                         current_core_load)) {
                                        insert_core_mapping(k, pu, tree);
                                        k++;
                                    }
                                }
                            }

                        }
                    }
                }
            } else {    // depth_nodes > depth_sockets
                find_leastload_socket(tree, root, &result);
                sockets = result;
                per_socket_node = num_nodes / num_sockets;
                for (j = 0; (j < per_socket_node) && (k < num_cores); j++) {
                    find_leastload_node(tree, sockets, &result);
                    node = result;
                    if (j == 0) {
                        current_socketornode_load =
                            tree[depth_nodes * num_nodes + node->logical_index].load;
                        per = num_cores / num_sockets;
                        for (i = 0; (i < per) && (k < num_cores); i++) {
                            find_leastload_core(tree, node, &result);
                            core = result;
                            find_leastload_pu(tree, core, &result);
                            pu = result;
                            if (i == 0) {
                                current_core_load =
                                    tree[depth_pus * num_pus + pu->logical_index].load;
                                insert_core_mapping(k, pu, tree);
                                k++;
                            } else {
                                if (compare_float
                                    (tree[depth_pus * num_pus + pu->logical_index].load,
                                     current_core_load)) {
                                    insert_core_mapping(k, pu, tree);
                                    k++;
                                }
                            }
                        }
                    } else {
                        if (compare_float
                            (tree[depth_nodes * num_nodes + node->logical_index].load,
                             current_socketornode_load)) {
                            for (i = 0; (i < per) && (k < num_cores); i++) {
                                find_leastload_core(tree, node, &result);
                                core = result;
                                find_leastload_pu(tree, core, &result);
                                pu = result;
                                if (i == 0) {
                                    current_core_load =
                                        tree[depth_pus * num_pus +
                                             pu->logical_index].load;
                                    insert_core_mapping(k, pu, tree);
                                    k++;
                                } else {
                                    if (compare_float
                                        (tree
                                         [depth_pus * num_pus + pu->logical_index].load,
                                         current_core_load)) {
                                        insert_core_mapping(k, pu, tree);
                                        k++;
                                    }
                                }
                            }
                        }
                    }
                }
            }   /* depth_nodes > depth_sockets */
        }
    }   /* while */
}

/*
 * Compare two hwloc_obj_t of type HWLOC_OBJ_PU according to sibling_rank, used with qsort
 */
static int cmpproc_smt(const void *a, const void *b)
{
    hwloc_obj_t pa = *(hwloc_obj_t *) a;
    hwloc_obj_t pb = *(hwloc_obj_t *) b;
    return (pa->sibling_rank ==
            pb->sibling_rank) ? pa->os_index - pb->os_index : pa->sibling_rank -
        pb->sibling_rank;
}

static int cmpdepth_smt(const void *a, const void *b)
{
    ancestor_type pa = *(ancestor_type *) a;
    ancestor_type pb = *(ancestor_type *) b;
    if ((pa.ancestor)->depth > (pb.ancestor)->depth) {
        return -1;
    } else if ((pa.ancestor)->depth < (pb.ancestor)->depth) {
        return 1;
    } else {
        return 0;
    }
}

static int cmparity_smt(const void *a, const void *b)
{
    ancestor_type pa = *(ancestor_type *) a;
    ancestor_type pb = *(ancestor_type *) b;
    if ((pa.ancestor)->arity > (pb.ancestor)->arity) {
        return -1;
    } else if ((pa.ancestor)->arity < (pb.ancestor)->arity) {
        return 1;
    } else {
        return 0;
    }
}

static void get_first_obj_bunch(hwloc_obj_t * result)
{
    hwloc_obj_t *objs;
    ancestor_type *array;
    int i, j, k, num_objs, num_ancestors;

    if ((num_objs = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU)) <= 0) {
        return;
    }

    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_objs * sizeof(hwloc_obj_t))) == NULL) {
        return;
    }

    for (i = 0; i < num_objs; i++) {
        objs[i] = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, i);
    }

    num_ancestors = num_objs * (num_objs - 1) / 2;

    if ((array =
         (ancestor_type *) MPIU_Malloc(num_ancestors * sizeof(ancestor_type))) == NULL) {
        return;
    }

    k = 0;
    for (i = 0; i < (num_objs - 1); i++) {
        for (j = i + 1; j < num_objs; j++) {
            array[k].obja = objs[i];
            array[k].objb = objs[j];
            array[k].ancestor = hwloc_get_common_ancestor_obj(topology, objs[i], objs[j]);
            k++;
        }
    }

    qsort(array, num_ancestors, sizeof(ancestor_type), cmpdepth_smt);

    for (i = 0; i < (num_ancestors - 1); i++) {
        if ((array[i + 1].ancestor)->depth < (array[i].ancestor)->depth) {
            break;
        }
    }

    qsort(array, (i + 1), sizeof(ancestor_type), cmparity_smt);

    *result = array[0].obja;

    MPIU_Free(objs);
    MPIU_Free(array);
    return;
}

static void get_first_socket_bunch(hwloc_obj_t * result, hwloc_obj_type_t binding_level)
{
    hwloc_obj_t *objs;
    ancestor_type *array;
    int i, j, k, num_objs, num_ancestors;

    if ((num_objs = hwloc_get_nbobjs_by_type(topology, binding_level)) <= 0) {
        return;
    }

    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_objs * sizeof(hwloc_obj_t))) == NULL) {
        return;
    }

    for (i = 0; i < num_objs; i++) {
        objs[i] = hwloc_get_obj_by_type(topology, binding_level, i);
    }

    num_ancestors = num_objs * (num_objs - 1) / 2;

    if ((array =
         (ancestor_type *) MPIU_Malloc(num_ancestors * sizeof(ancestor_type))) == NULL) {
        return;
    }

    k = 0;
    for (i = 0; i < (num_objs - 1); i++) {
        for (j = i + 1; j < num_objs; j++) {
            array[k].obja = objs[i];
            array[k].objb = objs[j];
            array[k].ancestor = hwloc_get_common_ancestor_obj(topology, objs[i], objs[j]);
            k++;
        }
    }

    qsort(array, num_ancestors, sizeof(ancestor_type), cmpdepth_smt);

    for (i = 0; i < (num_ancestors - 1); i++) {
        if ((array[i + 1].ancestor)->depth < (array[i].ancestor)->depth) {
            break;
        }
    }

    if (i < num_ancestors - 1)
        qsort(array, (i + 1), sizeof(ancestor_type), cmparity_smt);

    *result = array[0].obja;

    MPIU_Free(objs);
    MPIU_Free(array);
    return;
}

/*
 * Yields "scatter" affinity scenario in core_mapping.
 */
void map_scatter_core(int num_cpus)
{
    hwloc_obj_t *objs, obj, a;
    unsigned *pdist, maxd;
    int i, j, ix, jp, d, s;

    /* Init and load HWLOC_OBJ_PU objects */
    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_cpus * sizeof(hwloc_obj_t *))) == NULL)
        return;

    obj = NULL;
    i = 0;
    while ((obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_PU, obj)) != NULL)
        objs[i++] = obj;
    if (i != num_cpus) {
        MPIU_Free(objs);
        return;
    }

    /* Sort HWLOC_OBJ_PU objects according to sibling_rank */
    qsort(objs, num_cpus, sizeof(hwloc_obj_t *), cmpproc_smt);

    /* Init cumulative distances */
    if ((pdist = (unsigned *) MPIU_Malloc(num_cpus * sizeof(unsigned))) == NULL) {
        MPIU_Free(objs);
        return;
    }

    /* Loop over objects, ix is index in objs where sorted objects start */
    ix = num_cpus;
    s = -1;
    while (ix > 0) {
        /* If new group of SMT processors starts, zero distances */
        if (s != objs[0]->sibling_rank) {
            s = objs[0]->sibling_rank;
            for (j = 0; j < ix; j++)
                pdist[j] = 0;
        }
        /*
         * Determine object that has max. distance to all already stored objects.
         * Consider only groups of SMT processors with same sibling_rank.
         */
        maxd = 0;
        jp = 0;
        for (j = 0; j < ix; j++) {
            if ((j) && (objs[j - 1]->sibling_rank != objs[j]->sibling_rank))
                break;
            if (pdist[j] > maxd) {
                maxd = pdist[j];
                jp = j;
            }
        }

        /* Rotate found object to the end of the list, map out found object from distances */
        obj = objs[jp];
        for (j = jp; j < num_cpus - 1; j++) {
            objs[j] = objs[j + 1];
            pdist[j] = pdist[j + 1];
        }
        objs[j] = obj;
        ix--;

        /*
         * Update cumulative distances of all remaining objects with new stored one.
         * If two HWLOC_OBJ_PU objects don't share a common ancestor, the topology is broken.
         * Our scheme cannot be used in this case.
         */
        for (j = 0; j < ix; j++) {
            if ((a = hwloc_get_common_ancestor_obj(topology, obj, objs[j])) == NULL) {
                MPIU_Free(pdist);
                MPIU_Free(objs);
                return;
            }
            d = objs[j]->depth + obj->depth - 2 * a->depth;
            pdist[j] += d * d;
        }
    }

    /* Collect os_indexes into core_mapping */
    for (i = 0; i < num_cpus; i++) {
        core_mapping[i] = objs[i]->os_index;
    }

    MPIU_Free(pdist);
    MPIU_Free(objs);
    return;
}

void map_scatter_socket(int num_sockets, hwloc_obj_type_t binding_level)
{
    hwloc_obj_t *objs, obj, a;
    unsigned *pdist, maxd;
    int i, j, ix, jp, d, s, num_cores;

    /* Init and load HWLOC_OBJ_SOCKET or HWLOC_OBJ_NODE objects */
    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_sockets * sizeof(hwloc_obj_t *))) == NULL)
        return;

    if ((num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE)) <= 0) {
        return;
    }

    obj = NULL;
    i = 0;
    while ((obj = hwloc_get_next_obj_by_type(topology, binding_level, obj)) != NULL)
        objs[i++] = obj;
    if (i != num_sockets) {
        MPIU_Free(objs);
        return;
    }

    /* Sort HWLOC_OBJ_SOCKET or HWLOC_OBJ_NODE objects according to sibling_rank */
    qsort(objs, num_sockets, sizeof(hwloc_obj_t *), cmpproc_smt);

    /* Init cumulative distances */
    if ((pdist = (unsigned *) MPIU_Malloc(num_sockets * sizeof(unsigned))) == NULL) {
        MPIU_Free(objs);
        return;
    }

    /* Loop over objects, ix is index in objs where sorted objects start */
    ix = num_sockets;
    s = -1;
    while (ix > 0) {
        /* If new group of SMT processors starts, zero distances */
        if (s != objs[0]->sibling_rank) {
            s = objs[0]->sibling_rank;
            for (j = 0; j < ix; j++)
                pdist[j] = 0;
        }
        /*
         * Determine object that has max. distance to all already stored objects.
         * Consider only groups of SMT processors with same sibling_rank.
         */
        maxd = 0;
        jp = 0;
        for (j = 0; j < ix; j++) {
            if ((j) && (objs[j - 1]->sibling_rank != objs[j]->sibling_rank))
                break;
            if (pdist[j] > maxd) {
                maxd = pdist[j];
                jp = j;
            }
        }

        /* Rotate found object to the end of the list, map out found object from distances */
        obj = objs[jp];
        for (j = jp; j < num_sockets - 1; j++) {
            objs[j] = objs[j + 1];
            pdist[j] = pdist[j + 1];
        }
        objs[j] = obj;
        ix--;

        /*
         * Update cumulative distances of all remaining objects with new stored one.
         * If two HWLOC_OBJ_SOCKET or HWLOC_OBJ_NODE objects don't share a common ancestor, the topology is broken.
         * Our scheme cannot be used in this case.
         */
        for (j = 0; j < ix; j++) {
            if ((a = hwloc_get_common_ancestor_obj(topology, obj, objs[j])) == NULL) {
                MPIU_Free(pdist);
                MPIU_Free(objs);
                return;
            }
            d = objs[j]->depth + obj->depth - 2 * a->depth;
            pdist[j] += d * d;
        }
    }

    /* Collect os_indexes into core_mapping */
    for (i = 0, j = 0; i < num_cores; i++, j++) {
        if (j == num_sockets) {
            j = 0;
        }
        core_mapping[i] = hwloc_bitmap_to_ulong((hwloc_const_bitmap_t) (objs[j]->cpuset));
    }

    MPIU_Free(pdist);
    MPIU_Free(objs);
    return;
}

 /*
  * Yields "bunch" affinity scenario in core_mapping.
  */
void map_bunch_core(int num_cpus)
{
    hwloc_obj_t *objs, obj, a;
    unsigned *pdist, mind;
    int i, j, ix, jp, d, s, num_cores, num_pus;

    /* Init and load HWLOC_OBJ_PU objects */
    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_cpus * sizeof(hwloc_obj_t *))) == NULL)
        return;

    obj = NULL;
    i = 0;

    if ((num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE)) <= 0) {
        MPIU_Free(objs);
        return;
    }

    if ((num_pus = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU)) <= 0) {
        MPIU_Free(objs);
        return;
    }

    /* SMT Disabled */
    if (num_cores == num_pus) {

        get_first_obj_bunch(&obj);

        if (obj == NULL) {
            MPIU_Free(objs);
            return;
        }

        objs[i] = obj;
        i++;

        while ((obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_PU, obj)) != NULL) {
            objs[i] = obj;
            i++;
        }

        obj = NULL;
        while (i != num_cpus) {
            obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_PU, obj);
            objs[i++] = obj;
        }

        if (i != num_cpus) {
            MPIU_Free(objs);
            return;
        }

    } else {    /* SMT Enabled */

        while ((obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_PU, obj)) != NULL)
            objs[i++] = obj;

        if (i != num_cpus) {
            MPIU_Free(objs);
            return;
        }

        /* Sort HWLOC_OBJ_PU objects according to sibling_rank */
        qsort(objs, num_cpus, sizeof(hwloc_obj_t *), cmpproc_smt);
    }

    /* Init cumulative distances */
    if ((pdist = (unsigned *) MPIU_Malloc(num_cpus * sizeof(unsigned))) == NULL) {
        MPIU_Free(objs);
        return;
    }

    /* Loop over objects, ix is index in objs where sorted objects start */
    ix = num_cpus;
    s = -1;
    while (ix > 0) {
        /* If new group of SMT processors starts, zero distances */
        if (s != objs[0]->sibling_rank) {
            s = objs[0]->sibling_rank;
            for (j = 0; j < ix; j++)
                pdist[j] = UINT_MAX;
        }
        /*
         * Determine object that has min. distance to all already stored objects.
         * Consider only groups of SMT processors with same sibling_rank.
         */
        mind = UINT_MAX;
        jp = 0;
        for (j = 0; j < ix; j++) {
            if ((j) && (objs[j - 1]->sibling_rank != objs[j]->sibling_rank))
                break;
            if (pdist[j] < mind) {
                mind = pdist[j];
                jp = j;
            }
        }

        /* Rotate found object to the end of the list, map out found object from distances */
        obj = objs[jp];
        for (j = jp; j < num_cpus - 1; j++) {
            objs[j] = objs[j + 1];
            pdist[j] = pdist[j + 1];
        }
        objs[j] = obj;
        ix--;

        /*
         * Update cumulative distances of all remaining objects with new stored one.
         * If two HWLOC_OBJ_PU objects don't share a common ancestor, the topology is broken.
         * Our scheme cannot be used in this case.
         */
        for (j = 0; j < ix; j++) {
            if ((a = hwloc_get_common_ancestor_obj(topology, obj, objs[j])) == NULL) {
                MPIU_Free(pdist);
                MPIU_Free(objs);
                return;
            }
            d = objs[j]->depth + obj->depth - 2 * a->depth;
            pdist[j] += d * d;
        }
    }

    /* Collect os_indexes into core_mapping */
    for (i = 0; i < num_cpus; i++) {
        core_mapping[i] = objs[i]->os_index;
    }

    MPIU_Free(pdist);
    MPIU_Free(objs);
    return;
}

int check_num_child(hwloc_obj_t obj)
{
    int i = 0, k, num_cores;

    if ((num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE)) <= 0) {
        return 0;
    }

    for (k = 0; k < num_cores; k++) {
        if (hwloc_bitmap_isset((hwloc_const_bitmap_t) (obj->cpuset), k)) {
            i++;
        }
    }

    return i;
}

void map_bunch_socket(int num_sockets, hwloc_obj_type_t binding_level)
{
    hwloc_obj_t *objs, obj, a;
    unsigned *pdist, mind;
    int i, j, ix, jp, d, s, num_cores, num_pus;

    /* Init and load HWLOC_OBJ_PU objects */
    if ((objs = (hwloc_obj_t *) MPIU_Malloc(num_sockets * sizeof(hwloc_obj_t *))) == NULL)
        return;

    obj = NULL;
    i = 0;

    if ((num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE)) <= 0) {
        MPIU_Free(objs);
        return;
    }

    if ((num_pus = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU)) <= 0) {
        MPIU_Free(objs);
        return;
    }

    /* SMT Disabled */
    if (num_cores == num_pus) {

        get_first_socket_bunch(&obj, binding_level);

        if (obj == NULL) {
            MPIU_Free(objs);
            return;
        }

        objs[i] = obj;
        i++;

        while ((obj = hwloc_get_next_obj_by_type(topology, binding_level, obj)) != NULL) {
            objs[i] = obj;
            i++;
        }

        obj = NULL;
        while (i != num_sockets) {
            obj = hwloc_get_next_obj_by_type(topology, binding_level, obj);
            objs[i++] = obj;
        }

        if (i != num_sockets) {
            MPIU_Free(objs);
            return;
        }

    } else {    /* SMT Enabled */

        while ((obj = hwloc_get_next_obj_by_type(topology, binding_level, obj)) != NULL)
            objs[i++] = obj;

        if (i != num_sockets) {
            MPIU_Free(objs);
            return;
        }

        /* Sort HWLOC_OBJ_SOCKET or HWLOC_OBJ_NODE objects according to sibling_rank */
        qsort(objs, num_sockets, sizeof(hwloc_obj_t *), cmpproc_smt);

    }

    /* Init cumulative distances */
    if ((pdist = (unsigned *) MPIU_Malloc(num_sockets * sizeof(unsigned))) == NULL) {
        MPIU_Free(objs);
        return;
    }

    /* Loop over objects, ix is index in objs where sorted objects start */
    ix = num_sockets;
    s = -1;
    while (ix > 0) {
        /* If new group of SMT processors starts, zero distances */
        if (s != objs[0]->sibling_rank) {
            s = objs[0]->sibling_rank;
            for (j = 0; j < ix; j++)
                pdist[j] = UINT_MAX;
        }
        /*
         * Determine object that has min. distance to all already stored objects.
         * Consider only groups of SMT processors with same sibling_rank.
         */
        mind = UINT_MAX;
        jp = 0;
        for (j = 0; j < ix; j++) {
            if ((j) && (objs[j - 1]->sibling_rank != objs[j]->sibling_rank))
                break;
            if (pdist[j] < mind) {
                mind = pdist[j];
                jp = j;
            }
        }

        /* Rotate found object to the end of the list, map out found object from distances */
        obj = objs[jp];
        for (j = jp; j < num_sockets - 1; j++) {
            objs[j] = objs[j + 1];
            pdist[j] = pdist[j + 1];
        }
        objs[j] = obj;
        ix--;

        /*
         * Update cumulative distances of all remaining objects with new stored one.
         * If two HWLOC_OBJ_SOCKET or HWLOC_OBJ_NODE objects don't share a common ancestor, the topology is broken.
         * Our scheme cannot be used in this case.
         */
        for (j = 0; j < ix; j++) {
            if ((a = hwloc_get_common_ancestor_obj(topology, obj, objs[j])) == NULL) {
                MPIU_Free(pdist);
                MPIU_Free(objs);
                return;
            }
            d = objs[j]->depth + obj->depth - 2 * a->depth;
            pdist[j] += d * d;
        }
    }

    /* Collect os_indexes into core_mapping */
    int num_child_in_socket[num_sockets];

    for (i = 0; i < num_sockets; i++) {
        num_child_in_socket[i] = check_num_child(objs[i]);
    }

    for (i = 1; i < num_sockets; i++)
        num_child_in_socket[i] += num_child_in_socket[i - 1];

    for (i = 0, j = 0; i < num_cores; i++) {
        if (i == num_child_in_socket[j]) {
            j++;
        }
        core_mapping[i] = hwloc_bitmap_to_ulong((hwloc_const_bitmap_t) (objs[j]->cpuset));
    }

    MPIU_Free(pdist);
    MPIU_Free(objs);
    return;
}

static int num_digits(unsigned long numcpus)
{
    int n_digits = 0;
    while (numcpus > 0) {
        n_digits++;
        numcpus /= 10;
    }
    return n_digits;
}

int get_cpu_mapping_hwloc(long N_CPUs_online, hwloc_topology_t tp)
{
    unsigned topodepth = -1, depth = -1;
    int num_processes = 0, rc = 0, i;
    int num_sockets = 0;
    int num_numanodes = 0;
    int num_cpus = 0;
    char *s;
    struct dirent **namelist;
    pid_t pid;
    obj_attribute_type *tree = NULL;
    char *value;

    /* Determine topology depth */
    topodepth = hwloc_topology_get_depth(tp);
    if (topodepth == HWLOC_TYPE_DEPTH_UNKNOWN) {
        fprintf(stderr, "Warning: %s: Failed to determine topology depth.\n", __func__);
        return (topodepth);
    }

    /* Count number of (logical) processors */
    depth = hwloc_get_type_depth(tp, HWLOC_OBJ_PU);

    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
        fprintf(stderr, "Warning: %s: Failed to determine number of processors.\n",
                __func__);
        return (depth);
    }
    if ((num_cpus = hwloc_get_nbobjs_by_type(tp, HWLOC_OBJ_PU)) <= 0) {
        fprintf(stderr, "Warning: %s: Failed to determine number of processors.\n",
                __func__);
        return -1;
    }

    /* Count number of sockets */
    depth = hwloc_get_type_depth(tp, HWLOC_OBJ_SOCKET);
    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
        fprintf(stderr, "Warning: %s: Failed to determine number of sockets.\n",
                __func__);
        return (depth);
    } else {
        num_sockets = hwloc_get_nbobjs_by_depth(tp, depth);
    }

    /* Count number of numanodes */
    depth = hwloc_get_type_depth(tp, HWLOC_OBJ_NODE);
    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
        num_numanodes = -1;
    } else {
        num_numanodes = hwloc_get_nbobjs_by_depth(tp, depth);
    }

    if (s_cpu_mapping == NULL) {
        /* We need to do allocate memory for the custom_cpu_mapping array
         * and determine the current load on the different cpu's only
         * when the user has not specified a mapping string. If the user
         * has provided a mapping string, it overrides everything.
         */
        /*TODO: might need a better representation as number of cores per node increases */
        unsigned long long_max = ULONG_MAX;
        int n_digits = num_digits(long_max);
        custom_cpu_mapping =
            MPIU_Malloc(sizeof(char) * num_cpus * (n_digits + 1) + 1);
        if (custom_cpu_mapping == NULL) {
            goto error_free;
        }
        MPIU_Memset(custom_cpu_mapping, 0,
                    sizeof(char) * num_cpus * (n_digits + 1) + 1);
        core_mapping = (unsigned long *) MPIU_Malloc(num_cpus * sizeof(unsigned long));
        if (core_mapping == NULL) {
            goto error_free;
        }
        for (i = 0; i < num_cpus; i++) {
            core_mapping[i] = -1;
        }

        tree = MPIU_Malloc(num_cpus * topodepth * sizeof(obj_attribute_type));
        if (tree == NULL) {
            goto error_free;
        }
        for (i = 0; i < num_cpus * topodepth; i++) {
            tree[i].obj = NULL;
            tree[i].load = 0;
            CPU_ZERO(&(tree[i].cpuset));
        }

        if (!(obj_tree = (int *) MPIU_Malloc(num_cpus * topodepth * sizeof(*obj_tree)))) {
            goto error_free;
        }
        for (i = 0; i < num_cpus * topodepth; i++) {
            obj_tree[i] = -1;
        }

        ip = 0;

        /* MV2_ENABLE_LEASTLOAD: map_bunch/scatter or map_bunch/scatter_load */
        if ((value = getenv("MV2_ENABLE_LEASTLOAD")) != NULL) {
            mv2_enable_leastload = atoi(value);
            if (mv2_enable_leastload != 1) {
                mv2_enable_leastload = 0;
            }
        }

        /* MV2_ENABLE_LEASTLOAD=1, map_bunch_load or map_scatter_load is used */
        if (mv2_enable_leastload == 1) {
            /*
             * Get all processes' pid and cpuset.
             * Get numanode, socket, and core current load according to processes running on it.
             */
            num_processes = scandir("/proc", &namelist, pid_filter, alphasort);
            if (num_processes < 0) {
                fprintf(stderr, "Warning: %s: Failed to scandir /proc.\n", __func__);
                return -1;
            } else {
                int status;
                cpu_set_t pid_cpuset;
                CPU_ZERO(&pid_cpuset);

                /* Get cpuset for each running process. */
                for (i = 0; i < num_processes; i++) {
                    pid = atol(namelist[i]->d_name);
                    status = sched_getaffinity(pid, sizeof(pid_cpuset), &pid_cpuset);
                    /* Process completed. */
                    if (status < 0) {
                        continue;
                    }
                    cac_load(tree, pid_cpuset);
                }
                while (num_processes--) {
                    MPIU_Free(namelist[num_processes]);
                }
                MPIU_Free(namelist);
            }

            if (policy == POLICY_SCATTER) {
                map_scatter_load(tree);
            } else if (policy == POLICY_BUNCH) {
                map_bunch_load(tree);
            } else {
                goto error_free;
            }
        } else {
            /* MV2_ENABLE_LEASTLOAD != 1 or MV2_ENABLE_LEASTLOAD == NULL, map_bunch or map_scatter is used */
            if (policy == POLICY_SCATTER) {
                /* Scatter */
                hwloc_obj_type_t binding_level = HWLOC_OBJ_SOCKET;
                if (level == LEVEL_SOCKET) {
                    map_scatter_socket(num_sockets, binding_level);
                } else if (level == LEVEL_NUMANODE) {
                    if (num_numanodes == -1) {
                        /* There is not numanode, fallback to socket */
                        map_scatter_socket(num_sockets, binding_level);
                    } else {
                        binding_level = HWLOC_OBJ_NODE;
                        map_scatter_socket(num_numanodes, binding_level);
                    }
                } else {
                    map_scatter_core(num_cpus);
                }

            } else if (policy == POLICY_BUNCH) {
                /* Bunch */
                hwloc_obj_type_t binding_level = HWLOC_OBJ_SOCKET;
                if (level == LEVEL_SOCKET) {
                    map_bunch_socket(num_sockets, binding_level);
                } else if (level == LEVEL_NUMANODE) {
                    if (num_numanodes == -1) {
                        /* There is not numanode, fallback to socket */
                        map_bunch_socket(num_sockets, binding_level);
                    } else {
                        binding_level = HWLOC_OBJ_NODE;
                        map_bunch_socket(num_numanodes, binding_level);
                    }
                } else {
                    map_bunch_core(num_cpus);
                }
            } else {
                goto error_free;
            }
        }

        /* Assemble custom_cpu_mapping string */
        s = custom_cpu_mapping;
        for (i = 0; i < num_cpus; i++) {
            sprintf(s, "%lu:", core_mapping[i]);
            s = custom_cpu_mapping + strlen(custom_cpu_mapping);
        }
        i = strlen(custom_cpu_mapping);
        if (i) {
            custom_cpu_mapping[i - 1] = '\0';
        }
    }

    /* Done */
    rc = MPI_SUCCESS;

  error_free:
    if (core_mapping != NULL) {
        MPIU_Free(core_mapping);
    }
    if (tree != NULL) {
        MPIU_Free(tree);
    }
    if (obj_tree) {
        MPIU_Free(obj_tree);
    }

    MPIU_DBG_MSG_FMT(OTHER, TYPICAL,
                     (MPIU_DBG_FDEST,
                      "num_cpus=%d, num_sockets=%d, custom_cpu_mapping=\"%s\"", num_cpus,
                      num_sockets, custom_cpu_mapping));

    return rc;
}


int get_cpu_mapping(long N_CPUs_online)
{
    char line[MAX_LINE_LENGTH];
    char input[MAX_NAME_LENGTH];
    char bogus1[MAX_NAME_LENGTH];
    char bogus2[MAX_NAME_LENGTH];
    char bogus3[MAX_NAME_LENGTH];
    int physical_id;            //return value
    int mapping[N_CPUs_online];
    int core_index = 0;
    cpu_type_t cpu_type = 0;
    int model;
    int vendor_set = 0, model_set = 0, num_cpus = 0;

    FILE *fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        printf("can not open cpuinfo file \n");
        return 0;
    }

    MPIU_Memset(mapping, 0, sizeof(mapping));
    custom_cpu_mapping = (char *) MPIU_Malloc(sizeof(char) * N_CPUs_online * 2);
    if (custom_cpu_mapping == NULL) {
        return 0;
    }
    MPIU_Memset(custom_cpu_mapping, 0, sizeof(char) * N_CPUs_online * 2);

    while (!feof(fp)) {
        MPIU_Memset(line, 0, MAX_LINE_LENGTH);
        fgets(line, MAX_LINE_LENGTH, fp);

        MPIU_Memset(input, 0, MAX_NAME_LENGTH);
        sscanf(line, "%s", input);

        if (!vendor_set) {
            if (strcmp(input, "vendor_id") == 0) {
                MPIU_Memset(input, 0, MAX_NAME_LENGTH);
                sscanf(line, "%s%s%s", bogus1, bogus2, input);

                if (strcmp(input, "AuthenticAMD") == 0) {
                    cpu_type = CPU_FAMILY_AMD;
                } else {
                    cpu_type = CPU_FAMILY_INTEL;
                }
                vendor_set = 1;
            }
        }

        if (!model_set) {
            if (strcmp(input, "model") == 0) {
                sscanf(line, "%s%s%d", bogus1, bogus2, &model);
                model_set = 1;
            }
        }

        if (strcmp(input, "physical") == 0) {
            sscanf(line, "%s%s%s%d", bogus1, bogus2, bogus3, &physical_id);
            mapping[core_index++] = physical_id;
        }
    }

    num_cpus = core_index;
    if (num_cpus == 4) {
        if ((memcmp(INTEL_XEON_DUAL_MAPPING, mapping, sizeof(int) * num_cpus) == 0)
            && (cpu_type == CPU_FAMILY_INTEL)) {
            strcpy(custom_cpu_mapping, "0:2:1:3");
        } else
            if ((memcmp(AMD_OPTERON_DUAL_MAPPING, mapping, sizeof(int) * num_cpus) == 0)
                && (cpu_type == CPU_FAMILY_AMD)) {
            strcpy(custom_cpu_mapping, "0:1:2:3");
        }
    } else if (num_cpus == 8) {
        if (cpu_type == CPU_FAMILY_INTEL) {
            if (model == CLOVERTOWN_MODEL) {
                if (memcmp(INTEL_CLOVERTOWN_MAPPING, mapping, sizeof(int) * num_cpus) ==
                    0) {
                    strcpy(custom_cpu_mapping, "0:1:4:5:2:3:6:7");
                }
            } else if (model == HARPERTOWN_MODEL) {
                if (memcmp(INTEL_HARPERTOWN_LEG_MAPPING, mapping, sizeof(int) * num_cpus)
                    == 0) {
                    strcpy(custom_cpu_mapping, "0:1:4:5:2:3:6:7");
                } else
                    if (memcmp
                        (INTEL_HARPERTOWN_COM_MAPPING, mapping,
                         sizeof(int) * num_cpus) == 0) {
                    strcpy(custom_cpu_mapping, "0:4:2:6:1:5:3:7");
                }
            } else if (model == NEHALEM_MODEL) {
                if (memcmp(INTEL_NEHALEM_LEG_MAPPING, mapping, sizeof(int) * num_cpus) ==
                    0) {
                    strcpy(custom_cpu_mapping, "0:2:4:6:1:3:5:7");
                } else
                    if (memcmp(INTEL_NEHALEM_COM_MAPPING, mapping, sizeof(int) * num_cpus)
                        == 0) {
                    strcpy(custom_cpu_mapping, "0:4:1:5:2:6:3:7");
                }
            }
        }
    } else if (num_cpus == 16) {
        if (cpu_type == CPU_FAMILY_INTEL) {
            if (model == NEHALEM_MODEL) {
                if (memcmp(INTEL_NEHALEM_LEG_MAPPING, mapping, sizeof(int) * num_cpus) ==
                    0) {
                    strcpy(custom_cpu_mapping, "0:2:4:6:1:3:5:7:8:10:12:14:9:11:13:15");
                } else
                    if (memcmp(INTEL_NEHALEM_COM_MAPPING, mapping, sizeof(int) * num_cpus)
                        == 0) {
                    strcpy(custom_cpu_mapping, "0:4:1:5:2:6:3:7:8:12:9:13:10:14:11:15");
                }
            }
        } else if (cpu_type == CPU_FAMILY_AMD) {
            if (memcmp(AMD_BARCELONA_MAPPING, mapping, sizeof(int) * num_cpus) == 0) {
                strcpy(custom_cpu_mapping, "0:1:2:3:4:5:6:7:8:9:10:11:12:13:14:15");
            }
        }
    }
    fclose(fp);

    return MPI_SUCCESS;
}

int get_socket_id (int ib_socket, int cpu_socket, int num_sockets,
        tab_socket_t * tab_socket)
{
    extern int rdma_local_id, rdma_num_hcas;

    int rdma_num_proc_per_hca;
    int offset_id;
    int j;
    int socket_id = ib_socket;
    int delta = cpu_socket / tab_socket[ib_socket].num_hca;

    rdma_num_proc_per_hca = rdma_num_local_procs / rdma_num_hcas;

    if (rdma_num_local_procs % rdma_num_hcas) {
        rdma_num_proc_per_hca++;
    }

    offset_id = rdma_local_id % rdma_num_proc_per_hca;

    if (offset_id < delta) {
        return ib_socket;
    }

    for (j = 0; j < num_sockets - 1; j++) {
        socket_id = tab_socket[ib_socket].closest[j];

        if (tab_socket[socket_id].num_hca == 0) {
            offset_id -= delta;

            if (offset_id < delta) {
                return socket_id;
            }
        }
    }

    /*
     * Couldn't find a free socket, spread remaining processes
     */
    return rdma_local_id % num_sockets;
}

#undef FUNCNAME
#define FUNCNAME smpi_setaffinity
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int smpi_setaffinity(int my_local_id)
{
    int cpu_socket;
    int selected_ib;
    int selected_socket;
    int mpi_errno = MPI_SUCCESS;
    tab_socket_t * tab_socket;

    hwloc_cpuset_t cpuset;
    MPIDI_STATE_DECL(MPID_STATE_SMPI_SETAFFINITY);
    MPIDI_FUNC_ENTER(MPID_STATE_SMPI_SETAFFINITY);

    mpi_errno = hwloc_topology_init(&topology);
    hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_IO_DEVICES);
    if (mpi_errno != 0) {
        mv2_enable_affinity = 0;
    }

    if (mv2_enable_affinity > 0) {
        hwloc_topology_load(topology);
        cpuset = hwloc_bitmap_alloc();
        if (s_cpu_mapping) {
            /* If the user has specified how to map the processes,
             * use it */
            char *tp = s_cpu_mapping;
            char *cp = NULL;
            int j = 0;
            int i;
            char tp_str[s_cpu_mapping_line_max + 1];
            long N_CPUs_online = sysconf(_SC_NPROCESSORS_ONLN);

            if (N_CPUs_online < 1) {
                MPIU_ERR_SETFATALANDJUMP2(mpi_errno,
                                          MPI_ERR_OTHER,
                                          "**fail", "%s: %s", "sysconf",
                                          strerror(errno));
            }

            /* Call the cpu_mapping function to find out about how the
             * processors are numbered on the different sockets.
             * The hardware information gathered from this function
             * is required to determine the best set of intra-node thresholds.
             * However, since the user has specified a mapping pattern,
             * we are not going to use any of our proposed binding patterns
             */

            mpi_errno = get_cpu_mapping_hwloc(N_CPUs_online, topology);

            while (*tp != '\0') {
                i = 0;
                cp = tp;

                while (*cp != '\0' && *cp != ':' && i < s_cpu_mapping_line_max) {
                    ++cp;
                    ++i;
                }

                strncpy(tp_str, tp, i);
                if (atoi(tp) < 0 || atoi(tp) >= N_CPUs_online) {
                    fprintf(stderr,
                            "Warning! : Core id %d does not exist on this architecture! \n",
                            atoi(tp));
                    fprintf(stderr, "CPU Affinity is undefined \n");
                    mv2_enable_affinity = 0;
                    MPIU_Free(s_cpu_mapping);
                    goto fn_fail;
                }
                tp_str[i] = '\0';

                if (j == my_local_id) {
                    // parsing of the string
                    char *token = tp_str;
                    int cpunum = 0;
                    while (*token != '\0') {
                        if (isdigit(*token)) {
                            cpunum = first_num_from_str(&token);
                            if (cpunum >= N_CPUs_online) {
                                fprintf(stderr,
                                        "Warning! : Core id %d does not exist on this architecture! \n",
                                        cpunum);
                                fprintf(stderr, "CPU Affinity is undefined \n");
                                mv2_enable_affinity = 0;
                                MPIU_Free(s_cpu_mapping);
                                goto fn_fail;
                            }
                            hwloc_bitmap_set(cpuset, cpunum);
                        } else if (*token == ',') {
                            token++;
                        } else if (*token == '-') {
                            token++;
                            if (!isdigit(*token)) {
                                fprintf(stderr,
                                        "Warning! : Core id %c does not exist on this architecture! \n",
                                        *token);
                                fprintf(stderr, "CPU Affinity is undefined \n");
                                mv2_enable_affinity = 0;
                                MPIU_Free(s_cpu_mapping);
                                goto fn_fail;
                            } else {
                                int cpuend = first_num_from_str(&token);
                                if (cpuend >= N_CPUs_online || cpuend < cpunum) {
                                    fprintf(stderr,
                                            "Warning! : Core id %d does not exist on this architecture! \n",
                                            cpuend);
                                    fprintf(stderr, "CPU Affinity is undefined \n");
                                    mv2_enable_affinity = 0;
                                    MPIU_Free(s_cpu_mapping);
                                    goto fn_fail;
                                }
                                int cpuval;
                                for (cpuval = cpunum + 1; cpuval <= cpuend; cpuval++)
                                    hwloc_bitmap_set(cpuset, cpuval);
                            }
                        } else if (*token != '\0') {
                            fprintf(stderr,
                                    "Warning! Error parsing the given CPU mask! \n");
                            fprintf(stderr, "CPU Affinity is undefined \n");
                            mv2_enable_affinity = 0;
                            MPIU_Free(s_cpu_mapping);
                            goto fn_fail;
                        }
                    }
                    // then attachement
                    hwloc_set_cpubind(topology, cpuset, 0);
                    break;
                }

                if (*cp == '\0') {
                    break;
                }

                tp = cp;
                ++tp;
                ++j;
            }

            MPIU_Free(s_cpu_mapping);
            s_cpu_mapping = NULL;
        } else {
            /* The user has not specified how to map the processes,
             * use the data available in /proc/cpuinfo file to decide
             * on the best cpu mapping pattern
             */
            long N_CPUs_online = sysconf(_SC_NPROCESSORS_ONLN);

            if (N_CPUs_online < 1) {
                MPIU_ERR_SETFATALANDJUMP2(mpi_errno,
                                          MPI_ERR_OTHER,
                                          "**fail", "%s: %s", "sysconf", strerror(errno)
);
            }

            /* Call the cpu_mapping function to find out about how the
             * processors are numbered on the different sockets.
             */
            mpi_errno = get_cpu_mapping_hwloc(N_CPUs_online, topology);
            if (mpi_errno != MPI_SUCCESS) {
                /* In case, we get an error from the hwloc mapping function */
                mpi_errno = get_cpu_mapping(N_CPUs_online);
            }

            if (mpi_errno != MPI_SUCCESS || custom_cpu_mapping == NULL) {
                /* For some reason, we were not able to retrieve the cpu mapping
                 * information. We are falling back on the linear mapping.
                 * This may not deliver the best performace
                 */
                hwloc_bitmap_only(cpuset, my_local_id % N_CPUs_online);
                hwloc_set_cpubind(topology, cpuset, 0);
            } else {
                /*
                 * We have all the information that we need. We will bind the
                 * processes to the cpu's now
                 */
                int linelen = strlen(custom_cpu_mapping);
                char tp_str[custom_cpu_mapping_line_max + 1];
                char *tp = custom_cpu_mapping;
                char *cp = NULL;
                int i, j = 0, k;

                if (!SMP_ONLY) {
                    int num_cpus = hwloc_get_nbobjs_by_type(topology,
                            HWLOC_OBJ_PU);
                    int depth_sockets = hwloc_get_type_depth(topology,
                            HWLOC_OBJ_SOCKET);
                    int num_sockets = hwloc_get_nbobjs_by_depth(topology,
                            depth_sockets);

                    if (linelen < custom_cpu_mapping_line_max) {
                        custom_cpu_mapping_line_max = linelen;
                    }

                    cpu_socket = num_cpus/num_sockets;

                    /*
                     * Make selected_ib global or make this section a function
                     */
                    if (FIXED_MAPPING == rdma_rail_sharing_policy) {
                        selected_ib = rdma_process_binding_rail_offset /
                            rdma_num_rails_per_hca;
                    }

                    else {
                        selected_ib = 0;
                    }

                    tab_socket = (tab_socket_t*)MPIU_Malloc(num_sockets *
                            sizeof(tab_socket_t));
                    if (NULL == tab_socket) {
                        fprintf(stderr, "could not allocate the socket "
                                "table\n");
                    }

                    for (i = 0; i < num_sockets; i++) {
                        tab_socket[i].num_hca = 0;

                        for(k = 0; k < num_sockets; k++) {
                            tab_socket[i].closest[k] = -1;
                        }
                    }

                    for (i = 0; i < rdma_num_hcas; i++) {
                        struct ibv_device * ibdev = mv2_MPIDI_CH3I_RDMA_Process.ib_dev[i];
                        tab_socket[get_ib_socket(ibdev)].num_hca++;
                    }

                    hwloc_obj_t obj_src;
                    hwloc_obj_t objs[num_sockets];
                    char string[20];

                    for (i = 0; i < num_sockets; i++) {
                        obj_src = hwloc_get_obj_by_type(topology,
                                HWLOC_OBJ_SOCKET,i);
                        hwloc_get_closest_objs(topology, obj_src,
                                (hwloc_obj_t *)&objs, num_sockets - 1);

                        for (k = 0; k < num_sockets - 1; k++) {
                            hwloc_obj_snprintf(string, sizeof(string),
                                    topology, objs[k], "#", 1);
                            tab_socket[i].closest[k] = atoi(&string[7]);
                        }
                    }

                    /*
                     * Make this information available globally
                     */
                    ib_socket_bind = get_ib_socket(
                                mv2_MPIDI_CH3I_RDMA_Process.ib_dev[selected_ib]);
                    i = ib_socket_bind;

                    selected_socket = get_socket_id(ib_socket_bind, cpu_socket,
                            num_sockets, tab_socket);
                    MPIU_Free(tab_socket);
                }

                while (*tp != '\0') {
                    i = 0;
                    cp = tp;

                    while (*cp != '\0' && *cp != ':' && i < custom_cpu_mapping_line_max) {
                        ++cp;
                        ++i;
                    }

                    strncpy(tp_str, tp, i);
                    tp_str[i] = '\0';

                    if (j == my_local_id) {
                        if (level == LEVEL_CORE) {
                            if (SMP_ONLY) {
                                hwloc_bitmap_only(cpuset, atol(tp_str));
                            } else {
                                hwloc_bitmap_only(cpuset,
                                        (atol(tp_str) % cpu_socket)
                                        + (selected_socket * cpu_socket));
                            }
                        } else {
                            if (SMP_ONLY) {
                                hwloc_bitmap_from_ulong(cpuset, atol(tp_str));
                            } else {
                                hwloc_bitmap_from_ulong(cpuset,
                                        (atol(tp_str) % cpu_socket)
                                        + (selected_socket * cpu_socket));
                            }
                        }
                        hwloc_set_cpubind(topology, cpuset, 0);
                        break;
                    }

                    if (*cp == '\0') {
                        break;
                    }

                    tp = cp;
                    ++tp;
                    ++j;
                }
            }

            MPIU_Free(custom_cpu_mapping);
        }
    }

  fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_SMPI_SETAFFINITY);
    return mpi_errno;

  fn_fail:
    goto fn_exit;
}

void mv2_show_cpu_affinity(MPIDI_PG_t * pg)
{
    int i, j, num_cpus, my_rank, pg_size;
    int mpi_errno = MPI_SUCCESS, errflag = 0;
    char buf[512];
    cpu_set_t *allproc_cpu_set;
    MPID_Comm *comm_world = NULL;
    MPIDI_VC_t *vc = NULL;

    comm_world = MPIR_Process.comm_world;
    pg_size = comm_world->local_size;
    my_rank = comm_world->rank;

    allproc_cpu_set = (cpu_set_t *) MPIU_Malloc(sizeof(cpu_set_t) * pg_size);
    CPU_ZERO(&allproc_cpu_set[my_rank]);
    sched_getaffinity(0, sizeof(cpu_set_t), &allproc_cpu_set[my_rank]);

    mpi_errno = MPIR_Allgather_impl(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, allproc_cpu_set,
                                    sizeof(cpu_set_t), MPI_BYTE, comm_world, &errflag);
    if (mpi_errno != MPI_SUCCESS) {
        fprintf(stderr, "MPIR_Allgather_impl returned error");
        return;
    }
    if (my_rank == 0) {
        num_cpus = sysconf(_SC_NPROCESSORS_CONF);
        fprintf(stderr, "-------------CPU AFFINITY-------------\n");
        for (i = 0; i < pg_size; i++) {
            MPIDI_Comm_get_vc(comm_world, i, &vc);
            if (vc->smp.local_rank != -1) {
                MPIU_Memset(buf, 0, sizeof(buf));
                for (j = 0; j < num_cpus; j++) {
                    if (CPU_ISSET(j, &allproc_cpu_set[vc->pg_rank])) {
                        sprintf((char *) (buf + strlen(buf)), "%3d", j);
                    }
                }
                fprintf(stderr, "RANK:%d  CPU_SET: %s\n", i, buf);
            }
        }
        fprintf(stderr, "-------------------------------------\n");
    }
    MPIU_Free(allproc_cpu_set);
}
