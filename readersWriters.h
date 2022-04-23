
#include "err.h"
typedef struct ReadWrite ReadWrite;

ReadWrite *rw_init();

void destroy(ReadWrite *rw);

void read_prepare(ReadWrite *rw);

void read_close(ReadWrite *rw);

void write_prepare(ReadWrite *rw);

void write_close(ReadWrite *rw);

void clean_prepare(ReadWrite *rw);