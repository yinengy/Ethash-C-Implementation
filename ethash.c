#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>  // pow
#include <string.h>
#include <inttypes.h> // uint64
#include "lib/sha3.h" // Credit: https://github.com/brainhub/SHA3IUF/blob/master/sha3.h
#include "lib/mt64.h" // http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt64.html

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
#define FNV_PRIME 0x01000193  // fnv() parameter


// Credit: https://stackoverflow.com/questions/8534274/is-the-_strrev-function-not-available-in-linux
//char *_strrev(char *str)
//{
//    if (!str || ! *str)
//        return str;
//
//    int i = strlen(str) - 1, j = 0;
//
//    char ch;
//    while (i > j)
//    {
//        ch = str[i];
//        str[i] = str[j];
//        str[j] = ch;
//        i--;
//        j++;
//    }
//    return str;
//}

// convert a byte array to int
// input: byte array
// output: corresponding int
int decode_int(char* s) {
    if (!s) {
        return 0;
    }

    char hex[9];

    // convert byte array to hex string
    // here consider little endian
    snprintf(hex, 9, "%02x%02x%02x%02x", (unsigned char)s[3],
        (unsigned char)s[2], (unsigned char)s[1], (unsigned char)s[0]);
    int number = (int)strtol(hex, NULL, 16);

    return number;
}


// encode int to byte array
// input: s : int to covert
//        pad_length : = length to pad by '\0' (should not < 4)
// output: byte array
// note: will malloc string
char* encode_int(int s, int pad_length) {
    if (s == 0) {
        return "";
    }

    char hex[8];

    // convert s to hex string
    int hex_len = snprintf(hex, 9, "%x", s);


    // pad 0 at begin to make len be even
    char padded_hex[8];

    // fix bug here: use strcat or strcpy leads to core dump in gem5
    if (hex_len % 2 == 1) {
        padded_hex[0] = '0';
        for (int i = 0; i < hex_len; i++) {
            padded_hex[i + 1] = hex[i];
        }
        hex_len += 1;
    }
    else {
        for (int i = 0; i < hex_len; i++) {
            padded_hex[i] = hex[i];
        }
    }

    // convert hex string to bytearray, little endian
    char* bytearray = malloc(pad_length);
    char bytearray_t[pad_length];
    char* pos = padded_hex;

    int num_pad = pad_length - (hex_len / 2);

    for (int count = 1; count <= (hex_len / 2); count++) {
        sscanf((unsigned char*)pos, "%2hhx", &bytearray[pad_length - num_pad - count]);
        sscanf((unsigned char*)pos, "%2hhx", &bytearray_t[pad_length - num_pad - count]);
        pos += 2;
    }

    // pad '0/'
    for (int count = (hex_len / 2) + 1; count <= pad_length; count++) {
        bytearray[count] = '\0';
        bytearray_t[count] = '\0';
    }

    int a = decode_int(bytearray);
    //if (a != s) {
    //    exit(1);
    //}

    return bytearray;
}


// convert given int array to a long hex encoded byte array
// input: int array and its length
// output: byte array
char* serialize_hash(int* h, int length) {
    char* hash = (char*)malloc(4 * length);

    for (int i = 0; i < length; i++) {
        char* temp = encode_int(h[i], 4);

        strcat(hash, temp);
    }

    return hash;
}


// convert a long hex encoded byte array to int array
// input: byte array and its length
// output int array
// note: will malloc int array
int* deserialize_hash(char* h, int length) {
    // each int correspoding to 4 bytes in bytes array
    int* hash = (int*)malloc(length * sizeof(int));

    // convert each 4 bytes to int
    char* pos = h;

    for (int i = 0; i < strlen(h) / 4; i++) {
        char temp[4];
        strncpy(temp, pos, 4);

        hash[i] = decode_int(temp);
    }

    return hash;
}


// hash the int array by function h
// size is the size of x
// h is a function pointer to hash function
// will return int array
// l is a pointer for returned array, compute by this function
// TODO: check malloc and free
int* hash_words(char* h(char*, int), int size, char* x) {
    char* y = h(x, size);
    return deserialize_hash(y, 16);
}


// same as hash_words(), but x is a int array
int* hash_words_list(char* h(char*, int), int size, int* x) {
    char* temp = serialize_hash(x, size);
    char* y = h(temp, size);
    return deserialize_hash(y, 16);
}


// A warpper for sha3_HashBuffer() in lib/sha3.h
// will be used by sha3_512 as a function pointer
char* sha3_512_wrapper(char* x, int size) {
    char* out = malloc(512);
    sha3_HashBuffer(512, SHA3_FLAGS_KECCAK, x, size, out, 512);
    return out;
}


// sha3 hash function, outputs 64 bytes
// input: a byte array pointer or int array pointer
// if is_list != 0, it means x is a int array
int* sha3_512(void* x, int is_list) {
    if (is_list != 0) {
        return hash_words_list(sha3_512_wrapper, 64, (int*)x);
    }
    return hash_words(sha3_512_wrapper, 32, (char*)x);
}


struct Block {
    int number;
};

int fnv(int v1, int v2) {
    return (int)pow((double)(((v1 * FNV_PRIME) ^ v2) % 2), (double)32);
}

// generate one element in dataset
// input: cache: int array, generated by mkcache
//        len: length of cache
//        i: index of this element in dataset
// output: int array
unsigned int* calc_dataset_item(unsigned int** cache, int len, int i) {
    int r = HASH_BYTES / WORD_BYTES;
    // initialize the mix
    unsigned int mix[16];
    memcpy(mix, cache[i % len], 64);
    mix[0] ^= i;

    // sha3 malloc int array, so need to free it
    unsigned int* temp = sha3_512(mix, 1);
    memcpy(mix, temp, 64);
    free(temp);

    // fnv it with a lot of random cache nodes based on i
    for (int j = 0; j < DATASET_PARENTS; j++) {
        int cache_index = fnv(i ^ j, mix[j % r]);

        for (int k = 0; k < 16; k++) {
            mix[k] = fnv(mix[k], cache[cache_index % len][k]);
        }

    }

    return sha3_512(mix, 1);
}

// generate (typically 1GB) dataset based on (typically 16MB) cache
// input: full_size: dataset size
//        cache: int array, generated by mkcache
//        cache_size: size of cache
// output: int array
unsigned int** calc_dataset(int full_size, unsigned int** cache, int cache_size) {
    int loop_times = full_size / HASH_BYTES;
    unsigned int** o = malloc(sizeof(int*) * loop_times);

    for (int i = 0; i < loop_times; i++) {
        o[i] = calc_dataset_item(cache, cache_size / 64, i);
    }

    return o;
}


// generate cache
// input: cache size and seed
// output: int array
unsigned int** mkcache(int cache_size, char* seed) {
    int n = cache_size / HASH_BYTES;

    // Sequentially produce the initial dataset
    unsigned int** o = malloc(sizeof(int*) * n);
    o[0] = sha3_512(seed, 0);

    for (int i = 1; i < n; i++) {
        o[i] = sha3_512(o[i - 1], 1);
    }

    // Use a low - round version of randmemohash
    for (int i = 0; i < CACHE_ROUNDS; i++) {
        for (int j = 0; j < n; j++) {
            unsigned int v = o[j][0] % n;

            // map xor over o[(i - 1 + n) % n], o[v]
            unsigned int temp[16];
            for (int k = 0; k < 16; k++) {
                temp[k] = o[(j - 1 + n) % n][k] ^ o[v][k];
            }
            o[j] = sha3_512(temp, 1);
        }
    }

    return o;
}



// generate seedhash based on block number
// input: block struct
char* get_seedhash(struct Block block) {
    char* s = malloc(32);
    for (int i = 0; i < 32; i++) {
        s[i] = '\0';
    }

    // no need to implement this for block.number = 1
    // for (int i = 0; i < block.number / EPOCH_LENGTH; i++) {
    //     s = 
    // }
    return s;
}


// mine a block
// input: full_size: size of dataset
//        dataset: int array
//        header: header of the block
//        difficulty: difficulty to mine the block
// output: nonce, if not found in given times, return 0
// Note: difficulty is acutally a fixed number in this function, see comment below
uint64_t mine(int full_size, int** dataset, char* header, int difficulty) {
    // in python: "2 ** 256 // difficulty" = 256
    // no int256 support in C, so difficulty actually is fixed in this program
    // TODO: should be fixed in the future
    // currently can only do this calculation by hand
    int target = 256;

    // randint(0, 2 ** 64)
    uint64_t nonce = genrand64_int64();

    int i = 0;

    while (decode_int(hashimoto_full(full_size, dataset, header, nonce)) > 256) {
        // in python "nonce = (nonce + 1) % 2 ** 64"
        // no need to do the mod by exploiting the overflow in uint64_t
        nonce += 1;
        i += 1;

        if (i > 10) {
            printf("tried 10 times without finding solution, give up.\n");
            return 0;
        }
    }

    printf("tried %d times. Found solution with nonce = %x\n", i, nonce);

    return nonce;
}


int main() {
    int header_size = 508 + 8 * 5;

    struct Block block = { 1 };

    // create byte array with header_size
    char* header = malloc(header_size);
    for (int i = 0; i < header_size; i++) {
        header = '\0';
    }

    // difficulty in genesis block
    // Credit: https://lightrains.com/blogs/setup-local-ethereum-blockchain-private-testnet
    int difficulty = 0x4000;

    int cache_size = 1677;
    int full_size = 16776;
    char* seedhash = get_seedhash(block);
    printf("Step (1/3): Make cache (around 16MB)... \n");
    unsigned int** cache = mkcache(cache_size, seedhash);
    printf("Step (1/3) finished.\n");
    printf("Step (2/3): Make dataset (around 1GB)... May takes several hours to do so\n");
    unsigned int** dataset = calc_dataset(full_size, cache, cache_size);
    printf("Step (2/3) finished.\n");
    printf("Step (3/3) mine a block...");
    uint64_t nonce = mine(full_size, dataset, header, difficulty);
    printf("Step (3/3) finished.\n");
    printf("\nAlgorithm ends.\n");

    return 0;
}