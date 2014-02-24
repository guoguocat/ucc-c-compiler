#ifndef UCC_EXT_H
#define UCC_EXT_H

extern char **include_paths;

char *actual_path(const char *prefix, const char *path);
void rename_or_move(char *old, char *new);
void cat(char *in, char *out, int append);

void preproc( char *in,    char *out, char **args);
void compile( char *in,    char *out, char **args);
void assemble(char *in,    char *out, char **args);
void link_all(char **objs, char *out, char **args);
void dsym(char *exe);

void ucc_ext_cmds_show(int);
void ucc_ext_cmds_noop(int);

#endif
