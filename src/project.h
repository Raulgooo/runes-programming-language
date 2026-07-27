#ifndef RUNES_PROJECT_H
#define RUNES_PROJECT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *manifest_path;
  char *root;
  char *name;
  char *entry;
  char *target;
  char **module_roots;
  size_t module_root_count;
  char **dependency_names;
  char **dependency_paths;
  size_t dependency_count;
} RunesProject;

void runes_project_init(RunesProject *project);
void runes_project_destroy(RunesProject *project);

/* Returns an allocated manifest path, or NULL when none exists. */
char *runes_project_discover(const char *start_directory);

bool runes_project_load(RunesProject *project, const char *manifest_path);

#endif
