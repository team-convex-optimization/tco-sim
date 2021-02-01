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

struct tco_shmem_data_control *control_data;
sem_t *control_data_sem;

struct tco_shmem_data_training *data_training;
sem_t *data_sem_training;

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

    if (shmem_map(TCO_SHMEM_NAME_CONTROL, TCO_SHMEM_SIZE_CONTROL, TCO_SHMEM_NAME_SEM_CONTROL, O_RDWR, (void **)&control_data, &control_data_sem) != 0)
    {
        log_error("Failed to map control shared memory and associated semaphore");
        return NULL;
    }

    if (shmem_map(TCO_SHMEM_NAME_TRAINING, TCO_SHMEM_SIZE_TRAINING, TCO_SHMEM_NAME_SEM_TRAINING, O_RDWR, (void **)&data_training, &data_sem_training) != 0)
    {
        log_error("Failed to map sim shared memory and associated semaphore");
        return NULL;
    }

    struct tco_shmem_data_control *shmem_data = (struct tco_shmem_data_control *)api->godot_alloc(TCO_SHMEM_SIZE_CONTROL);
    return shmem_data;
}

void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data)
{
    api->godot_free(p_user_data);
    munmap(0, TCO_SHMEM_SIZE_CONTROL);
    sem_close(control_data_sem);
    log_debug("Shmem has been destroyed");
}

godot_variant shmem_data_read(godot_object *p_instance, void *p_method_data,
                              void *p_user_data, int p_num_args, godot_variant **p_args)
{
    godot_variant ret_val;
    godot_array data;
    api->godot_variant_new_nil(&ret_val);
    api->godot_array_new(&data);

    /* If either pointer is NULL, it means the library needs to initialize */
    if (control_data_sem == NULL || control_data == NULL)
    {
        log_debug("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    /* Check if init was successful, if not return NILL */
    if (control_data_sem == NULL || control_data == NULL)
    {
        return ret_val;
    }

    /* To minimize semaphore blocking time, we copy the data in shmem into this variable */
    struct tco_shmem_data_control shmem_data_cpy = {0};

    if (sem_wait(control_data_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return ret_val;
    }
    /* START: Critical section */
    if (control_data->valid)
    {
        memcpy(&shmem_data_cpy, control_data, TCO_SHMEM_SIZE_CONTROL); /* Assumed to never fail */
    }
    else
    {
        /* To prevent use of the shmem data which was never read */
        shmem_data_cpy.valid = 0;
    }
    /* END: Critical section */
    if (sem_post(control_data_sem) == -1)
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

godot_variant shmem_data_write(godot_object *p_instance, void *p_method_data,
                               void *p_user_data, int p_num_args, godot_variant **p_args)
{
    godot_variant nill;
    api->godot_variant_new_nil(&nill);

    if (p_num_args != 7)
    {
        log_error("Incorrect arg count to write to training shmem. %d given, needed exactly 7", p_num_args);
        return nill;
    }

    /* 'reset' is special since it needs to stay the same unless the engine explicitly calls another
    function to reset the 'reset' field. */
    uint8_t reset;

    if (sem_wait(data_sem_training) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return nill;
    }
    /* START: Critical section */
    if (data_training->valid == 0)
    {
        reset = 0;
    }
    else
    {
        reset = data_training->reset;
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

    godot_vector3 pos_godot = api->godot_variant_as_vector3(p_args[5]);

    godot_pool_byte_array video_godot_arr = api->godot_variant_as_pool_byte_array(p_args[6]);
    godot_pool_byte_array_read_access *video_godot_arr_access = api->godot_pool_byte_array_read(&video_godot_arr);

    /* Transform variants to C types */
    const uint8_t valid = 1u;
    const uint8_t *wheels_off_track = api->godot_pool_byte_array_read_access_ptr(wheels_off_track_godot_arr_access);
    const uint8_t drifting = (uint8_t)api->godot_variant_as_int(p_args[1]);
    const float speed = (float)api->godot_variant_as_real(p_args[2]);
    const float steer = (float)api->godot_variant_as_real(p_args[3]);
    const float motor = (float)api->godot_variant_as_real(p_args[4]);
    const float pos[3] = {
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_X),
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_Y),
        api->godot_vector3_get_axis(&pos_godot, GODOT_VECTOR3_AXIS_Z),
    };
    const uint8_t *video = api->godot_pool_byte_array_read_access_ptr(video_godot_arr_access);

    /* Construct new contents of the shared memory */
    struct tco_shmem_data_training data_training_cpy = {
        .valid = valid,
        .reset = reset,
        .wheels_off_track = {0},
        .drifting = drifting,
        .speed = speed,
        .steer = steer,
        .motor = motor,
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
