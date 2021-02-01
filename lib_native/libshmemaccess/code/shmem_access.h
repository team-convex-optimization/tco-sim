#ifndef _SHMEM_ACCESS_H_
#define _SHMEM_ACCESS_H_

#include <gdnative_api_struct.gen.h>

/**
 * @brief Maps shared memory into process memory and gets a reference to the associated semaphore
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @return Pointer to 
 */
void *shmem_constructor(godot_object *p_instance, void *p_method_data);

/**
 * @brief Unmaps shared memory and closes the instance to the semaphore
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function. Unused.
 */
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);

/**
 * @brief Reads data from control data shared memory and will block until the semaphore is released
 * @return Array of all 16 channels from control data shmem as a 'godot_type'. If a channel is not
 * active, it write a NILL at that index.
 */
godot_variant shmem_data_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                              int p_num_args, godot_variant **p_args);

/**
 * @brief Takes an array containing all elements required to fill the training shared memory and updates it
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function. Unused.
 * @param p_num_args Number of arguments passed in from GDScript
 * @param p_args Pointer to array of arguments. This function takes 7 arguments as defined in 'tco_shmem'.
 * Args:
 *     [0] = wheels_off_track[4]
 *     [1] = drifting
 *     [2] = speed
 *     [3] = steer
 *     [4] = motor
 *     [5] = pos[3]
 *     [6] = video[18][32]
 * @return Returns a Godot 'nill'
 */
godot_variant shmem_data_write(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args);

#endif /* _SHMEM_ACCESS_H_ */