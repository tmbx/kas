/*
   This is a trustmanager that accepts a precise certificate list that is set at run-time.
*/

package com.teambox.ssl;

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.net.*;
import java.util.*;
import java.util.zip.*;
import java.security.cert.*;
import javax.net.ssl.*;

// The behavior of X509TrustManager is essentially unspecified. Hopefully the
// checkServerTrusted() method will be called properly. Don't you love fuzzy
// cryptographic APIs?
public class KasTrustManager implements X509TrustManager {
    
    // The list of certificates to check.
    public Collection check_certs;
    
    public KasTrustManager() {}
    
    // How is this method used? Unspecified.
    public X509Certificate[] getAcceptedIssuers() { return null; } 
    public void checkClientTrusted(X509Certificate[] certs, String authType) {}
    
    // How is this method used? Unspecified. In what order are the certs
    // provided? Unspecified.
    public void checkServerTrusted(X509Certificate[] certs, String authType) 
        throws CertificateException 
    {
        // Convert certs to a collection.
        ArrayList<X509Certificate> certs_list = new ArrayList();
        int i=0;
        while (i<certs.length)
        {
            certs_list.add(certs[i]);
            i++;
        }

        // Check that all checked certs are in the cert list,
        // and compare list lengths for sanity.
        if (!this.checkCertListMatch(this.check_certs, certs_list) || this.check_certs.size() != certs_list.size())
        {
           throw new CertificateException();
        }
    }
 
    // Check that all certiticates in A match one certificate in B.
    public boolean checkCertListMatch(Collection coll_a, Collection coll_b)
        throws CertificateException
    {
        X509Certificate cert_a;
        X509Certificate cert_b;
        Iterator i;
        Iterator j;
        boolean checked;

        i = coll_a.iterator();
        while (i.hasNext())
        {
            cert_a = (X509Certificate) i.next();

            checked = false;

            j = coll_b.iterator();
            while (j.hasNext())
            {
                cert_b = (X509Certificate) j.next();

                // Compare certs.
                if (Arrays.equals(cert_a.getTBSCertificate(), cert_b.getTBSCertificate()))
                {
                    checked = true; 
                    break;
                }
            }
            if (!checked) { return false; }
        }
        return true;
    }
}

