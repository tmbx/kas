/* Copyright (C) 2006-2012, Opersys Inc., All rights reserved.
 *
 * The protocol version is specified with a major and minor number. The ANP
 * format itself is not affected by these version numbers. The version is
 * expected to be used by higher-level protocols.
 * 
 * The following basic data types are defined by the ANP (explanations follow):
 *    UINT32: a 32 bits unsigned integer in network byte order.
 *    UINT64: a 64 bits unsigned integer in network byte order.
 *    STR: an UTF-8 string.
 *    BIN: a binary blob of data.
 *
 *    All basic types are identified by a single-byte integer code before they
 *    are sent on the network. For instance, the integer code corresponding to
 *    'UINT32' is '1'. Thus, the 32 bits value '42' would be encoded as
 *    0x0100000042 on the network.
 *
 *    The 'STR' type allows the transfer of UTF-8 strings. A 32 bits integer in
 *    network byte order yielding the length of the string, not including the
 *    terminating zero, is encoded. Then, the string data itself is encoded, not
 *    including the terminating zero.
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

#ifndef _ANP_CORE_DEFS_H
#define _ANP_CORE_DEFS_H


/* ANP object identifiers. */
#define ANP_UINT32		    1
#define ANP_UINT64		    2
#define ANP_STR			    3
#define ANP_BIN			    4

/* Size in bytes of an ANP message header. */
#define ANP_MSG_HDR_SIZE	    (4*4 + 8*1)

/* Suggested maximum size of a ANP payload (20 MB). */
#define ANP_MAX_PAYLOAD_SIZE	    (20*1024*1024)


#endif
