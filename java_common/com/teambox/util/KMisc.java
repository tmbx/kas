/*
    Misc static functions
*/

package com.teambox.util;

import java.lang.Thread;
import java.util.*;

public class KMisc
{
    // Sleep for ms_interval milliseconds
    public static void sleep_ms(int ms_interval)
    {
        long begin_time = System.currentTimeMillis();
        long time = begin_time;
        long sleep_time;
        while (true)
        {
            // Calculate remaining time to sleep.
            sleep_time = ms_interval - (time - begin_time);

            // Exit loop if interval has passed.
            if (sleep_time < 0) { break; }
 
            // Sleep
            try { Thread.sleep(sleep_time); }
            catch (InterruptedException e) { }

            // Get current time.
            time = System.currentTimeMillis();
        }
    }

    // Join elements from a collection with a string delimiter.
    public static String list_join(Collection s, String delimiter)
    {
        StringBuffer buffer = new StringBuffer();
        Iterator iter = s.iterator();
        while (iter.hasNext()) 
        {
            buffer.append(iter.next());
            if (iter.hasNext()) { buffer.append(delimiter); }
        }
        return buffer.toString();
    }
}

