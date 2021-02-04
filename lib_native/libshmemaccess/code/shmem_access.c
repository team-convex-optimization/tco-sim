#include <stdlib.h>

#include <sys/mman.h>

#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "tco_libd.h"
#include "tco_shmem.h"

#include "shmem_access.h"

extern godot_gdnative_core_api_struct *api;

int log_level = LOG_INFO | LOG_ERROR;
uint8_t log_initialized = 0;

struct tco_shmem_data_control *data_control = NULL;
sem_t *data_sem_control = NULL;

struct tco_shmem_data_training *data_training = NULL;
sem_t *data_sem_training = NULL;

static int shmem_not_initialized()
{
    return data_sem_control == NULL || data_control == NULL || data_training == NULL || data_sem_training == NULL;
}

void *shmem_constructor(godot_object *p_instance, void *p_method_data)
{
    /* Avoid double init of logger (would fail while trying to reopen the opened log file) */
    if (!log_initialized)
    {
        if (log_init("libshmemaccess", "./log.txt") != 0)
        {
            api->godot_print_error("Failed to init logger", __func__, __FILE__, __LINE__);
            return NULL;
        }
        log_initialized = 1;
    }

    if (shmem_map(TCO_SHMEM_NAME_CONTROL, TCO_SHMEM_SIZE_CONTROL, TCO_SHMEM_NAME_SEM_CONTROL, O_RDWR, (void **)&data_control, &data_sem_control) != 0)
    {
        log_error("Failed to map control shared memory and associated semaphore");
        return NULL;
    }

    if (shmem_map(TCO_SHMEM_NAME_TRAINING, TCO_SHMEM_SIZE_TRAINING, TCO_SHMEM_NAME_SEM_TRAINING, O_RDWR, (void **)&data_training, &data_sem_training) != 0)
    {
        log_error("Failed to map training shared memory and associated semaphore");
        return NULL;
    }

    struct tco_shmem_data_control *shmem_data = (struct tco_shmem_data_control *)api->godot_alloc(TCO_SHMEM_SIZE_CONTROL);
    return shmem_data;
}

void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data)
{
    api->godot_free(p_user_data);
    munmap(0, TCO_SHMEM_SIZE_CONTROL);
    sem_close(data_sem_control);
    log_debug("Shmem has been destroyed");
}

godot_variant shmem_data_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                              int p_num_args, godot_variant **p_args)
{
    godot_variant ret_val;
    godot_array data;
    api->godot_variant_new_nil(&ret_val);
    api->godot_array_new(&data);

    if (shmem_not_initialized())
    {
        log_debug("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    /* Check if init was successful, if not return NILL */
    if (shmem_not_initialized())
    {
        return ret_val;
    }

    /* To minimize semaphore blocking time, we copy the data in shmem into this variable */
    struct tco_shmem_data_control shmem_data_cpy = {0};

    if (sem_wait(data_sem_control) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return ret_val;
    }
    /* START: Critical section */
    if (data_control->valid)
    {
        memcpy(&shmem_data_cpy, data_control, TCO_SHMEM_SIZE_CONTROL); /* Assumed to never fail */
    }
    else
    {
        /* To prevent use of the shmem data which was never read */
        shmem_data_cpy.valid = 0;
    }
    /* END: Critical section */
    if (sem_post(data_sem_control) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return ret_val;
    }

    if (shmem_data_cpy.valid)
    {
        /* 
        Loop through channels. If they are active, append the pulse frac to the array, else append
        NILL. 
        */
        for (int i = 0; i < 16; i++)
        {
            godot_variant pulse_frac;
            if (shmem_data_cpy.ch[i].active > 0)
            {
                api->godot_variant_new_real(&pulse_frac, shmem_data_cpy.ch[i].pulse_frac);
            }
            else
            {
                api->godot_variant_new_nil(&pulse_frac);
            }
            api->godot_array_push_back(&data, &pulse_frac);
        }
    }

    api->godot_variant_new_array(&ret_val, &data);
    return ret_val;
}

/* TODO: Return a different value on failure and success, right now it's always NILL */
godot_variant shmem_data_write(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args)
{
    godot_variant nill;
    api->godot_variant_new_nil(&nill);

    if (shmem_not_initialized())
    {
        log_debug("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    /* Check if init was successful, if not return NILL */
    if (shmem_not_initialized())
    {
        return nill;
    }

    if (p_num_args != 5)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 5", p_num_args);
        return nill;
    }

    /* 'state' is special since it needs to stay the same unless the engine explicitly calls another
    function to reset the 'state' field. */
    uint8_t state;

    if (sem_wait(data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return nill;
    }
    /* START: Critical section */
    if (data_training->valid == 0)
    {
        state = 0; /* By default simulation should run */
    }
    else
    {
        state = data_training->state;
    }
    /* END: Critical section */
    if (sem_post(data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return nill;
    }

    /* Make godot variants more easily transformable to C types */
    godot_pool_byte_array wheels_off_track_godot_arr = api->godot_variant_as_pool_byte_array(p_args[0]);
    godot_pool_byte_array_read_access *wheels_off_track_godot_arr_access = api->godot_pool_byte_array_read(&wheels_off_track_godot_arr);

    godot_vector3 pos_godot = api->godot_variant_as_vector3(p_args[3]);

    godot_pool_byte_array video_godot_arr = api->godot_variant_as_pool_byte_array(p_args[4]);
    godot_pool_byte_array_read_access *video_godot_arr_access = api->godot_pool_byte_array_read(&video_godot_arr);

    /* Transform variants to C types */
    const uint8_t valid = 1u;
    const uint8_t *wheels_off_track = api->godot_pool_byte_array_read_access_ptr(wheels_off_track_godot_arr_access);
    const uint8_t drifting = (uint8_t)api->godot_variant_as_int(p_args[1]);
    const float speed = (float)api->godot_variant_as_real(p_args[2]);
    const float pos[3] = {
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_X),
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_Y),
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_Z),
    };
    const uint8_t *video = api->godot_pool_byte_array_read_access_ptr(video_godot_arr_access);

    /* Construct new contents of the shared memory */
    struct tco_shmem_data_training data_training_cpy = {
        .valid = valid,
        .state = state,
        .wheels_off_track = {0},
        .drifting = drifting,
        .speed = speed,
        .pos = {0},
        .video = {0},
    };
    /* These are assumed to never fail */
    memcpy(&data_training_cpy.wheels_off_track, wheels_off_track, 4 * sizeof(uint8_t));
    memcpy(&data_training_cpy.pos, pos, 3 * sizeof(float));
    memcpy(&data_training_cpy.video, video, TCO_SIM_HEIGHT * TCO_SIM_WIDTH * sizeof(uint8_t));

    if (sem_wait(data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return nill;
    }
    /* START: Critical section */
    /* Assumed to never fail */
    memcpy(data_training, &data_training_cpy, TCO_SHMEM_SIZE_TRAINING);
    /* END: Critical section */
    if (sem_post(data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return nill;
    }

    /* Help Godot free all the unused memory */
    api->godot_pool_byte_array_destroy(&wheels_off_track_godot_arr);
    api->godot_pool_byte_array_read_access_destroy(wheels_off_track_godot_arr_access);
    api->godot_pool_byte_array_destroy(&video_godot_arr);
    api->godot_pool_byte_array_read_access_destroy(video_godot_arr_access);

    return nill;
}

/* TODO: Implement the 'read' and 'reset' methods as one method using 'p_method_data' */
godot_variant shmem_state_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args)
{
    godot_variant nill;
    api->godot_variant_new_nil(&nill);

    if (shmem_not_initialized())
    {
        log_debug("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    /* Check if init was successful, if not return NILL */
    if (shmem_not_initialized())
    {
        return nill;
    }

    if (p_num_args != 0)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 0", p_num_args);
        return nill;
    }

    uint8_t state;
    if (sem_wait(data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return nill;
    }
    /* START: Critical section */
    if (data_training->valid)
    {
        state = data_training->state;
    }
    else
    {
        state = 0; /* By default simulation will be paused */
    }
    /* END: Critical section */
    if (sem_post(data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return nill;
    }

    godot_variant ret;
    api->godot_variant_new_uint(&ret, state);
    return ret;
}

godot_variant shmem_state_reset(godot_object *p_instance, void *p_method_data, void *p_user_data,
                                int p_num_args, godot_variant **p_args)
{
    godot_variant nill;
    api->godot_variant_new_nil(&nill);

    if (shmem_not_initialized())
    {
        log_debug("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    /* Check if init was successful, if not return NILL */
    if (shmem_not_initialized())
    {
        return nill;
    }

    if (p_num_args != 0)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 0", p_num_args);
        return nill;
    }

    if (sem_wait(data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return nill;
    }
    /* START: Critical section */
    data_training->state = 0; /* Doesn't matter if memory is valid or not */
    /* END: Critical section */
    if (sem_post(data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return nill;
    }

    return nill;
}
