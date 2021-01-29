#include <gdnative_api_struct.gen.h>

#include <stdlib.h>

#include <sys/mman.h>

#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "tco_libd.h"
#include "tco_shmem.h"

int log_level = LOG_INFO | LOG_DEBUG | LOG_ERROR;
static uint8_t log_initialized = 0;

const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;

void *shmem_constructor(godot_object *p_instance, void *p_method_data);
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);

/* Define functions here */

godot_variant shmem_get_data(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args);

godot_variant shmem_write_data(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args);

godot_variant shmem_is_valid(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args);

/* GDNative initialization code */

void GDN_EXPORT godot_gdnative_init(godot_gdnative_init_options *p_options)
{
    api = p_options->api_struct;

    /* Find extensions. */
    for (int i = 0; i < api->num_extensions; i++)
    {
        switch (api->extensions[i]->type)
        {
        case GDNATIVE_EXT_NATIVESCRIPT:
        {
            nativescript_api = (godot_gdnative_ext_nativescript_api_struct *)api->extensions[i];
        };
        break;
        default:
            break;
        }
    }
}

void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *p_options)
{
    api = NULL;
    nativescript_api = NULL;
}

/* This function shows the Godot Engine which functions are available */
void GDN_EXPORT godot_nativescript_init(void *p_handle)
{
    godot_instance_create_func create = {NULL, NULL, NULL};
    create.create_func = &shmem_constructor;

    godot_instance_destroy_func destroy = {NULL, NULL, NULL};
    destroy.destroy_func = &shmem_destructor;

    nativescript_api->godot_nativescript_register_class(p_handle, "Shmem", "Reference",
                                                        create, destroy);

    godot_instance_method get_data = {NULL, NULL, NULL};
    get_data.method = &shmem_get_data;

    godot_instance_method write_data = {NULL, NULL, NULL};
    write_data.method = &shmem_write_data;

    godot_instance_method is_valid = {NULL, NULL, NULL};
    is_valid.method = &shmem_is_valid;

    godot_method_attributes attributes = {GODOT_METHOD_RPC_MODE_DISABLED};

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "get_data",
                                                         attributes, get_data);

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "write_data",
                                                         attributes, write_data);

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "is_valid",
                                                         attributes, is_valid);
}

/**
 * End of engine interfacing code and start of library code
*/

struct tco_shmem_data_control *control_data;
sem_t *control_data_sem;

struct tco_shmem_data_sim *sim_data;
sem_t *sim_data_sem;

/**
 * @brief will map the shared memory and create the associated semaphore. Called by godot engine.
 * @return a copy of the shmem_data. We only send "snapshots" of the shmem to the engine.
*/
void *shmem_constructor(godot_object *p_instance, void *p_method_data)
{
    /* Avoid double init of logger (would fail while trying to reopen the opened log file) */
    if (!log_initialized)
    {
        if (log_init("libshmemaccess", "./log.txt") != 0)
        {
            api->godot_print_error("Failed to init logger", "shmem_constructor", "shmem_access.c", 94);
            return (void *)EXIT_FAILURE;
        }
        log_initialized = 1;
    }

    if (shmem_map(TCO_SHMEM_NAME_CONTROL, TCO_SHMEM_SIZE_CONTROL, TCO_SHMEM_NAME_SEM_CONTROL, O_RDWR, (void **)&control_data, &control_data_sem) != 0)
    {
        log_error("Failed to map control shared memory and associated semaphore");
        return (void *)EXIT_FAILURE;
    }

    if (shmem_map(TCO_SHMEM_NAME_SIM, TCO_SHMEM_SIZE_SIM, TCO_SHMEM_NAME_SEM_SIM, O_RDWR, (void **)&sim_data, &sim_data_sem) != 0) {
        api->godot_print_error("Failed to init shmem_sim", "shmem_constructor", "shmem_access.c", 94);

        log_error("Failed to map sim shared memory and associated semaphore");
        return (void *)EXIT_FAILURE;
    }

    struct tco_shmem_data_control *shmem_data = (struct tco_shmem_data_control *)api->godot_alloc(TCO_SHMEM_SIZE_CONTROL);
    return shmem_data;
}

/**
 * @brief will map the shared memory and create the associated semaphore. Called by godot engine.
*/
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data)
{
    api->godot_free(p_user_data);
    munmap(0, TCO_SHMEM_SIZE_CONTROL);
    sem_close(control_data_sem);
}

/**
 * @brief will return a snapshot of the shmem space. Will block until the semaphore gives rights to
 * access the shmem space
 * @return snapshot of shmem space as a godot_type. There are 16 channels so there will be an array
 * of size 16 floats given. If an entry contains false, then it is inactive.
*/
godot_variant shmem_get_data(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args)
{
    godot_variant real_ret;
    godot_array ret;
    api->godot_variant_new_nil(&real_ret);
    api->godot_array_new(&ret);

    if (control_data_sem == NULL || control_data == NULL)
    {
        log_info("Running constructor");
        shmem_constructor(p_instance, p_method_data);
    }
    if (control_data_sem == NULL || control_data == NULL)
    {
        api->godot_variant_new_bool(&real_ret, 0);
        return real_ret;
    }

    struct tco_shmem_data_control shmem_data_cpy = {0};

    if (sem_wait(control_data_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return real_ret;
    }
    /* START: Critical section */
    if (control_data->valid)
    {
        memcpy(&shmem_data_cpy, control_data, TCO_SHMEM_SIZE_CONTROL); /* Assumed never to fail */
    }
    else
    {
        shmem_data_cpy.valid = 0;
    }
    
    /* END: Critical section */
    if (sem_post(control_data_sem) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return real_ret;
    }

    if (shmem_data_cpy.valid)
    {
        /* 
        Loop through channels. 
        If they are active, append the pulse frac to the array, 
        else append NILL. 
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
            api->godot_array_push_back(&ret, &pulse_frac);
        }
    }

    api->godot_variant_new_array(&real_ret, &ret);
    return real_ret;
}

/* This function takes 3 arguments : 
   number_of_wheels_on_track (cast to uint8_t)
   motor_power (cast to float)
   steering_angle (cast to float)
*/
godot_variant shmem_write_data(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args) 
{
    godot_variant gnll;
    api->godot_variant_new_nil(&gnll);
    if (p_num_args != 3)
    {
        log_error("Incorrect arg count to write to shmem. %d/3 given", p_num_args);
        api->godot_print_warning("Not enough args to write to shmem", "shmem_write_data", "shmem_access.c", 219);
        return gnll;
    }

    uint8_t num_wheels_on_track = (uint8_t)api->godot_variant_as_int(p_args[0]);
    float motor_power = (float)api->godot_variant_as_real(p_args[1]);
    float servo_angle = (float)api->godot_variant_as_real(p_args[2]);

    /* Aquire semaphore */
    if (sem_wait(sim_data_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return gnll;
    }
    /* Write data */
    if (sim_data->valid == 0)
    {
        log_info("sim_data filed is marked as invalid.");
        return gnll;
    }
    sim_data->wheels_on_track = num_wheels_on_track;
    sim_data->motor_power = motor_power;
    sim_data->steering_angle = servo_angle;
    
    /* Release Semaphore */
    if (sem_post(sim_data_sem) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return gnll;
    }

    return gnll;
}

godot_variant shmem_is_valid(godot_object *p_instance, void *p_method_data,
                             void *p_user_data, int p_num_args, godot_variant **p_args)
{
    godot_variant ret;
    api->godot_variant_new_int(&ret, -1);
    /* Aquire semaphore */
    if (sem_wait(sim_data_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return ret;
    }
    /* read data */
    int valid = (int)sim_data->valid;
    sim_data->valid = (uint8_t) 1;
    /* Release Semaphore */
    if (sem_post(sim_data_sem) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return ret;
    }

    api->godot_variant_new_int(&ret, valid);
    return ret;
}