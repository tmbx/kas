// Socket through HTTP Connect

package com.teambox.socket;

import java.net.*;
import java.io.*;

import com.teambox.util.*;

// TODO: implement authentified connections

public class HTTPConnectSocket extends Socket
{
    private String kproxy_host = null;
    private int kproxy_port = 0;
    private String kremote_host = null;
    private int kremote_port = 0;
    private boolean kconnected = false;
    private int ktimeout_ms = 0;

    public HTTPConnectSocket()
        throws IOException
    {
        super();
    }

    public boolean isConnected()
    {
        return this.kconnected;
    }

    public void connect(String host, int port, String proxy_host, int proxy_port)
        throws IOException, KException
    {
        this.kproxy_host = proxy_host;
        this.kproxy_port = proxy_port;
        this.kremote_host = host;
        this.kremote_port = port;

        this.kconnect();
    }

    public void connect(String host, int port, String proxy_host, int proxy_port, int timeout_ms)
        throws IOException, KException
    {
        this.kproxy_host = proxy_host;
        this.kproxy_port = proxy_port;
        this.kremote_host = host;
        this.kremote_port = port;
        this.ktimeout_ms = timeout_ms;

        this.kconnect();
    }

    public void kconnect()
        throws IOException, KException
    {
        OutputStream os;

        if (this.kproxy_host == null) { throw new IOException("No proxy host set"); }
        if (this.kproxy_port == 0) { throw new IOException("No proxy port set"); }
        if (this.kremote_host == null) { throw new IOException("No remote host set"); }
        if (this.kremote_port == 0) { throw new IOException("No remote port set"); }

        InetSocketAddress addr = new InetSocketAddress(this.kproxy_host, this.kproxy_port);

        // Connect to HTTP proxy
        if (this.ktimeout_ms == 0) { super.connect(addr); }
        else { super.connect(addr, this.ktimeout_ms); }

        this.kconnected = true;

        // Send the CONNECT request
        os = this.getOutputStream();
        os.write(("CONNECT " + this.kremote_host + ":" + this.kremote_port + " HTTP/1.0\r\n\r\n").getBytes());
        os.flush();

        // Read the first line of the response
        BufferedReader is = new BufferedReader(new InputStreamReader(getInputStream()));
        String str = is.readLine();

        // Check the HTTP code - it should be "200" on success
        // Bug? ISA reports HTTP/1.1 200 even if request is HTTP/1.0
        // As we're opening a permanent tunnel and not fetching web pages, this should not change anything.
        if (!str.startsWith("HTTP/1.0 200 ") && !str.startsWith("HTTP/1.1 200 "))
        {
            if (str.startsWith("HTTP/1.0 407 ") || str.startsWith("HTTP/1.1 407 "))
            {
                throw new KException("Proxy requires authentication.");
            }
            else
            {
                throw new IOException("Proxy error '" + str + "'");
            }
        }

        // Success -- skip remaining HTTP headers
        do
        {
            str = is.readLine();
        } while (str.length() != 0);

        //this.kconnected = true;
    }
}

