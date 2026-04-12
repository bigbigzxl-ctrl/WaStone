/* syscalls.c — minimal newlib syscall stubs for nostdlib + -lc builds */
#include <sys/stat.h>

int _close(int fd)              { (void)fd; return -1; }
int _lseek(int fd, int ptr, int dir) { (void)fd; (void)ptr; (void)dir; return 0; }
int _read(int fd, char *ptr, int len) { (void)fd; (void)ptr; (void)len; return 0; }
int _fstat(int fd, struct stat *st)  { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)             { (void)fd; return 1; }
int _getpid(void)               { return 1; }
int _kill(int pid, int sig)     { (void)pid; (void)sig; return -1; }
