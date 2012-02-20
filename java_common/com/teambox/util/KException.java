/*
   This class can be used for catching custom exceptions without having to create a new 
   class everytime. It's primary goal is to save time.
*/

package com.teambox.util;

public class KException extends Exception
{
    public KException(String s)
    {
        super(s);
    }
}

