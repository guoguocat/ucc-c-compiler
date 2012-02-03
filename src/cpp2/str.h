#ifndef STR_H
#define STR_H

char *ustrdup2(const char *a, const char *b);
char *word_dup(const char *);

char *str_quote(const char *s);
void  str_trim(char *);

char *word_replace(char *line, char *pos, const char *find, const char *replace);
char *word_find(   char *line, char *word);
char *str_replace(char *line, char *start, char *end, const char *replace);
int   word_replace_g(char **pline, char *find, const char *replace);
char *nest_close_paren(char *start);

#endif
