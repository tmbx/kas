/* Copyright (C) 2006-2012, Opersys Inc., All rights reserved.
 *
 * The protocol version is specified with a major and minor number. It is
 * currently ignored.
 * 
 * The following basic data types are defined by the ANP (explanations follow):
 *    UINT32: a 32 bits unsigned integer in network byte order.
 *    UINT64: a 64 bits unsigned integer in network byte order.
 *    STR: an ISO-8859-1 string.
 *    BIN: a binary blob of data.
 *
 *    All basic types are identified by a single-byte integer code before they
 *    are sent on the network. For instance, the integer code corresponding to
 *    'UINT32' is '1'. Thus, the 32 bits value '42' would be encoded as
 *    0x0100000042 on the network.
 *
 *    The 'STR' type allows the transfer of ISO-8859-1 strings. A 32 bits
 *    integer in network byte order yielding the length of the string, not 
 *    including the terminating zero, is encoded. Then, the string data itself
 *    is encoded, not including the terminating zero.
 *
 *    The 'BIN' type allows the transfer of raw binary data. A 32 bits integer
 *    in network byte order yielding the size of the binary data is encoded,
 *    then the binary data itself is encoded.
 *    
 *
 * The format of a ANP message is as follow:
 *    Message header:
 *       Protocol major number: 32 bits integer in network byte order.
 *       Protocol minor number: 32 bits integer in network byte order.
 *       Message type:		32 bits integer in network byte order.
 *       Message ID:	        64 bits integer in network byte order.
 *       Payload size:		32 bits integer in network byte order.
 *    Message payload:
 *       <Message-specific data>
 *
 *    The message type identifies how the data in the ANP message should be
 *    interpreted. The message ID is used to map reply messages to their
 *    corresponding request messages. The payload size is the length of that
 *    data, not counting the data in the header.
 */

#ifndef _ANP_H_
#define _ANP_H_

/* Size of an ANP message header. */
#define ANP_MSG_HDR_SIZE 24

/* Maximum size of an ANP message payload. */
#define ANP_MSG_MAX_SIZE (102*1024*1024)

/* ANP object identifiers. */
enum anp_type
{
    ANP_UINT32 = 1,
    ANP_UINT64 = 2,
    ANP_STR = 3,
    ANP_BIN = 4,
};

struct anp_element
{
    enum anp_type type;
    union
    {
        uint32_t uint32;
        uint64_t uint64;
        kstr *str;
        kbuffer *bin;
    };
};

/* This structure represents a message to exchange between KCD and a client. */
struct anp_msg {
    
    /* Major and minor protocol version. */
    uint32_t major;
    uint32_t minor;
    
    /* Type of the message. */
    uint32_t type;
    
    /* Identifier of the message. */
    uint64_t id;
    
    /* Payload of the message. */
    kbuffer payload;

    /* Position in the element array, to allow functions to read a message
     * element-by-element.
     */
    int pos;

    /* The elements in the message. */
    karray element_array;
};

char* anp_type_name(enum anp_type type);

struct anp_element* anp_element_new();
void anp_element_clean(struct anp_element *self);
void anp_element_destroy(struct anp_element *self);

void anp_write_uint32(kbuffer *buf, uint32_t i);
void anp_write_uint64(kbuffer *buf, uint64_t i);
void anp_write_kstr(kbuffer *buf, kstr *str);
void anp_write_cstr(kbuffer *buf, char *str);
void anp_write_bin(kbuffer *buf, kbuffer *bin);
int anp_read_element(kbuffer *buf, struct anp_element *el);
int anp_read_uint32(kbuffer *buf, uint32_t *i);
int anp_read_uint64(kbuffer *buf, uint64_t *i);
int anp_read_kstr(kbuffer *buf, kstr *str);
int anp_read_bin(kbuffer *buf, kbuffer *bin);
int anp_dump(kbuffer *buf, kstr *dump_str);

struct anp_msg* anp_msg_new();
void anp_msg_destroy(struct anp_msg *self);
void anp_msg_clear_payload(struct anp_msg *self);
int anp_msg_parse(struct anp_msg *self, kbuffer *buf);
void anp_msg_to_buf(struct anp_msg *msg, kbuffer *buf);
int anp_msg_dump(struct anp_msg *self, kstr *dump_str);
void anp_msg_write_uint32(struct anp_msg *self, uint32_t i);
void anp_msg_write_uint64(struct anp_msg *self, uint64_t i);
void anp_msg_write_kstr(struct anp_msg *self, kstr *str);
void anp_msg_write_cstr(struct anp_msg *self, char *str);
void anp_msg_write_bin(struct anp_msg *self, kbuffer *bin);
int anp_msg_read_uint32(struct anp_msg *self, uint32_t *i);
int anp_msg_read_uint64(struct anp_msg *self, uint64_t *i);
int anp_msg_read_kstr(struct anp_msg *self, kstr *str);
int anp_msg_read_bin(struct anp_msg *self, kbuffer *bin);
int anp_msg_get_uint32(struct anp_msg *self, int pos, uint32_t *i);
int anp_msg_get_uint64(struct anp_msg *self, int pos, uint64_t *i);
int anp_msg_get_kstr(struct anp_msg *self, int pos, kstr *str);
int anp_msg_get_bin(struct anp_msg *self, int pos, kbuffer *bin);

#endif
