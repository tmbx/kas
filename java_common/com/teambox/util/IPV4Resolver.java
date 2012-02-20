/*
    This class allows to do an IPV4 dns lookups with a timeout.

    Usage:
        IPV4Resolver r = IPV4Resolver(host, timeout_ms);
        
        Public attributes:
            (boolean) resolved
            (InetAddress) addr

    Notes:
        - The timeout is ignored if greater than the java dns timeout.
        - The resolver thread does not shutdown at the same moment a timeout is reached because 
          Inet4Address.getByName() blocks and can't be interrupted. The said thread will 
          most probably timeout later.
          FIXME: Fix that if possible
*/

package com.teambox.util;

import java.lang.Thread;
import java.net.*;

public class IPV4Resolver
    implements Runnable
{
    // Resolver thread.
    private Thread resolver = null;

    // Misc
    private InetAddress _addr = null;
    private String host = null;
    private int timeout_ms = 0;

    // Resulting address.
    public boolean resolved = false;
    public InetAddress addr = null;

    // Constructor
    public IPV4Resolver(String host, int timeout_ms)
    {
        this.host = host;
        this.timeout_ms = timeout_ms;

        // Start resolver thread.
        this.resolver = new Thread(this);
        this.resolver.start();

        // Wait for notify or until timeout.
        try { synchronized(this) { this.wait(this.timeout_ms); } }
        catch (InterruptedException e) { }

        // Get the resolved or unresolved address from the resolver thread.
        synchronized(this) 
        {
            this.addr = this._addr; 
            if (this.addr != null) { this.resolved = true; }
        } 
    }

    // Threads
    public void run()
    {
        if (Thread.currentThread() == this.resolver)
        {
            // This is the resolver thread.

            InetAddress tmp_addr = null;
            
            // Try to resolve host name.
            try { tmp_addr = (Inet4Address) Inet4Address.getByName(this.host); }
            catch (UnknownHostException e) { }

            // Notify calling class that lookup is done.
            synchronized(this)
            {
                this._addr = tmp_addr;
                this.notify();
            }
        }
    }

    // For testing as a standalone resolver...
    public static void main(String args[])
    {
        for (int i = 0; i<args.length; i++)
        {
            IPV4Resolver r = new IPV4Resolver(args[i], 5000);
            if (r.resolved) { System.out.println("Host " + args[i] + ": " + r.addr); }
            else { System.out.println("Host " + args[i] + ": <unresolved>"); }
        }
    }
}


