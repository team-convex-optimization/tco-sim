#ifndef _SHMEM_ACCESS_H_
#define _SHMEM_ACCESS_H_

#include <gdnative_api_struct.gen.h>

/**
 * @brief Maps shared memory into process memory and gets a reference to the associated semaphore
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @note When something went wrong, Godot should print an error like "p_ptr == __null" indicating
 * that the user_data has not be allocated.
 * @return Pointer to user_data on success and NULL on failure
 */
void *shmem_constructor(godot_object *p_instance, void *p_method_data);

/**
 * @brief Unmaps shared memory and closes the instance to the semaphore
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function and
 * contains all Shmem object info.
 */
void shmem_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);

/**
 * @brief Reads data from control data shared memory and will block until the semaphore is released
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function and
 * contains all Shmem object info.
 * @param p_num_args Number of arguments passed in from GDScript
 * @param p_args Pointer to array of arguments. This function takes no arguments.
 * @return Godot variant array of all 16 channels from control data shmem on success and -1 on
 * failure.
 */
godot_variant shmem_data_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                              int p_num_args, godot_variant **p_args);

/**
 * @brief Takes an array containing all elements required to fill the state shared memory and
 * updates it
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function and
 * contains all Shmem object info.
 * @param p_num_args Number of arguments passed in from GDScript
 * @param p_args Pointer to array of arguments. This function takes 5 arguments as defined in
 * 'tco_shmem'.
 * Args:
 *     [0] = wheels_off_track[4]
 *     [1] = drifting
 *     [2] = speed
 *     [3] = pos[3]
 *     [4] = video[480][640]
 *     [5] = rpm
 * @return Godot variant 0 on success and -1 on failure
 */
godot_variant shmem_data_write(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args);

/**
 * @brief Read the state byte from state shmem and return it.
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function and
 * contains all Shmem object info.
 * @param p_num_args Number of arguments passed in from GDScript
 * @param p_args Pointer to array of arguments. This function takes no arguments.
 * @return On success: 0 means 'stop', 1 means 'step', 2 means 'reset', and on failure -1
 */
godot_variant shmem_state_read(godot_object *p_instance, void *p_method_data, void *p_user_data,
                               int p_num_args, godot_variant **p_args);

/* TODO: Return 0 on success and 1 on failure  */
/**
 * @brief Write a 0 to the state byte in state shmem indicating that the simulator finished
 * performing any requested changes to the simulation state.
 * @param p_instance Pointer to this function
 * @param p_method_data For reusing one function for multiple methods. Unused.
 * @param p_user_data A pointer to a 'user_data' struct that gets passed to every function and
 * contains all Shmem object info.
 * @param p_num_args Number of arguments passed in from GDScript
 * @param p_args Pointer to array of arguments. This function takes no arguments.
 * @return Godot variant 0 on success and -1 on failure
 */
godot_variant shmem_state_reset(godot_object *p_instance, void *p_method_data, void *p_user_data,
                                int p_num_args, godot_variant **p_args);

#endif /* _SHMEM_ACCESS_H_ */