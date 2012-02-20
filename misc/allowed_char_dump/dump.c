#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* Characters allowed in a file name. */
char kcd_kfs_allowed_file_char[256];

/* This function initializes the kcd_kfs_allowed_file_char table. */
void kcd_kfs_initialize_allowed_file_char() {
    int i = 0;
    memset(kcd_kfs_allowed_file_char, 0, 256);
    
    /* Allow some punctuation, letters and numbers. */ 
    kcd_kfs_allowed_file_char[32] = 1;  
    for (i = 35; i <= 46; i++) kcd_kfs_allowed_file_char[i] = 1;
    for (i = 48; i <= 59; i++) kcd_kfs_allowed_file_char[i] = 1;
    kcd_kfs_allowed_file_char[61] = 1;
    for (i = 64; i <= 91; i++) kcd_kfs_allowed_file_char[i] = 1;
    for (i = 93; i <= 123; i++) kcd_kfs_allowed_file_char[i] = 1;
    kcd_kfs_allowed_file_char[125] = 1;  	
    for (i = 192; i <= 229; i++) kcd_kfs_allowed_file_char[i] = 1;
    for (i = 231; i <= 246; i++) kcd_kfs_allowed_file_char[i] = 1;
    for (i = 248; i <= 253; i++) kcd_kfs_allowed_file_char[i] = 1;
    kcd_kfs_allowed_file_char[255] = 1;
}


int main(int argc, char **argv) {
    int i, j;
    kcd_kfs_initialize_allowed_file_char();
    
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            int res = kcd_kfs_allowed_file_char[(i << 4) + j];
            printf("%d", res);
            if (i != 15 || j != 15) printf(",");
            if (j != 15) printf(" ");
        }
        printf("\n");
    }
    
    return 0;
}


