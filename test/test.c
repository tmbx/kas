#ifndef __windows__
# define _GNU_SOURCE
#endif /*__windows__*/

#include <ktools.h>
#include <kerror.h>
#include <stdio.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdlib.h>
#include <signal.h>
#include "test.h"

/* A null terminated array of unit test functions */
extern const unit_test_t unit_test_array[];

void start_unit_test(unit_test_t func) {
#ifdef _GNU_SOURCE
    int retval;
    Dl_info info;
    dlerror();
    retval = dladdr(func, &info);
    if (retval == 0) {
        printf ("0x%X:\n", (unsigned int)func);
    } else
        printf("%s:\n", info.dli_sname);
#else
    printf ("0x%X:\n", (unsigned int)func);
#endif
    func();
}

void signal_handler(int sig) {
    void *array[20];
    int i, size = 20;
    char **strings;
    static int currently_handling = 0;

    /* Handle recursive segfault */
    if (currently_handling)
        exit(1);

    currently_handling = 1;

    fprintf(stderr, "  [0;31m***[0m Received signal #%d\n", sig);

    size = backtrace(array, size);
    strings = backtrace_symbols (array, size);

    for (i = 0; i < size; i++)
        printf ("    %s\n", strings[i]);

    free (strings);

    exit(-1);
}

int main (int UNUSED(argc), char UNUSED(**argv)) {
    const unit_test_t *func;
    ktools_initialize();

    signal(SIGSEGV, signal_handler);

    for (func = unit_test_array; *func != NULL; func++) {
        start_unit_test(*func);
    }

    ktools_finalize();
    return 0;
}
