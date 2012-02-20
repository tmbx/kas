/*
   Gets a secured connection to KCD.
   Compatible with:
      - direct connection
      - socks proxies (transparent - to test)
      - unauthenticated http proxies that allow connections to port 443
*/

package com.teambox.kas;

import java.net.*;
import java.util.*;
import java.io.*;
import javax.net.ssl.*;
import java.security.cert.*;

// local
import com.teambox.util.*;
import com.teambox.socket.*;
import com.teambox.ssl.*;

public class KasConn
{
    // Handles communications with javascript
    private KDebug kdebug;

    // Available methods for connecting 
    public static final int METHOD_DIRECT = (1<<0);
    public static final int METHOD_SOCKS = (1<<1);
    public static final int METHOD_HTTP_CONNECT = (1<<2);
    public static final int METHODS_ALL = METHOD_DIRECT | METHOD_SOCKS | METHOD_HTTP_CONNECT;

    // Method used to connect
    private int method;

    // Proxy stuff
    private Proxy proxy = null;

    // Destination host
    private String host = null;
    private InetAddress addr = null;
    private int port = 0;

    // Sockets and IO
    private Socket socket = null;
    private SSLSocket ssl_socket = null;
    private SSLContext sc = null;
    private String ssl_certs_string = null;

    // DNS timeout
    private int dns_timeout_ms = 8000;

    // Misc work stuff.
    int _tries;
    ArrayList <Exception>_exceptions;

    // Constructors
    public KasConn(KDebug o)
    {
        this.kdebug = o;
    }

    // Convert methods bitfield to a string.
    private String methodsToString(int methods)
    {
        String res = "";
        if ((methods & this.METHOD_HTTP_CONNECT) > 0) { res += " http_connect"; }
        if ((methods & this.METHOD_SOCKS) > 0) { res += " socks"; }
        if ((methods & this.METHOD_DIRECT) > 0) { res += " direct"; }
        return res;
    }

    // Connect to the KCD host using the first working method.
    public void dispatchConnect(String host, int port)
        throws Exception
    {
        this.dispatchConnect(host, port, this.METHOD_HTTP_CONNECT);
    }
    public void dispatchConnect(String host, int port, int first_method)
        throws Exception
    {
        this.kdebug.debug(6, "kasconn", "dispatchConnect() called");

        int methods;
        Iterator iter;
        this.host = host;
        this.port = port;
        this._tries = 0;
        this._exceptions = new ArrayList();

        // Try first_method first, then try other methods as fallback.
        for (int i=0; i<2; i++)
        {
            this.kdebug.debug(9, "kasconn", "dispatchConnect(): round " + i);

            if (i == 0) { methods = first_method; }
            else { methods = METHODS_ALL ^ first_method; }

            this.kdebug.debug(8, "kasconn", "dispatchConnect(): methods:" + this.methodsToString(methods));

            if ((methods & this.METHOD_HTTP_CONNECT) > 0)
            {
                // Try http proxies that are set for https.
                if (this.tryHTTPConnect()) { return; }
            }

            if ((methods & this.METHOD_SOCKS) > 0)
            {
                // Try SOCKS.
                if (this.trySOCKSConnect()) { return; }
            }

            if ((methods & this.METHOD_DIRECT) > 0)
            {
                // Try direct connection.
                if (this.tryDirectConnect()) { return; }
            }
        }
        
        if (this._tries > 1) { throw new KException("All connection methods failed."); }
        else { throw this._exceptions.get(this._exceptions.size()-1); }
    }

    private boolean tryHTTPConnect() 
        throws Exception
    {
        // Try all available http proxies.
        Iterator iter = this.getProxies(new String[]{"https", "http"});

        this.method = this.METHOD_HTTP_CONNECT;
        for (;iter.hasNext();)
        {
            this.proxy = (Proxy) iter.next();
            if (this.proxy.type() == Proxy.Type.valueOf("HTTP"))
            {
                this._tries++;
                this.kdebug.debug(3, "kasconn", "Trying proxy '" + this.proxy.toString() + "'.");
                try { this.connect(); return true; }
                catch (SocketTimeoutException e) 
                { 
                    this.kdebug.debug(3, "kasconn", "Connection timeout.");
                    this._exceptions.add(new KException("Connection timeout."));
                }
                catch (Exception e) 
                { 
                    this.kdebug.debug(3, "kasconn", "HTTP CONNECT connection failed: '" + 
                        e.getClass().toString() + ": '" + e.getMessage() + "'");
                    StackTraceElement[] st = e.getStackTrace();
                    for (int sti = 0; sti < st.length; sti++) 
                    { this.kdebug.debug(4, "kasconn", "stack trace: " + st[sti].toString()); }
                    this._exceptions.add(e);
                }
            }
        }

        return false;
    }

    private boolean trySOCKSConnect() 
        throws Exception
    {
        boolean tested_once = false;

        // Detect browser SOCKS proxies.
        Iterator iter = this.getProxies(new String[]{"socket"});

        this.method = this.METHOD_SOCKS;
        for (;iter.hasNext();)
        {
            // Only test once when a SOCKS proxy is found. Do not connect to the proxy... let JAVA
            // handle that for us.
            if (tested_once) { break; }
            this.proxy = (Proxy) iter.next();
            if (this.proxy.type() == Proxy.Type.valueOf("SOCKS"))
            {
                this._tries++;
                tested_once = true;
                this.kdebug.debug(3, "kasconn", "Trying system SOCKS proxy.");
                try { this.connect(); return true; }
                catch (SocketTimeoutException e) 
                { 
                    this.kdebug.debug(3, "kasconn", "Connection timeout."); 
                    this._exceptions.add(new KException("Connection timeout."));
                }
                catch (Exception e) 
                { 
                    this.kdebug.debug(3, "kasconn", "SOCKS connection failed: '" + 
                        e.getClass().toString() + ": '" + e.getMessage() + "'");
                    StackTraceElement[] st = e.getStackTrace();
                    for (int sti = 0; sti < st.length; sti++) 
                    { this.kdebug.debug(4, "kasconn", "stack trace: " + st[sti].toString()); }
                    this._exceptions.add(e);
                }
            }
        }

        return false;
    }

    private boolean tryDirectConnect()
        throws Exception
    {                
        this.kdebug.debug(3, "kasconn", "Trying the DIRECT method...");

        // Try direct connection.
        this.method = this.METHOD_DIRECT;
        this.proxy = null;
        IPV4Resolver r = new IPV4Resolver(this.host, this.dns_timeout_ms);
        if (! r.resolved) 
        {
            this.kdebug.debug(2, "kasconn", "Unable to resolve " + this.host);
            this._exceptions.add(new KException("Could not resolve address '" + this.host + "'."));
        }
        else
        {
            this._tries++;
            this.addr = r.addr;
            try { this.connect(); return true; }
            catch (SocketTimeoutException e) 
            { 
                this.kdebug.debug(3, "kasconn", "Connection timeout."); 
                this._exceptions.add(new KException("Connection timeout."));
            }
            catch (Exception e) 
            { 
                this.kdebug.debug(3, "kasconn", "DIRECT connection failed: '" + 
                    e.getClass().toString() + ": '" + e.getMessage() + "'");
                StackTraceElement[] st = e.getStackTrace();
                for (int sti = 0; sti < st.length; sti++) 
                { this.kdebug.debug(4, "kasconn", "stack trace: " + st[sti].toString()); }
                this._exceptions.add(e);
            }
        }

        return false;
    }

    // Get a unique list of available proxies for the specified uri types.
    // Valid types are: http, https and socket.
    private Iterator getProxies(String[] types) 
        throws KException, URISyntaxException
    {
        URI uri;
        List<Proxy> proxies = new ArrayList();
        List<Proxy> _proxies;

        this.kdebug.debug(9, "kasconn", "getProxies() called");

        // Iterate through the types.
        for (String type : types)
        {
            // Check that the type is valid.
            if (!type.equals("http") && !type.equals("https") && !type.equals("socket"))
            {
                throw new KException("Bad proxy type: " + type);
            }

            // Get proxies list for that uri.
            uri = new URI(type + "://" + this.host + ":" + this.port + "/");
            _proxies = ProxySelector.getDefault().select(uri);

            // Add proxies to the result list if not already there.
            for (Proxy _proxy : _proxies)
            {
                if (!proxies.contains(_proxy)) { proxies.add(_proxy); }
            }
            this.kdebug.debug(9, "kasconn", "getProxies() done.");
        }

        return proxies.listIterator();
    }

    // Get proxy type in a string.
    private String getProxyTypeString(Proxy proxy)
    {
        String types[] = new String[]{"DIRECT", "HTTP", "SOCKS"};

        for (int i=0; i<types.length; i++)
        {
            if (Proxy.Type.valueOf(types[i]).equals(proxy.type())) { return types[i]; }
        }

        return "unknown";
    }

    // Dump all detected proxies for debugging.
    public String dumpProxies()
    {
        Proxy proxy;
        String s="";
        String url_types[] = new String[]{"http", "https", "socket"};
        String url_type;
        Iterator iter;
        for (int i=0; i<url_types.length; i++)
        {
            url_type = url_types[i];
            try 
            {
                iter = this.getProxies(new String[]{url_type});
                for (;iter.hasNext();)
                {
                    proxy = (Proxy) iter.next();
                    String proxy_type = getProxyTypeString(proxy);
                    InetSocketAddress isa = (InetSocketAddress)proxy.address();
                    if (isa != null)
                    {
                        String addr = isa.getHostName();
                        int port = isa.getPort();
                        s += url_type + ":" + proxy_type + ":" + addr + ":" + port + "; ";
                    }
                }
            }
            catch (Exception e)
            {
                this.kdebug.debug(4, "kasconn", "dumpProxies() exception " + 
                    e.getClass().toString() + ": '" + e.getMessage() + "'");
                StackTraceElement[] st = e.getStackTrace();
                for (int sti = 0; sti < st.length; sti++) 
                { this.kdebug.debug(5, "kasconn", "stack trace: " + st[sti].toString()); }
            }
        }
        return s;
    }

    // Establish an SSL socket connection.
    public void connect() 
        throws Exception
    {
        int conn_timeout = 16000; // 16 seconds
        int comm_timeout =  60000; // 1 minute
        
        this.kdebug.debug(6, "kasconn", "sockConnect('" + this.host + "', " + this.port + ") called");

        this.kdebug.debug(2, "kasconn", "Connecting to host '" + this.host + "', port " + this.port + ".");

        if (this.method == this.METHOD_DIRECT || this.method == this.METHOD_SOCKS)
        {
            // Tell JAVA to use / not use system proxies.
            if (this.method == this.METHOD_SOCKS) { System.setProperty("java.net.useSystemProxies","true"); }
            else { System.setProperty("java.net.useSystemProxies","false"); }

            // Direct connect
            this.socket = new Socket();
            this.socket.bind(null);
            InetSocketAddress isa = new InetSocketAddress(this.host, this.port);
            this.socket.connect(isa, conn_timeout);
            this.socket.setSoTimeout(comm_timeout);
            this.SSLize();
        }
        else if (this.method == this.METHOD_HTTP_CONNECT)
        {
            // Tell JAVA to not use system proxies.
            System.setProperty("java.net.useSystemProxies", "false");

            // Connect through a HTTP proxy
            String proxy_host = ((InetSocketAddress)this.proxy.address()).getHostName();
            int proxy_port = ((InetSocketAddress)this.proxy.address()).getPort();

            HTTPConnectSocket _socket = new HTTPConnectSocket();
            _socket.bind(null);
            _socket.connect(this.host, this.port, proxy_host, proxy_port, conn_timeout);
            _socket.setSoTimeout(comm_timeout);
            this.socket = (Socket) _socket;
            this.SSLize();
        }
        else
        {
            throw new KException("Bad connection method.");
        }

        this.kdebug.debug(1, "kasconn", "Connected to host '" + this.host + "', port " + this.port + ".");
    }

    // Close SSL and regular sockets
    public void close()
    {
        if (this.ssl_socket != null && this.ssl_socket.isConnected()) 
        { 
            try { this.ssl_socket.close(); } 
            catch(IOException e) { }
        }
        if (this.socket != null && this.socket.isConnected()) 
        { 
            try { this.socket.close(); } 
            catch(IOException e) { } 
        }
    }

    // Set the SSL certs string used for connection.
    public void setSSLCertsString(String certs_string)
    {
        this.ssl_certs_string = certs_string;
        this.kdebug.debug(4, "kasconn", "Setting the SSL cert. Length: " + this.ssl_certs_string.length());
    }

    // Parse ssl certs from a string.
    private Collection parseCertsString(String certs_string)
        throws CertificateException
    {
        CertificateFactory cf = CertificateFactory.getInstance("X.509");
        Collection certs = new ArrayList();
        String cert_start = "-----BEGIN CERTIFICATE-----";
        String cert_end = "-----END CERTIFICATE-----";

        int s_offset=0, e_offset=0;
        String tmp_str;
        X509Certificate cert;
        InputStream is;

        while (s_offset != -1 && e_offset != -1)
        {
            s_offset = certs_string.indexOf(cert_start, s_offset);
            if (s_offset > -1)
            {
                e_offset = certs_string.indexOf(cert_end, s_offset);
                if (e_offset > -1)
                {
                    tmp_str = certs_string.substring(s_offset, e_offset + cert_end.length());
                    this.kdebug.debug(1, "kasconn", "Parsing certificate.");
                    is = new ByteArrayInputStream(tmp_str.getBytes());
                    cert = (X509Certificate) cf.generateCertificate(is);
                    certs.add(cert);
                    s_offset = e_offset;
                }
            }
        }

        return certs;
    }
                
    // Create an SSL connection with the current connection
    public void SSLize() 
        throws IOException, Exception
    {
        this.kdebug.debug(4, "kasconn", "SSLize() called");

        // Do some checks
        if (this.socket == null || !this.socket.isConnected()) { throw new IOException("Socket is not connected."); }
        if (this.host == null) { throw new IOException("No host provided."); }
        if (this.port == 0) { throw new IOException("No port provided."); }

        // Handle certificate if provided
        if (this.ssl_certs_string != null)
        {
            // Get list of certificates.
            Collection check_certs = this.parseCertsString(this.ssl_certs_string);

            // Create our certificate manager chain.
            KasTrustManager tm[] = { new KasTrustManager() };
            tm[0].check_certs = check_certs;
            sc = SSLContext.getInstance("SSL");
            sc.init(null, tm,  new java.security.SecureRandom());

            this.kdebug.debug(5, "kasconn", "SSL certificate provided... making sure it matches.");
        }
        else
        {
            // No certificate provided... do no check.
            DummyTrustManager tm[] = { new DummyTrustManager() };
            sc = SSLContext.getInstance("SSL");
            sc.init(null, tm,  new java.security.SecureRandom());

            this.kdebug.debug(4, "kasconn", "Warning: no SSL certificate provided... do no verification.");
        }

        // Make an ssl socket out of the non-ssl socket.
        this.kdebug.debug(5, "kasconn", "Creating SSL socket with the current socket.");
        this.ssl_socket = (SSLSocket) sc.getSocketFactory().createSocket(this.socket, this.host, this.port, true);
        this.kdebug.debug(4, "kasconn", "Created SSL socket with the current socket.");

        // Start SSL handshaking
        this.kdebug.debug(4, "kasconn", "Starting SSL handshaking.");
        this.ssl_socket.startHandshake();
        this.kdebug.debug(3, "kasconn", "SSL handshaking done.");
    }

    // Get socket
    public Socket getSocket()
    {
        return this.socket;
    }

    // Get SSL socket
    public SSLSocket getSSLSocket()
    {
        return this.ssl_socket;
    }
}



