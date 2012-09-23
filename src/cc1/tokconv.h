#ifndef TOKCONV_H
#define TOKCONV_H

enum op_type        curtok_to_op(void);

enum type_primitive curtok_to_type_primitive(void);
enum type_qualifier curtok_to_type_qualifier(void);
enum type_storage   curtok_to_type_storage(void);

void eat( enum token t, const char *fnam, int line);
void eat2(enum token t, const char *fnam, int line, int die);
void uneat(enum token t);
int accept(enum token t);

#define EAT(t)         eat( (t), __FILE__, __LINE__)
#define EAT_OR_DIE(t)  eat2((t), __FILE__, __LINE__, 1)

int curtok_is_type_primitive(void);
int curtok_is_type_qual(void);
int curtok_is_type_store(void);

int curtok_in_list(va_list l);

char *token_current_spel(void);
char *token_current_spel_peek(void);
void token_get_current_str(char **ps, int *pl);

enum op_type curtok_to_compound_op(void);
int          curtok_is_compound_assignment(void);

const char *token_to_str(enum token t);

extern int parse_had_error;

#endif
