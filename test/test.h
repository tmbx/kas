#ifndef __TEST_H__
#define __TEST_H__

#include <stdio.h>

typedef void (*unit_test_t) ();

#define TASSERT(_test_) \
    (_test_) ? \
    	fprintf(stderr, "  [0;32m*[0m %s:%u %s: [0;32mPASSED[0m (%s)\n", __FILE__, __LINE__, __FUNCTION__, #_test_) \
        : \
    	fprintf(stderr, "  [0;31m*[0m %s:%u %s: [0;31mFAILED[0m (%s)\n", __FILE__, __LINE__, __FUNCTION__, #_test_)

#define UNIT_TEST(name) \
    void __test_##name()

#endif /*__TEST_H__*/
