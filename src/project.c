#define _XOPEN_SOURCE 700

#include "project.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *duplicate_range(const char *start, size_t length) {
  char *result = malloc(length + 1);
  if (!result)
    return NULL;
  memcpy(result, start, length);
  result[length] = '\0';
  return result;
}

static char *path_directory(const char *path) {
  const char *slash = strrchr(path, '/');
  if (!slash)
    return strdup(".");
  if (slash == path)
    return strdup("/");
  return duplicate_range(path, (size_t)(slash - path));
}

static char *path_join(const char *left, const char *right) {
  size_t left_length = strlen(left);
  size_t right_length = strlen(right);
  bool separator = left_length && left[left_length - 1] != '/';
  char *result = malloc(left_length + right_length + (separator ? 2 : 1));
  if (!result)
    return NULL;
  memcpy(result, left, left_length);
  size_t offset = left_length;
  if (separator)
    result[offset++] = '/';
  memcpy(result + offset, right, right_length + 1);
  return result;
}

static bool regular_file_exists(const char *path) {
  struct stat status;
  return stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static char *trim(char *text) {
  while (isspace((unsigned char)*text))
    text++;
  char *end = text + strlen(text);
  while (end > text && isspace((unsigned char)end[-1]))
    end--;
  *end = '\0';
  return text;
}

static void manifest_error(const char *path, size_t line, const char *message) {
  fprintf(stderr, "%s:%zu: manifest error: %s\n", path, line, message);
}

static char *parse_quoted_string(const char *value) {
  while (isspace((unsigned char)*value))
    value++;
  if (*value != '"')
    return NULL;
  value++;
  const char *end = strchr(value, '"');
  if (!end)
    return NULL;
  for (const char *tail = end + 1; *tail; tail++)
    if (!isspace((unsigned char)*tail))
      return NULL;
  return duplicate_range(value, (size_t)(end - value));
}

static bool append_string(char ***items, size_t *count, char *value) {
  char **grown = realloc(*items, sizeof(char *) * (*count + 1));
  if (!grown)
    return false;
  grown[*count] = value;
  *items = grown;
  (*count)++;
  return true;
}

static bool append_dependency(RunesProject *project, char *name, char *path) {
  size_t count = project->dependency_count;
  char **names =
      realloc(project->dependency_names, sizeof(char *) * (count + 1));
  if (!names)
    return false;
  project->dependency_names = names;
  char **paths =
      realloc(project->dependency_paths, sizeof(char *) * (count + 1));
  if (!paths)
    return false;
  project->dependency_paths = paths;
  project->dependency_names[count] = name;
  project->dependency_paths[count] = path;
  project->dependency_count++;
  return true;
}

static bool is_module_name(const char *name) {
  if (!name || !(isalpha((unsigned char)name[0]) || name[0] == '_'))
    return false;
  for (size_t i = 1; name[i]; i++)
    if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
      return false;
  return true;
}

static char *canonical_project_path(const RunesProject *project,
                                    const char *relative) {
  char *joined = path_join(project->root, relative);
  if (!joined)
    return NULL;
  char *canonical = realpath(joined, NULL);
  if (!canonical)
    fprintf(stderr, "%s: %s\n", joined, strerror(errno));
  free(joined);
  return canonical;
}

static bool parse_string_array(RunesProject *project, const char *value,
                               size_t line) {
  while (isspace((unsigned char)*value))
    value++;
  if (*value++ != '[') {
    manifest_error(project->manifest_path, line,
                   "module roots must be an array of strings");
    return false;
  }
  for (;;) {
    while (isspace((unsigned char)*value))
      value++;
    if (*value == ']') {
      value++;
      while (isspace((unsigned char)*value))
        value++;
      if (*value) {
        manifest_error(project->manifest_path, line,
                       "unexpected text after module roots");
        return false;
      }
      return true;
    }
    if (*value != '"') {
      manifest_error(project->manifest_path, line,
                     "module roots must contain quoted strings");
      return false;
    }
    value++;
    const char *end = strchr(value, '"');
    if (!end) {
      manifest_error(project->manifest_path, line,
                     "unterminated module root string");
      return false;
    }
    char *relative = duplicate_range(value, (size_t)(end - value));
    char *canonical =
        relative ? canonical_project_path(project, relative) : NULL;
    free(relative);
    if (!canonical || !append_string(&project->module_roots,
                                     &project->module_root_count, canonical)) {
      free(canonical);
      return false;
    }
    value = end + 1;
    while (isspace((unsigned char)*value))
      value++;
    if (*value == ',') {
      value++;
      continue;
    }
    if (*value != ']') {
      manifest_error(project->manifest_path, line,
                     "expected ',' or ']' in module roots");
      return false;
    }
  }
}

static char *parse_dependency_path(const char *value) {
  while (isspace((unsigned char)*value))
    value++;
  if (*value == '"')
    return parse_quoted_string(value);
  if (*value++ != '{')
    return NULL;
  while (isspace((unsigned char)*value))
    value++;
  if (strncmp(value, "path", 4) != 0)
    return NULL;
  value += 4;
  while (isspace((unsigned char)*value))
    value++;
  if (*value++ != '=')
    return NULL;
  while (isspace((unsigned char)*value))
    value++;
  if (*value++ != '"')
    return NULL;
  const char *end = strchr(value, '"');
  if (!end)
    return NULL;
  char *result = duplicate_range(value, (size_t)(end - value));
  value = end + 1;
  while (isspace((unsigned char)*value))
    value++;
  if (*value != '}') {
    free(result);
    return NULL;
  }
  value++;
  while (isspace((unsigned char)*value))
    value++;
  if (*value) {
    free(result);
    return NULL;
  }
  return result;
}

void runes_project_init(RunesProject *project) {
  memset(project, 0, sizeof(*project));
}

void runes_project_destroy(RunesProject *project) {
  free(project->manifest_path);
  free(project->root);
  free(project->name);
  free(project->entry);
  free(project->target);
  for (size_t i = 0; i < project->module_root_count; i++)
    free(project->module_roots[i]);
  free(project->module_roots);
  for (size_t i = 0; i < project->dependency_count; i++) {
    free(project->dependency_names[i]);
    free(project->dependency_paths[i]);
  }
  free(project->dependency_names);
  free(project->dependency_paths);
  memset(project, 0, sizeof(*project));
}

char *runes_project_discover(const char *start_directory) {
  char *current = realpath(start_directory ? start_directory : ".", NULL);
  if (!current)
    return NULL;
  for (;;) {
    char *candidate = path_join(current, "runes.toml");
    if (!candidate) {
      free(current);
      return NULL;
    }
    if (regular_file_exists(candidate)) {
      free(current);
      return candidate;
    }
    free(candidate);
    if (strcmp(current, "/") == 0)
      break;
    char *parent = path_directory(current);
    free(current);
    current = parent;
    if (!current)
      return NULL;
  }
  free(current);
  return NULL;
}

bool runes_project_load(RunesProject *project, const char *manifest_path) {
  char *canonical = realpath(manifest_path, NULL);
  if (!canonical) {
    fprintf(stderr, "%s: %s\n", manifest_path, strerror(errno));
    return false;
  }
  FILE *file = fopen(canonical, "r");
  if (!file) {
    fprintf(stderr, "%s: %s\n", canonical, strerror(errno));
    free(canonical);
    return false;
  }
  project->manifest_path = canonical;
  project->root = path_directory(canonical);
  if (!project->root) {
    fclose(file);
    return false;
  }

  enum {
    SECTION_NONE,
    SECTION_PROJECT,
    SECTION_MODULES,
    SECTION_DEPENDENCIES
  } section = SECTION_NONE;
  char *line_text = NULL;
  size_t capacity = 0, line = 0;
  bool ok = true;
  while (ok && getline(&line_text, &capacity, file) >= 0) {
    line++;
    char *comment = strchr(line_text, '#');
    if (comment)
      *comment = '\0';
    char *text = trim(line_text);
    if (!*text)
      continue;
    if (*text == '[') {
      if (strcmp(text, "[project]") == 0)
        section = SECTION_PROJECT;
      else if (strcmp(text, "[modules]") == 0)
        section = SECTION_MODULES;
      else if (strcmp(text, "[dependencies]") == 0)
        section = SECTION_DEPENDENCIES;
      else {
        manifest_error(canonical, line, "unknown section");
        ok = false;
      }
      continue;
    }
    char *equals = strchr(text, '=');
    if (!equals) {
      manifest_error(canonical, line, "expected key = value");
      ok = false;
      continue;
    }
    *equals = '\0';
    char *key = trim(text);
    char *value = trim(equals + 1);
    if (section == SECTION_PROJECT &&
        (strcmp(key, "name") == 0 || strcmp(key, "entry") == 0 ||
         strcmp(key, "target") == 0)) {
      char *parsed = parse_quoted_string(value);
      char **field = strcmp(key, "name") == 0
                         ? &project->name
                         : strcmp(key, "entry") == 0 ? &project->entry
                                                     : &project->target;
      if (!parsed || *field) {
        free(parsed);
        manifest_error(canonical, line, "invalid or duplicate project field");
        ok = false;
      } else {
        *field = parsed;
      }
    } else if (section == SECTION_MODULES && strcmp(key, "roots") == 0) {
      if (project->module_root_count ||
          !parse_string_array(project, value, line))
        ok = false;
    } else if (section == SECTION_DEPENDENCIES) {
      char *relative = parse_dependency_path(value);
      char *path = relative ? canonical_project_path(project, relative) : NULL;
      char *name = strdup(key);
      free(relative);
      bool duplicate = false;
      for (size_t i = 0; i < project->dependency_count; i++)
        if (strcmp(project->dependency_names[i], key) == 0 ||
            (path && strcmp(project->dependency_paths[i], path) == 0))
          duplicate = true;
      if (!path || !name || !is_module_name(name) || strcmp(name, "std") == 0 ||
          duplicate || !append_dependency(project, name, path)) {
        free(path);
        free(name);
        manifest_error(canonical, line, "invalid path dependency");
        ok = false;
      }
    } else {
      manifest_error(canonical, line, "unknown field");
      ok = false;
    }
  }
  free(line_text);
  fclose(file);
  if (!ok)
    return false;
  if (!project->name || !project->entry) {
    manifest_error(canonical, line ? line : 1,
                   "[project] requires name and entry");
    return false;
  }
  char *entry = canonical_project_path(project, project->entry);
  if (!entry)
    return false;
  free(project->entry);
  project->entry = entry;
  if (!project->module_root_count) {
    char *default_root = path_join(project->root, "src");
    char *canonical_root = default_root ? realpath(default_root, NULL) : NULL;
    free(default_root);
    if (!canonical_root ||
        !append_string(&project->module_roots, &project->module_root_count,
                       canonical_root)) {
      free(canonical_root);
      fprintf(stderr, "%s: default module root 'src' does not exist\n",
              project->manifest_path);
      return false;
    }
  }
  return true;
}
