/* This is ordinary C code against msvcrt imports; no FreshOS-specific host calls. */
typedef __SIZE_TYPE__ size_t;
extern void *calloc(size_t,size_t), *memcpy(void *,const void *,size_t);
extern size_t strlen(const char *);
extern int puts(const char *);
extern void free(void *);
int entry(void){const char *s="CRT imports from compiled C";size_t n=strlen(s)+1;char*p=calloc(n,1);if(!p)return 2;memcpy(p,s,n);puts(p);free(p);return 0;}
