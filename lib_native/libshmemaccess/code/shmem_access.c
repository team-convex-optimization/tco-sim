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

const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;

void *shmem_constructor(godot_object *p_instance, void *p_method_data);
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);

/* Define functions here */

godot_variant shmem_get_data(godot_object *p_instance, void *p_method_data,
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

    godot_method_attributes attributes = {GODOT_METHOD_RPC_MODE_DISABLED};

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "get_data",
                                                         attributes, get_data);
}

/**
 * End of engine interfacing code and start of library code
*/

struct tco_shmem_data_control *control_data;
sem_t *control_data_sem;

/**
 * @brief will map the shared memory and create the associated semaphore. Called by godot engine.
 * @return a copy of the shmem_data. We only send "snapshots" of the shmem to the engine.
*/
void *shmem_constructor(godot_object *p_instance, void *p_method_data)
{
    if (log_init("sim", "./log.txt") != 0)
    {
        // TODO: Log using engine built-in logger: "Failed to initialize the logger\n"
        return (void *)EXIT_FAILURE;
    }

    if (shmem_map(TCO_SHMEM_NAME_CONTROL, TCO_SHMEM_SIZE_CONTROL, TCO_SHMEM_NAME_SEM_CONTROL, O_RDWR, (void **)&control_data, &control_data_sem) != 0)
    {
        log_error("Failed to map shared memory and associated semaphore");
        return (void *)EXIT_FAILURE;
    }

    struct tco_shmem_data_control *shmem_data = (struct tco_shmem_data_control *)api->godot_alloc(TCO_SHMEM_SIZE_CONTROL);
    memset(shmem_data, 0, TCO_SHMEM_SIZE_CONTROL);
    return shmem_data;
}

/**
 * @brief will map the shared memory and create the associated semaphore. Called by godot engine.
*/
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data)
{
    api->godot_free(p_user_data);
    shm_unlink(TCO_SHMEM_NAME_CONTROL); // Unmap the shmem space
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

    /* Code to access the shmem space */
    if (sem_wait(control_data_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return real_ret;
    }
    /* START: Critical section */
    if (control_data->valid == 1)
    {
        for (int i = 0; i < 16; i++)
        { //Loop through channels. If they are active, add the float to the array. Else False.
            if (control_data->ch[i].active > 0)
            {
                godot_variant pulse_frac;
                api->godot_variant_new_real(&pulse_frac, control_data->ch[i].pulse_frac);
                api->godot_array_push_back(&ret, &pulse_frac);
            }
            else
            {
                api->godot_array_push_back(&ret, false);
            }
        }
    }
    /* END: Critical section */
    if (sem_post(control_data_sem) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return real_ret;
    }

    api->godot_variant_new_array(&real_ret, &ret);
    return real_ret;
}
