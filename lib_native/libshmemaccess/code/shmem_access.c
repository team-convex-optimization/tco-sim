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

typedef struct user_data_t
{
    uint8_t log_initialized;
    struct tco_shmem_data_control *data_control;
    sem_t *data_sem_control;
    struct tco_shmem_data_training *data_training;
    sem_t *data_sem_training;
} user_data_t;

int log_level = LOG_INFO | LOG_ERROR;

static int shmem_not_initialized(user_data_t *p_user_data)
{
    return p_user_data->data_sem_control == NULL ||
           p_user_data->data_control == NULL ||
           p_user_data->data_training == NULL ||
           p_user_data->data_sem_training == NULL;
}

void *shmem_constructor(godot_object *p_instance, void *p_method_data)
{
    user_data_t *user_data = api->godot_alloc(sizeof(user_data_t));
    memset(user_data, '\0', sizeof(user_data_t));

    /* Avoid double init of logger (would fail while trying to reopen the opened log file) */
    if (!user_data->log_initialized)
    {
        if (log_init("libshmemaccess", "./log.txt") != 0)
        {
            api->godot_print_error("Failed to init logger", __func__, __FILE__, __LINE__);
            return NULL;
        }
        user_data->log_initialized = 1;
    }

    if (shmem_map(TCO_SHMEM_NAME_CONTROL, TCO_SHMEM_SIZE_CONTROL, TCO_SHMEM_NAME_SEM_CONTROL, O_RDWR, (void **)&(user_data->data_control), &(user_data->data_sem_control)) != 0)
    {
        log_error("Failed to map control shared memory and associated semaphore");
        return NULL;
    }

    if (shmem_map(TCO_SHMEM_NAME_TRAINING, TCO_SHMEM_SIZE_TRAINING, TCO_SHMEM_NAME_SEM_TRAINING, O_RDWR, (void **)&(user_data->data_training), &(user_data->data_sem_training)) != 0)
    {
        log_error("Failed to map training shared memory and associated semaphore");
        return NULL;
    }

    log_debug("Libshmemaccess constructed");
    return user_data;
}

void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data)
{
    user_data_t *user_data = (user_data_t *)p_user_data;

    if (munmap(user_data->data_control, TCO_SHMEM_SIZE_CONTROL) != 0)
    {
        log_error("Failed to unmap control shmem");
    }
    if (sem_close(user_data->data_sem_control) != 0)
    {
        log_error("Failed to close semaphor for control shmem");
    }
    if (munmap(user_data->data_training, TCO_SHMEM_SIZE_TRAINING) != 0)
    {
        log_error("Failed to unmap training shmem");
    }
    if (sem_close(user_data->data_sem_training) != 0)
    {
        log_error("Failed to close semaphor for training shmem");
    }

    log_debug("Libshmemaccess deconstructed");
    if (user_data->log_initialized)
    {
        if (log_deinit() != 0)
        {
            log_error("Failed to deinit the logger");
        }
        else
        {
            user_data->log_initialized = 0;
        }
    }
    api->godot_free(user_data);
}

godot_variant shmem_data_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                              int p_num_args, godot_variant **p_args)
{
    user_data_t *user_data = (user_data_t *)p_user_data;
    godot_variant ret_failure;
    api->godot_variant_new_int(&ret_failure, -1);

    if (shmem_not_initialized(user_data))
    {
        log_error("Not initialized at call to 'shmem_data_read'");
        return ret_failure;
    }

    /* To minimize semaphore blocking time, we copy the data in shmem into this variable */
    struct tco_shmem_data_control shmem_data_cpy = {0};

    if (sem_wait(user_data->data_sem_control) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return ret_failure;
    }
    /* START: Critical section */
    if (user_data->data_control->valid)
    {
        memcpy(&shmem_data_cpy, user_data->data_control, TCO_SHMEM_SIZE_CONTROL); /* Assumed to never fail */
    }
    else
    {
        /* To prevent use of the shmem data which was never read */
        shmem_data_cpy.valid = 0;
    }
    /* END: Critical section */
    if (sem_post(user_data->data_sem_control) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return ret_failure;
    }

    godot_pool_real_array data_ch_pool;
    api->godot_pool_real_array_new(&data_ch_pool);
    if (shmem_data_cpy.valid)
    {
        /* 
        Loop through channels. If they are active, append the pulse frac to the array, else append
        0.5. XXX: Setting channels to 0.5 by deault by not be okay.
        */
        for (uint8_t i = 0; i < 16; i++)
        {
            if (shmem_data_cpy.ch[i].active > 0)
            {
                /* godot_real === float */
                api->godot_pool_real_array_append(&data_ch_pool, shmem_data_cpy.ch[i].pulse_frac);
            }
            else
            {
                api->godot_pool_real_array_append(&data_ch_pool, 0.5f);
            }
        }
    }

    godot_variant data_ch_var;
    api->godot_variant_new_pool_real_array(&data_ch_var, &data_ch_pool);
    api->godot_pool_real_array_destroy(&data_ch_pool);
    api->godot_variant_destroy(&ret_failure);
    return data_ch_var;
}

godot_variant shmem_data_write(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args)
{
    user_data_t *user_data = (user_data_t *)p_user_data;
    godot_variant ret_failure;
    godot_variant ret_success;
    api->godot_variant_new_int(&ret_failure, -1);
    api->godot_variant_new_int(&ret_success, 0);

    if (shmem_not_initialized(user_data))
    {
        log_error("Not initialized at call to 'shmem_data_write'");
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    if (p_num_args != 5)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 5", p_num_args);
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    /* 'state' is special since it needs to stay the same unless the engine explicitly calls another
    function to reset the 'state' field. */
    uint8_t state;

    if (sem_wait(user_data->data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }
    /* START: Critical section */
    if (user_data->data_training->valid == 0)
    {
        state = 0; /* By default simulation should run */
    }
    else
    {
        state = user_data->data_training->state;
    }
    /* END: Critical section */
    if (sem_post(user_data->data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
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
    memcpy(&data_training_cpy.video, video, TCO_FRAME_HEIGHT * TCO_FRAME_WIDTH * sizeof(uint8_t));

    if (sem_wait(user_data->data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }
    /* START: Critical section */
    /* Assumed to never fail */
    memcpy(user_data->data_training, &data_training_cpy, TCO_SHMEM_SIZE_TRAINING);
    /* END: Critical section */
    if (sem_post(user_data->data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    /* Help Godot free all the unused memory */
    api->godot_pool_byte_array_destroy(&wheels_off_track_godot_arr);
    api->godot_pool_byte_array_read_access_destroy(wheels_off_track_godot_arr_access);
    api->godot_pool_byte_array_destroy(&video_godot_arr);
    api->godot_pool_byte_array_read_access_destroy(video_godot_arr_access);

    api->godot_variant_destroy(&ret_failure);
    return ret_success;
}

/* TODO: Implement the 'read' and 'reset' methods as one method using 'p_method_data' */
godot_variant shmem_state_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args)
{
    user_data_t *user_data = (user_data_t *)p_user_data;
    godot_variant ret_failure;
    api->godot_variant_new_int(&ret_failure, -1);

    if (shmem_not_initialized(user_data))
    {
        log_error("Not initialized at call to 'shmem_state_read'");
        return ret_failure;
    }

    if (p_num_args != 0)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 0", p_num_args);
        return ret_failure;
    }

    uint8_t state;
    if (sem_wait(user_data->data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return ret_failure;
    }
    /* START: Critical section */
    if (user_data->data_training->valid)
    {
        state = user_data->data_training->state;
    }
    else
    {
        state = 0; /* By default simulation will be paused */
    }
    /* END: Critical section */
    if (sem_post(user_data->data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return ret_failure;
    }

    godot_variant state_var;
    api->godot_variant_new_uint(&state_var, state);
    api->godot_variant_destroy(&ret_failure);
    return state_var;
}

godot_variant shmem_state_reset(godot_object *p_instance, void *p_method_data, void *p_user_data,
                                int p_num_args, godot_variant **p_args)
{
    user_data_t *user_data = (user_data_t *)p_user_data;
    godot_variant ret_failure;
    godot_variant ret_success;
    api->godot_variant_new_int(&ret_failure, -1);
    api->godot_variant_new_int(&ret_success, 0);

    if (shmem_not_initialized(user_data))
    {
        log_error("Not initialized at call to 'shmem_state_reset'");
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    if (p_num_args != 0)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 0", p_num_args);
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    if (sem_wait(user_data->data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }
    /* START: Critical section */
    user_data->data_training->state = 0; /* Doesn't matter if memory is valid or not */
    /* END: Critical section */
    if (sem_post(user_data->data_sem_training) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        api->godot_variant_destroy(&ret_success);
        return ret_failure;
    }

    api->godot_variant_destroy(&ret_failure);
    return ret_success;
}
