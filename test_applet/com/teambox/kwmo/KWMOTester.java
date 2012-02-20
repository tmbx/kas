/*
    This is the KWMO tester applet. It performs tests and send the results to the browser. It currently tests:
        - if java is functionnal (It is if applet loads.)
        - if host can resolve the VNC meta proxy host (If needed only - not used with some methods.)
        - if host can connect to the VNC meta proxy host

    It supports a few ways to connect to the VNC meta proxy host. See this file for more informations:
        - java_common/com/teambox/kas/KasConn.java

    Applet parameters:
        - debug_level (facultative): debug level (default is 1)
        - js_debug_func (facultative): javascript debug function that accepts: (int, string)
            where int is an integer >= 0
        - js_status_func (mandatory): javascript status function that accepts: (int, string)
            where int is an integer in JSCommVNC status codes.
        - js_server_log_func (mandatory): javascript server_log function that accepts: (string)
        - vnc_meta_proxy_host (facultative): host to use when connecting to the VNC meta proxy
        - vnc_meta_proxy_port (mandadory): port to use when connecting to the VNC meta proxy
        - teambox_cert (mandatory): 
             This is the certificate used on the remote server.
             If specified, the applet will verify the cert. Otherwise, it will
             accept to do the connection without verification.
             (The "\n" characters must be replaced by "%".)
*/

package com.teambox.kwmo;

import java.lang.*;
import java.applet.*;
import java.net.*;
import java.io.*;
import java.awt.*;
import java.util.*;
import java.util.zip.*;
import java.security.AccessControlException;
import java.security.cert.*;
import javax.net.ssl.*;

// from kas/java_common
import com.teambox.util.*;
import com.teambox.kas.KasConn;
import com.teambox.vnc.JSCommVNC;

public class KWMOTester extends Applet implements Runnable
{
    // This handles communications with javascript and implements the KDebug interface.
    private JSCommVNC jscomm = null;
    private String js_debug_func = null;
    private int debug_level;    
    private String js_status_func = null;
    private String js_server_log_func = null;

    // Tests thread
    Thread test_thread = null;

    // VNC meta proxy host and port
    String vnc_meta_proxy_host;
    int vnc_meta_proxy_port;
    
    // Teambox cert - accept only SSL connections to that host
    String teambox_cert;

    // KCD connection
    KasConn kasconn;

    // Applet initialization (extends Applet)
    public void init()
    {
        // First part... if this part fails, browser won't know it.
        try
        {
            // Helper for communicating with javascript - implements the KDebug interface
            this.jscomm = new JSCommVNC(this);

            // Helper for communicating with KCD
            this.kasconn = new KasConn(this.jscomm);

            // Get the applet parameters
            this.get_applet_parameters();
        }
        catch (KException e)
        {
            System.out.println("init() KException: '" + e.getMessage() + "'");
            return;
        }
        catch (Exception e)
        {
            System.out.println("init() Exception: " + 
                e.getClass().toString() + ": '" + e.getMessage() + "'");
            e.printStackTrace();
            return;
        }

        // Second part... everything from now on is reported to the browser.
        try
        {
            // Tell KWMO we're alive!
            this.jscomm.debug(1, "vnc_test", "Java tester applet loaded.");
            this.jscomm.status(JSCommVNC.VNC_STATUS_APPLET_LOADED);

            // Start the test thread for network operations
            this.test_thread = new Thread(this);
            this.test_thread.start();
        }
        catch (Exception e)
        {
            this.jscomm.debug(0, "vnc_test", "init() Exception: " + 
                e.getClass().toString() + ": '" + e.getMessage() + "'");
            this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED);
            e.printStackTrace();
        }
    }

    // Threads startup (implements Runnable)
    public void run()
    {
        try
        {
            if (Thread.currentThread() == this.test_thread)
            {
                // Test connection
                this.test_vnc_meta_proxy_conn();
            }
        }
        catch (KException e)
        {
            this.jscomm.debug(0, "vnc_test", "run() KException: '" + e.getMessage() + "'");
            this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, e.getMessage());
        }
        catch (Exception e)
        {
            this.jscomm.debug(0, "vnc_test", "run() Exception: " + 
                e.getClass().toString() + ": '" + e.getMessage() + "'");
            if (e instanceof AccessControlException)
            { this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "user did not authorize applet to load"); }
            else if (e instanceof NoRouteToHostException)
            { this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "no route to server"); }
            else if (e instanceof UnknownHostException)
            { this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "unknown host"); }
            else if (e instanceof ConnectException)
            { this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "could not connect to host"); }
            else
            {
                this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "unexpected error");
                e.printStackTrace();
            }
        }
    }

    // Get all parameters
    private void get_applet_parameters() throws Exception
    {
        String t;

        // Debug level (facultative)
        t = getParameter("debug_level");
        if (t != null && !t.trim().equals("")) { this.debug_level = Integer.parseInt(t.trim()); }
        else { this.debug_level = 1; }
        this.jscomm.set_debug_level(this.debug_level);
        this.jscomm.debug(4, "vnc_test", "Debug level: " + this.debug_level);

        // Javascript debug function (facultative)
        t = getParameter("js_debug_func");
        if (t != null && !t.trim().equals(""))
        {
            this.js_debug_func = t.trim();
            this.jscomm.set_js_debug_func(this.js_debug_func);
        }
        //else { throw new KException("Missing javascript debug function parameter."); }
        this.jscomm.debug(4, "vnc_test", "Javascript debug function: " + this.js_debug_func);

        // Javascript status function (mandatory)
        t = getParameter("js_status_func");
        if (t != null && !t.trim().equals("")) 
        { 
            this.js_status_func = t.trim(); 
            this.jscomm.set_js_status_func(this.js_status_func);
        }
        else { throw new KException("Missing javascript status function parameter."); }
        this.jscomm.debug(4, "vnc_test", "Javascript status function: " + this.js_status_func);

        // Javascript server log function (mandatory)
        t = getParameter("js_server_log_func");
        if (t != null && !t.trim().equals("")) 
        { 
            this.js_server_log_func = t.trim(); 
            this.jscomm.set_js_server_log_func(this.js_server_log_func);
        }
        else { throw new KException("Missing javascript server log function parameter."); }
        this.jscomm.debug(4, "vnc_test", "Javascript server log function: " + this.js_server_log_func);

        // VNC meta proxy port (facultative: default is web server)
        this.vnc_meta_proxy_host = getCodeBase().getHost();
        t = getParameter("vnc_meta_proxy_host");
        if (t != null && !t.trim().equals("")) { this.vnc_meta_proxy_host = t.trim(); }
        //else { throw new KException("Missing vnc_meta_proxy_host parameter."); }
        this.jscomm.debug(4, "vnc_test", "VNC meta proxy host: " + this.vnc_meta_proxy_host);

        // VNC meta proxy port (mandatory)
        t = getParameter("vnc_meta_proxy_port");
        if (t != null && !t.trim().equals("")) { this.vnc_meta_proxy_port = Integer.parseInt(t.trim()); }
        else { throw new KException("Missing vnc_meta_proxy_port parameter."); }
        this.jscomm.debug(4, "vnc_test", "VNC meta proxy port: " + this.vnc_meta_proxy_port);

        // KCD cert (mandatory)
        t = getParameter("teambox_cert");
        if (t != null && !t.trim().equals("")) { this.teambox_cert = t.replace("%", "\n"); }
        else { throw new KException("Missing Teambox cert."); }
        this.jscomm.debug(4, "vnc_test", "Teambox certificate has " + this.teambox_cert.length() + " bytes.");
        this.jscomm.debug(5, "vnc_test", "Teambox certificate is: " + this.teambox_cert);
    }

    // Test connectivity with the VNC meta proxy
    private void test_vnc_meta_proxy_conn()
        throws Exception 
    {
        BufferedReader in = null;
        OutputStream out = null;

        String line = null;
        SSLSocket ss;

        try
        {  
            // Log proxy data
            String dump = kasconn.dumpProxies();
            this.jscomm.debug(3, "vnc_test", "Detected proxies: " + dump);
            this.jscomm.server_log(dump);

            // Connect
            this.kasconn.setSSLCertsString(this.teambox_cert);
            this.kasconn.dispatchConnect(this.vnc_meta_proxy_host, this.vnc_meta_proxy_port);
            ss = this.kasconn.getSSLSocket();
            this.jscomm.debug(4, "vnc_test", "Connected to Teambox Main Application Server.");
            in = new BufferedReader(new InputStreamReader(ss.getInputStream()));
            out = ss.getOutputStream();

            // Send the test command to the socket
            this.jscomm.debug(5, "vnc_test", "Sending test string to Teambox Main Application Server.");
            out.write("VNC!VNC__META__PROXY__LOCAL__TESTING".getBytes());
            this.jscomm.debug(4, "vnc_test", "Sent test string to Teambox Main Application Server.");

            // Get result line
            line = in.readLine();
            this.jscomm.debug(4, "vnc_test", "Read output string: '" + line + "'");

            // Check result string
            if (line.equals("VNC__META__PROXY__LOCAL__TESTING__OK"))
            {
                // Test was successful.
                this.jscomm.debug(1, "vnc_test", "Test passed.");
                this.jscomm.status(JSCommVNC.VNC_STATUS_PASSED);
            }
            else
            {
                // Test failed.
                this.jscomm.debug(0, "vnc_test", "Test failed because of a bad response from Teambox Main Application Server.");
                this.jscomm.status(JSCommVNC.VNC_STATUS_FAILED, "bad response from Teambox Main Application Server");
            }
        }
        finally
        {
            try { this.kasconn.close(); }
            catch (Exception e)
            {
                this.jscomm.debug(0, "vnc_test", 
                    "test_vnc_meta_proxy_conn() exception while closing connection: " + 
                    e.getClass().toString() + ": '" + e.getMessage() + "'");
            }
        }
    }
}


