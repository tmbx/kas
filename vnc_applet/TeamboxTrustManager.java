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
class TeamboxTrustManager implements X509TrustManager {
    
    // The only certificate we trust.
    public X509Certificate trustedCert;
    
    public TeamboxTrustManager() {
    }
    
    // How is this method used? Unspecified.
    public X509Certificate[] getAcceptedIssuers() { return null; } 
    public void checkClientTrusted(X509Certificate[] certs, String authType) {}
    
    // How is this method used? Unspecified. In what order are the certs
    // provided? Unspecified.
    public void checkServerTrusted(X509Certificate[] certs, String authType) throws CertificateException {
        
        // Assuming only one certificate for security.
        if (certs.length != 1) throw new IllegalArgumentException();
        
        // Compare trusted and received certificates for strict equality.
        if (! Arrays.equals(trustedCert.getTBSCertificate(), certs[0].getTBSCertificate()))
            throw new CertificateException();
    }
}

