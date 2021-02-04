#include <gdnative_api_struct.gen.h>
#include "shmem_access.h"

/*
GDNative supports a large collection of functions for calling back into the main Godot executable.
In order for the module to have access to these functions, GDNative provides the application with
a struct containing pointers to all these functions.
*/
const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;

/**
 * @brief Initializes our dynamic library. Godot will give it a pointer to a structure that contains
 * various bits of information we may find useful among which the pointers to our API structures.
 */
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

/**
 * @brief Gets called before the library is unloaded. Godot will unload the library when no object
 * uses it anymore.
 */
void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *p_options)
{
    api = NULL;
    nativescript_api = NULL;
}

/**
 * @brief Most important function. Godot calls this function as part of loading a GDNative library
 * and communicates back to the engine what objects we make available.
 */
void GDN_EXPORT godot_nativescript_init(void *p_handle)
{
    godot_instance_create_func create = {NULL, NULL, NULL};
    create.create_func = &shmem_constructor;

    godot_instance_destroy_func destroy = {NULL, NULL, NULL};
    destroy.destroy_func = &shmem_destructor;

    nativescript_api->godot_nativescript_register_class(p_handle, "Shmem", "Reference",
                                                        create, destroy);

    godot_instance_method data_read = {NULL, NULL, NULL};
    data_read.method = &shmem_data_read;

    godot_instance_method data_write = {NULL, NULL, NULL};
    data_write.method = &shmem_data_write;

    godot_instance_method state_read = {NULL, NULL, NULL};
    state_read.method = &shmem_state_read;

    godot_instance_method state_reset = {NULL, NULL, NULL};
    state_reset.method = &shmem_state_reset;

    godot_method_attributes attributes = {GODOT_METHOD_RPC_MODE_DISABLED};

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "data_read",
                                                         attributes, data_read);

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "data_write",
                                                         attributes, data_write);

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "state_read",
                                                         attributes, state_read);

    nativescript_api->godot_nativescript_register_method(p_handle, "Shmem", "state_reset",
                                                         attributes, state_reset);
}
