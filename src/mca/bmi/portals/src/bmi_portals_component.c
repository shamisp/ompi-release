/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"
#include "portals_config.h"

#include <sys/types.h>
#include <unistd.h>

#include "include/constants.h"

#include "util/output.h"
#include "threads/thread.h"

#include "bmi_portals.h"
#include "bmi_portals_compat.h"


mca_bmi_portals_component_t mca_bmi_portals_component = {
    {
      /* First, the mca_base_module_t struct containing meta
         information about the module itself */
      {
        /* Indicate that we are a pml v1.0.0 module (which also
           implies a specific MCA version) */

        MCA_BMI_BASE_VERSION_1_0_0,

        "portals", /* MCA module name */
        1,  /* MCA module major version */
        0,  /* MCA module minor version */
        0,  /* MCA module release version */
        mca_bmi_portals_component_open,  /* module open */
        mca_bmi_portals_component_close  /* module close */
      },
      
      /* Next the MCA v1.0.0 module meta data */
      
      {
        /* Whether the module is checkpointable or not */
        
        false
      },
      
      mca_bmi_portals_component_init,  
      mca_bmi_portals_component_progress,
    }
};


static ompi_output_stream_t portals_output_stream = {
    true,  /* is debugging */
    0,     /* verbose level */
    0,     /* want syslog */
    0,     /* syslog priority */
    NULL,  /* syslog ident */
    NULL,  /* prefix */
    true, /* want stdout */
    false,  /* want stderr */
    false, /* want file */
    false, /* file append */
    "bmi-portals"   /* file suffix */
};


static inline char*
param_register_string(const char* param_name, 
                                 const char* default_value)
{
    char *param_value;
    int id = mca_base_param_register_string("bmi", "portals",
                                            param_name, NULL, 
                                            default_value);
    mca_base_param_lookup_string(id, &param_value);
    return param_value;
}


static inline int
param_register_int(const char* param_name, 
                              int default_value)
{
    int id = mca_base_param_register_int("bmi", "portals", param_name,
                                         NULL, default_value);
    int param_value = default_value;
    mca_base_param_lookup_int(id, &param_value);
    return param_value;
}



int
mca_bmi_portals_component_open(void)
{
    int i;

    /* initialize component state */
    mca_bmi_portals_component.portals_num_modules = 0;
    mca_bmi_portals_component.portals_modules = NULL;

    /* initalize component objects */
    OBJ_CONSTRUCT(&mca_bmi_portals_component.portals_lock,
                  ompi_mutex_t);

    /* get configured state for component */
#if PTL_PORTALS_UTCP
    mca_ptl_portals_component.portals_ifname = 
        param_register_string("ifname", "eth0");
#endif
    portals_output_stream.lds_verbose_level = 
        param_register_int("debug_level",
                           PTL_PORTALS_DEFAULT_DEBUG_LEVEL);

    mca_ptl_portals_component.portals_free_list_init_num = 
        param_register_int("free_list_init_num",
                           PTL_PORTALS_DEFAULT_FREE_LIST_INIT_NUM);
    mca_ptl_portals_component.portals_free_list_max_num = 
        param_register_int("free_list_max_num",
                           PTL_PORTALS_DEFAULT_FREE_LIST_MAX_NUM);
    mca_ptl_portals_component.portals_free_list_inc_num = 
        param_register_int("free_list_inc_num",
                           PTL_PORTALS_DEFAULT_FREE_LIST_INC_NUM);

    /* start up debugging output */
    asprintf(&(portals_output_stream.lds_prefix), 
             "bmi: portals (%5d): ", getpid());

    mca_bmi_portals_component.portals_output = 
        ompi_output_open(&portals_output_stream);

    /* fill default module state */
    mca_ptl_portals_module.super.bmi_flags = MCA_BMI_FLAGS_RDMA;

    for (i = 0 ; i < MCA_BMI_TAG_MAX ; ++i) {
        mca_bmi_portals_module.portals_reg = NULL;
    }

    for (i = 0 ; i < MCA_BMI_PORTALS_EQ_SIZE ; ++i) {
        mca_bmi_portals_module.portals_eq_sizes[i] = 0;
        mca_bmi_portals_module.portals_eq_handles[i] = PTL_EQ_NONE;
    }

    mca_bmi_portals_module.portals_ni_h = PTL_INVALID_HANDLE;
    mca_bmi_portals_module.portals_sr_dropped = 0;

    /* get configured state for default module */
    mca_ptl_portals_module.super.bmi_eager_limit = 
        param_register_int("eager_limit",
                           PTL_PORTALS_DEFAULT_EAGER_LIMIT);
    mca_ptl_portals_module.super.bmi_min_send_size = 
        param_register_int("min_send_size",
                           PTL_PORTALS_DEFAULT_MIN_SEND_SIZE);
    mca_ptl_portals_module.super.bmi_max_send_size = 
        param_register_int("max_send_size",
                           PTL_PORTALS_DEFAULT_MAX_SEND_SIZE);
    mca_ptl_portals_module.super.bmi_min_rdma_size = 
        param_register_int("min_rdma_size",
                           PTL_PORTALS_DEFAULT_MIN_RDMA_SIZE);
    mca_ptl_portals_module.super.bmi_max_rdma_size = 
        param_register_int("max_rdma_size",
                           PTL_PORTALS_DEFAULT_MAX_RDMA_SIZE);
    mca_ptl_portals_module.super.bmi_exclusivity = 
        param_register_int("exclusivity", 60);
    mca_ptl_portals_module.super.bmi_latency = 
        param_register_int("latency", 0);
    mca_ptl_portals_module.super.bmi_bandwidth = 
        param_register_int("bandwidth", 1000);

    return OMPI_SUCCESS;
}


int
mca_bmi_portals_component_close(void)
{
    /* release resources */
    OBJ_DESTRUCT(&mca_bmi_portals_component.portals_lock);

    if (NULL != mca_bmi_portals_component.portals_modules) {
        free(mca_bmi_portals_component.portals_modules);
    }

    if (NULL != mca_bmi_portals_component.portals_ifname) {
        free(mca_bmi_portals_component.portals_ifname);
    }

    if (NULL != portals_output_stream.lds_prefix) {
        free(portals_output_stream.lds_prefix);
    }

    ompi_output_close(mca_bmi_portals_component.portals_output);
    mca_bmi_portals_component.portals_output = -1;

    return OMPI_SUCCESS;
}


mca_bmi_base_module_t**
mca_bmi_portals_component_init(int *num_bmis, 
                               bool enable_progress_threads,
                               bool enable_mpi_threads)
{
    mca_bmi_base_module_t** bmis;
    *num_bmis = 0;
    int i;
    uint32_t length;

    if (enable_progress_threads) {
        ompi_output_verbose(20, mca_bmi_portals_component.portals_output,
                            "disabled because progress threads enabled");
        return NULL;
    }

    /* initialize portals bmi.  note that this is in the compat code because
       it's fairly non-portable between implementations */
    if (OMPI_SUCCESS != mca_bmi_portals_init(&mca_bmi_portals_component)) {
        ompi_output_verbose(20, mca_bmi_portals_component.portals_output,
                            "disabled because compatibility init failed");
        return NULL;
    }

    bmis = malloc(mca_bmi_portals_component.portals_num_modules *
                  sizeof(mca_bmi_portals_module_t*));
    for (i = 0 ; i < mca_bmi_portals_component.portals_num_modules ; ++i) {
        bmis[i] = (mca_bmi_base_module_t*) 
            (mca_bmi_portals_component.portals_modules + i);

        OBJ_CONSTRUCT(&bmis[i]->portals_frag_eager, ompi_free_list_t);
        OBJ_CONSTRUCT(&bmis[i]->portals_frag_max, ompi_free_list_t);
        OBJ_CONSTRUCT(&bmis[i]->portals_frag_user, ompi_free_list_t);

        /* eager frags */
        ompi_free_list_init(&(bmis[i].send_free_eager),
                            sizeof(mca_bmi_portals_bmis[i].super.bmi_eager_limit,
                            OBJ_CLASS(mca_bmi_portals_send_frag_eager_t),
                            mca_bmi_portals_component.portals_free_list_init_num,
                            mca_bmi_portals_component.portals_free_list_max_num,
                            mca_bmi_portals_component.portals_free_list_inc_num);

        /* send frags */
        ompi_free_list_init(&(bmis[i].send_free_eager),
                            bmis[i].super.bmi_max_sender_size,
                            OBJ_CLASS(mca_bmi_portals_send_frag_eager_t),
                            mca_bmi_portals_component.portals_free_list_init_num,
                            mca_bmi_portals_component.portals_free_list_max_num,
                            mca_bmi_portals_component.portals_free_list_inc_num);

        /* user frags */
        ompi_free_list_init(&(bmis[i].send_free_eager),
                            bmis[i].super.bmi_max_sender_size,
                            OBJ_CLASS(mca_bmi_portals_send_frag_eager_t),
                            mca_bmi_portals_component.portals_free_list_init_num,
                            mca_bmi_portals_component.portals_free_list_max_num,
                            mca_bmi_portals_component.portals_free_list_inc_num);
                            

    }
    *num_bmis = mca_bmi_portals_component.portals_num_modules;

    ompi_output_verbose(20, mca_bmi_portals_component.portals_output,
                        "initialized %d modules",
                        *num_bmis);

    return bmis;
}


int
mca_bmi_portals_component_progress(void)
{
    int num_progressed = 0;
    size_t i;

    for (i = 0 ; i < mca_bmi_portals_component.portals_num_modules ; ++i) {
        struct mca_bmi_portals_module_t *module = 
            mca_bmi_portals_component.portals_modules[i];
        ptl_event_t ev;
        ptl_sr_value_t numdropped;
        int which;
        int ret;

        if (module->eq_handles[MCA_BMI_PORTALS_EQ_SIZE - 1] == 
            PTL_EQ_NONE) continue; /* they are all initialized at once */

#if OMPI_ENABLE_DEBUG
        /* BWB - this is going to kill performance */
        PtlNIStatus(module->ni_handle,
                    PTL_SR_DROP_COUNT,
                    &numdropped);
        if (numdropped != module->dropped) {
            ompi_output_verbose(30, mca_bmi_portals_component.portals_output,
                                "*** Dropped message count changed.  %lld, %lld",
                                module->dropped, numdropped);
            module->dropped = numdropped;
        }
#endif

        ret = PtlEQPoll(module->eq_handles,
                        MCA_BMI_PORTALS_EQ_SIZE, /* number of eq handles */
                        10,
                        &ev,
                        &which);
        if (PTL_EQ_EMPTY == ret) {
            /* nothing to see here - move along */
            continue;
        } else if (!(PTL_OK == ret || PTL_EQ_DROPPED == ret)) {
            /* BWB - how can we report errors? */
            ompi_output(mca_bmi_portals_component.portals_output,
                        "Error calling PtlEQGet: %d", ret);
            continue;
        } else if (PTL_EQ_DROPPED == ret) {
            ompi_output_verbose(10, mca_bmi_portals_component.portals_output,
                                "*** Event queue entries were dropped");
        }

#if BMI_PORTALS_HAVE_EVENT_UNLINK
        /* not everyone has UNLINK.  Use it only to print the event,
           so we can make sure we properly re-initialize the ones that
           need to be re-initialized */
        if (PTL_EVENT_UNLINK == ev.type) {
            OMPI_OUTPUT_VERBOSE((100, mca_bmi_portals_component.portals_output,
                                 "unlink event occurred"));
            continue;
        }
#endif

        num_progressed++;
    }

    return num_progressed;
}

