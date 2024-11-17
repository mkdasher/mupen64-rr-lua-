#pragma once

/*
 *	Interface for special platform-related functionality.
 *
 *	Must be implemented in the view layer.
 */

namespace PlatformService
{
    /**
     * Loads a library from the specified path.
     * \param path The path to the library.
     * \return The handle to the loaded library, or nullptr.
     */
    void* load_library(const char* path);
 
    /**
     * Frees a loaded library.
     * \param module The handle to a platform-specific module type to free.
     */
    void free_library(void* module);

    /**
     * Gets a function from the specified module.
     * \param module The handle to a platform-specific module type to get the function from.
     * \param name The function's name.
     * \return The function pointer, or nullptr.
     */
    void* get_function_in_module(void* module, const char* name);
 
    /**
     * Gets the free() function from the C runtime of the specified module.
     * \param module The handle to a platform-specific module type to get the function from.
     * \return The free function in the module.
     */
    void(* get_free_function_in_module(void* module))(void*);
}
