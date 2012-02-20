"""
This library contains code for TCP client communications.
"""

# from system
import time
import socket
import OpenSSL
from OpenSSL import SSL
import logging

# Put after imports so log is not overwridden by an imported module.
log = logging.getLogger(__name__)

# Maximum bytes to read or write at once
TCP_CLIENT_CHUNK_SIZE = 1024 * 1024

class TcpClient(object):
    """
    Tcp client

    Must use open_ssl to allow dh.
    """

    def __init__(self, host, port, use_ssl=False, use_openssl=False, allow_dh=False):
        log.debug("%s: initializing..." % ( str(self) ) ) 
        self.host = host
        self.port = port

        self.sock = None
        self.use_ssl = use_ssl
        self.ssl_sock = None
        self.use_openssl = use_openssl
        self.openssl_ctx = None
        self.openssl_conn = None
        self.allow_dh = allow_dh

    def connect(self):
        log.debug("%s: connecting: host='%s' port='%s' ssl='%s' openssl='%s' allow_dh='%s'" % \
            ( str(self), str(self.host), str(self.port), str(self.use_ssl), str(self.use_openssl), str(self.allow_dh) ) )

        if self.use_openssl:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.openssl_ctx = SSL.Context(SSL.TLSv1_METHOD)
            if self.allow_dh:
                # Add DH cipher
                # FIXME - should get the right string to add only DH... even if this is what openssl does anyways
                # according to the current documentation.
                self.openssl_ctx.set_cipher_list("ALL")
            self.openssl_conn = SSL.Connection(self.openssl_ctx, self.sock)
            self.openssl_conn.connect((self.host, self.port))

        elif self.use_ssl:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.ssl_sock = socket.ssl(self.sock)
        
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))

        log.debug("%s: connected: host='%s' port='%i' ssl='%s' openssl='%s' allow_dh='%s'" % \
            ( str(self), self.host, self.port, str(self.use_ssl), str(self.use_openssl), str(self.allow_dh) ) )

    def close(self):
        log.debug("%s: close() call" % ( str(self) ) )

        if self.sock:
            log.debug("%s: closing connection" % ( str(self) ) )
            if self.use_openssl:
                self.openssl_conn.close()

            else:
                self.sock.close()

            self.sock = None
            self.ssl_ctx = None
            self.gnutls_sess = None

        log.debug("%s: closed" % ( str(self) ) )

    def read(self, bytes_to_read=None):
        if bytes_to_read:
            log.debug("%s: reading '%i' bytes" % ( str(self), bytes_to_read ) )
        else:
            log.debug("%s: reading all bytes" % ( str(self) ) )

        parts = []
        read_bytes = 0
        i = 0

        while 1:
            i += 1
            log.debug("%s: read(): loop %i" % ( str(self), i ) )

            n = TCP_CLIENT_CHUNK_SIZE
            if bytes_to_read and ((bytes_to_read - read_bytes) < TCP_CLIENT_CHUNK_SIZE):
                n = bytes_to_read - read_bytes

            log.debug("%s: trying to read '%i' bytes." % ( str(self), n ) )

            if self.use_openssl:
                try:
                    part = self.openssl_conn.recv(n)
                except OpenSSL.SSL.SysCallError, e:
                    if int(e[0]) == -1: # EOF
                        log.debug("%s: read EOF" % ( str(self) ) )
                        break
                    raise

            elif self.use_ssl:
                try:
                    part = self.ssl_sock.read(n)
                except socket.sslerror, e:
                    if e[0] == socket.SSL_ERROR_ZERO_RETURN or e[0] == socket.SSL_ERROR_EOF:
                        log.debug("%s: read EOF" % ( str(self) ) )
                        break

            else:
                part = self.sock.recv(n)
             
            tmp_read_bytes = len(part)
            if tmp_read_bytes > 0:
                read_bytes += tmp_read_bytes
                parts.append(part)

                log.debug("%s: read '%i' bytes." % ( str(self), n ) )

            else:
                log.debug("%s: read EOF" % ( str(self) ) )
                break

            if bytes_to_read == read_bytes:
                break

            time.sleep(0.001)

        return "".join(parts)

    def write(self, bytes):
        bytes_to_write = len(bytes)
        log.debug("%s: writing '%i' bytes" % ( str(self), bytes_to_write ) )

        bytes_written = 0
        i = 0

        while 1:
            i += 1
            log.debug("%s: write(): loop %i" % ( str(self), i ) )

            n = TCP_CLIENT_CHUNK_SIZE
            if (bytes_to_write - bytes_written) < TCP_CLIENT_CHUNK_SIZE:
                n = bytes_to_write - bytes_written

            s = bytes[bytes_written:bytes_written+n]

            # Don't uncomment unless you know what you're doing.
            #log.debug("%s: write(): writing bin '%s'" % ( str(self), s.encode('hex') ) )

            if self.use_openssl:
                tmp_bytes_written = self.openssl_conn.send(bytes[bytes_written:bytes_written+n])
            elif self.use_ssl:
                tmp_bytes_written = self.ssl_sock.write(bytes[bytes_written:bytes_written+n])
            else:
                tmp_bytes_written = self.sock.send(bytes[bytes_written:bytes_written+n])

            log.debug("%s: write(): wrote '%i' bytes" % ( str(self), tmp_bytes_written ) )

            bytes_written += tmp_bytes_written
            if bytes_written == bytes_to_write:
                break
            
            time.sleep(0.001)

        return bytes_written


# Unexaustive testing
def tcp_client_test():
    c = TcpClient("www.google.com", 443, use_openssl=True)
    c.connect()
    c.write("GET /\n\n")
    print "GOT %i bypes" % ( len(c.read()) )
    c.close()

    c = TcpClient("www.google.ca", 80)
    c.connect()
    c.write("GET /\n\n")
    print "GOT %i bypes" % ( len(c.read()) )
    c.close()

if __name__ == "__main__":
    tcp_client_test()

