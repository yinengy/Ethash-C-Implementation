#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#define WORD_BYTES 4  // bytes in word
#define DATASET_BYTES_INIT (1 << 30)  // bytes in dataset at genesis
#define DATASET_BYTES_GROWTH (1 << 23)  // dataset growth per epoch
#define CACHE_BYTES_INIT (1 << 24)  // bytes in cache at genesis
#define CACHE_BYTES_GROWTH (1 << 17)  // cache growth per epoch
#define CACHE_MULTIPLIER 1024  // Size of the DAG relative to the cache
#define EPOCH_LENGTH 30000  // blocks per epoch
#define MIX_BYTES 128  // width of mix
#define HASH_BYTES 64  // hash length in bytes
#define DATASET_PARENTS 256  // number of parents of each dataset element
#define CACHE_ROUNDS  3  // number of rounds in cache production
#define ACCESSES 64  // number of accesses in hashimoto loop


// Credit: https://stackoverflow.com/questions/8534274/is-the-strrev-function-not-available-in-linux
char *strrev(char *str)
{
    if (!str || ! *str)
        return str;

    int i = strlen(str) - 1, j = 0;

    char ch;
    while (i > j)
    {
        ch = str[i];
        str[i] = str[j];
        str[j] = ch;
        i--;
        j++;
    }
    return str;
}

// convert a hex encoded byte array to int
int decode_int(char * s) {
    if (!s) {
        return 0;
    }

    char hex[8];

    // convert byte array to hex string
    // here consider little endian
    sprintf(hex, "%02X%02X%02X%02X", s[3], s[2], s[1], s[0]);

    int number = (int) strtol(hex, NULL, 16);

    return number;
}

// return a hex encoded byte array of the int
// will malloc string
char *encode_int(int s) {
    if (s == 0) {
        return "";
    }

    char hex[8];

    // convert s to hex string
    int hex_len = sprintf(hex, "%x", s);

    // pad 0 at begin to make len be even
    char padded_hex[8];

    if (hex_len % 2 == 1) {
        strcpy(padded_hex, "0"); 
        strcat(padded_hex, hex);
    }

    // convert hex string to bytearray
    char bytearray[4];
    char *pos = padded_hex;

    for (size_t count = 0; count < 4; count++) {
        sscanf(pos, "%2hhx", &bytearray[count]);
        pos += 2;
    }

    char * r = (char *) malloc(strlen(bytearray) + 1);
    strcpy(r, strrev(bytearray)); // little endian
    
    return r; 
}

// pad \x00 at the end of the string to length
// will malloc string
char *zpad(char *s, int length) {
    int num_pad = length - strlen(s);

    if (num_pad <= 0) {
        return s;
    }

    char *padded_hex = (char *) malloc(num_pad * 2 + strlen(s) + 1);

    strcpy(padded_hex, s); 

    // pad zeros (little endian, so at the end)
    for (int i = 0; i < num_pad; i++) {
        strcat(padded_hex, "\\x00"); // TODO: check this
    }
        
    return padded_hex;
}

// convert given int array to a long hex encoded byte array
char *serialize_hash(int *h, int length) {
    char *hash = (char *) malloc(4 * length + 1);

    for (int i = 0; i < length; i++) {
        char *temp = encode_int(h[i]);
        char *temp2 = zpad(temp, 4);

        strcat(hash, temp2);

        free(temp);
        free(temp2);
    }

    return hash;
}

struct Block {
    int number;
};