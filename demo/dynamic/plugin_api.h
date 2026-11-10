#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdio.h>

/* Each plugin exports these four symbols. Returned strings stay valid until
 * the shared library is unloaded; user-facing strings are in Spanish.
 */

/* Return the plugin's display name. Takes no parameters. */
const char *plugin_name(void);

/* Return the supported extension, including its dot. Takes no parameters. */
const char *plugin_extension(void);

/* Return a Spanish description of the plugin. Takes no parameters. */
const char *info(void);

/* Analyze a file and write its Spanish result.
 * file_path: path to the input file; output: destination stream.
 * Returns zero on success or nonzero if analysis fails.
 */
int analyze(const char *file_path, FILE *output);

/* Function pointer types for symbols resolved with dlsym(). */
typedef const char *(*plugin_name_fn)(void);
typedef const char *(*plugin_extension_fn)(void);
typedef const char *(*plugin_info_fn)(void);
typedef int (*plugin_analyze_fn)(const char *file_path, FILE *output);

#endif /* PLUGIN_API_H */
