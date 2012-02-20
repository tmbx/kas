#include "test.h"
#include <inttypes.h>
#include <ktools.h>
#include <anp.h>

struct anp_msg *msg;

UNIT_TEST(anp_msg_write)
{
    kstr str;
    kbuffer buf;

    msg = anp_msg_new();
    msg->major = 1;
    msg->minor = 1;
    msg->type = 42;
    msg->id = 666;

    kstr_init_cstr(&str, "kstr test");
    kbuffer_init(&buf);
    kbuffer_write_cstr(&buf, "kbuffer test");

    anp_msg_write_uint32(msg, 42);
    anp_msg_write_uint64(msg, 5000000000ll);
    anp_msg_write_cstr(msg, "cstr test");
    anp_msg_write_kstr(msg, &str);
    anp_msg_write_bin(msg, &buf);

    kstr_clean(&str);
    kbuffer_clean(&buf);
}

UNIT_TEST(anp_msg_read)
{
    kstr str;
    kbuffer buf;
    uint32_t u32;
    uint64_t u64;

    TASSERT(msg->major == 1);
    TASSERT(msg->minor == 1);
    TASSERT(msg->type == 42);
    TASSERT(msg->id == 666);

    kstr_init(&str);
    kbuffer_init(&buf);

    TASSERT(anp_msg_read_uint32(msg, &u32) == 0 && u32 == 42);
    TASSERT(anp_msg_read_uint64(msg, &u64) == 0 && u64 == 5000000000ll);
    TASSERT(anp_msg_read_kstr(msg, &str) == 0 && strncmp("cstr test", str.data, str.slen) == 0);
    TASSERT(anp_msg_read_kstr(msg, &str) == 0 && strncmp("kstr test", str.data, str.slen) == 0);
    TASSERT(anp_msg_read_bin(msg, &buf) == 0 && memcmp("kbuffer test", buf.data, buf.len) == 0);

    kstr_clean(&str);
    kbuffer_clean(&buf);
}

UNIT_TEST(anp_msg_get)
{
    kstr str;
    kbuffer buf;
    uint32_t u32;
    uint64_t u64;

    kstr_init(&str);
    kbuffer_init(&buf);

    TASSERT(anp_msg_get_kstr(msg, 3, &str) == 0 && strncmp("kstr test", str.data, str.slen) == 0);
    TASSERT(anp_msg_get_uint32(msg, 0, &u32) == 0 && u32 == 42);
    TASSERT(anp_msg_get_bin(msg, 4, &buf) == 0 && memcmp("kbuffer test", buf.data, buf.len) == 0);
    TASSERT(anp_msg_get_uint64(msg, 1, &u64) == 0 && u64 == 5000000000ll);
    TASSERT(anp_msg_get_kstr(msg, 2, &str) == 0 && strncmp("cstr test", str.data, str.slen) == 0);

    kstr_clean(&str);
    kbuffer_clean(&buf);
}

UNIT_TEST(anp_parse)
{
    struct anp_msg *old_msg = msg;
    msg = anp_msg_new();
    anp_msg_parse(msg, &old_msg->payload);
    anp_msg_destroy(old_msg);

    kstr str;
    kbuffer buf;
    uint32_t u32;
    uint64_t u64;

    kstr_init(&str);
    kbuffer_init(&buf);

    TASSERT(anp_msg_read_uint32(msg, &u32) == 0 && u32 == 42);
    TASSERT(anp_msg_read_uint64(msg, &u64) == 0 && u64 == 5000000000ll);
    TASSERT(anp_msg_read_kstr(msg, &str) == 0 && strncmp("cstr test", str.data, str.slen) == 0);
    TASSERT(anp_msg_read_kstr(msg, &str) == 0 && strncmp("kstr test", str.data, str.slen) == 0);
    TASSERT(anp_msg_read_bin(msg, &buf) == 0 && memcmp("kbuffer test", buf.data, buf.len) == 0);

    kstr_clean(&str);
    kbuffer_clean(&buf);
}

UNIT_TEST(anp_msg_dump)
{
    kstr str;
    kstr_init(&str);

    TASSERT(anp_msg_dump(msg, &str) == 0);
    printf("%s\n", str.data);

    kstr_clean(&str);
}

UNIT_TEST(anp_clean)
{
    anp_msg_destroy(msg);
}
