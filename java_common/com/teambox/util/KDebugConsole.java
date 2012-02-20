/*
   This class implements the KDebug interface. It logs debug messages to the java console, if the
   debug level is high enough.

   The default debug level is 1.
*/

package com.teambox.util;

import java.lang.System;

// local
import com.teambox.util.KDebug;

public class KDebugConsole implements KDebug
{
    // Debug level
    private int dlevel;

    // Constructors
    public void KDebugConsole()
    {
        this.set_debug_level(1);
    }
    public void KDebugConsole(int dlevel)
    {
        this.set_debug_level(dlevel);
    }

    // Set the debug level
    public void set_debug_level(int dlevel)
    {
        this.dlevel = dlevel;
    }

    // Send debug message to console if debug level is high enough
    public void debug(int dlevel, String namespace, String s)
    {
        if (dlevel <= this.dlevel)
        {
            // Send debug message to java console
            System.out.println(dlevel + ":" + namespace + ":" + s);
        }
    }

    // Send log message to console
    public void log(String s)
    {
        // Send log message to java console
        System.out.println(s);
    }

    // Send error message to console
    public void error(String s)
    {
        // Send error message to java console
        System.err.println(s);
    }
}

