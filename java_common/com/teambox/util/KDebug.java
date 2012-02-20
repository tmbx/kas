/*
   This class is a generic debug interface.
*/

package com.teambox.util;

public interface KDebug
{
    void set_debug_level(int dlevel);

    void debug(int dlevel, String namespace, String s);

    void log(String s);

    void error(String s);
}

