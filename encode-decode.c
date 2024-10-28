#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include"encode(compression).h"

static const int mod_table[] = {0, 2, 1};

static const char base64_chars[] = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


#define chunk_size 2064

static const unsigned char base64_decode_table[256] = {
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,
    ['E'] = 4,  ['F'] = 5,  ['G'] = 6,  ['H'] = 7,
    ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
    ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27,
    ['c'] = 28, ['d'] = 29, ['e'] = 30, ['f'] = 31,
    ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
    ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39,
    ['o'] = 40, ['p'] = 41, ['q'] = 42, ['r'] = 43,
    ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
    ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51,
    ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
    ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63
};

// Function to write buffer to a file
int write_file(const char *filename, const unsigned char *data, size_t data_size) {
    FILE *file = fopen(filename, "ab");  // "ab" to append in binary mode
    if (!file) {
        perror("Failed to open file for writing");
        return 1;
    }

    size_t written = fwrite(data, 1, data_size, file);
    if (written != data_size) {  // Check if all data was written
        perror("Error writing data to file");
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

char *base64_encode_chunk(const unsigned char *data, size_t input_length, size_t *output_length) {
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length + 1); // +1 for null terminator
    if (encoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t combined = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = base64_chars[(combined >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(combined >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(combined >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[combined & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
    {    
        encoded_data[*output_length - 1 - i] = '=';
    }
       
 
    encoded_data[*output_length] = '\0';  // Null terminator
    return encoded_data;
}

// Function to read, encode, and write the file in chunks
int base64_encode_file(const char *input_file, const char *output_file,size_t* compressed_size) {
    FILE *in_file = fopen(input_file, "rb");
    FILE *out_file = fopen(output_file, "wb");
    if (!in_file || !out_file) {
        perror("Failed to open file");
        return 1;
    }

    char buffer[chunk_size];
    //char encoded_data[chunk_size];
    memset(buffer,0,chunk_size);
    //memset(buffer,0,encoded_data);
    if (!buffer) {
        perror("Failed to allocate buffer");
        fclose(in_file);
        fclose(out_file);
        return 1;
    }

    size_t read_size;
    while ((read_size = fread(buffer, 1, chunk_size, in_file)) > 0) {
        size_t encoded_size;
        char *encoded_data = base64_encode_chunk(buffer, read_size, &encoded_size);
        if (!encoded_data) {
            perror("Failed to encode chunk");
            fclose(in_file);
            fclose(out_file);
            return 1;
        }
        
        fwrite(encoded_data, 1, encoded_size, out_file);  // Write encoded data to output file
        free(encoded_data);  // Free encoded data after writing
        memset(buffer,0,chunk_size);
    }

    fseek(out_file, 0, SEEK_END);
    *compressed_size = ftell(out_file);
    fseek(out_file, 0, SEEK_SET);


    fclose(in_file);
    fclose(out_file);
    return 0;
}


size_t base64_decode_chunk(const char *input, size_t input_length, unsigned char *output) {
    size_t output_length = input_length / 4 * 3;
    if (input[input_length - 1] == '=') output_length--;
    if (input[input_length - 2] == '=') output_length--;

    for (int i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_decode_table[(unsigned char)input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_decode_table[(unsigned char)input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_decode_table[(unsigned char)input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_decode_table[(unsigned char)input[i++]];

        uint32_t combined = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        if (j < output_length) output[j++] = (combined >> 16) & 0xFF;
        if (j < output_length) output[j++] = (combined >> 8) & 0xFF;
        if (j < output_length) output[j++] = combined & 0xFF;
    }

    return output_length;
}

void base64_decode_file(const char *input_filename, const char *output_filename) {
    FILE *input_file = fopen(input_filename, "rb");
    FILE *output_file = fopen(output_filename, "wb");
    if (!input_file || !output_file) {
        perror("Failed to open input file");
        return;
    }

   
    char *input_chunk = malloc(chunk_size);
    unsigned char *decoded_chunk = malloc((chunk_size / 4) * 3);

    while (1) {
        size_t bytes_read = fread(input_chunk, 1, chunk_size, input_file);
        if (bytes_read == 0) break;

        size_t decoded_size = base64_decode_chunk(input_chunk, bytes_read, decoded_chunk);
        fwrite(decoded_chunk,1,decoded_size,output_file);
        memset(decoded_chunk,0,decoded_size);
    }


    free(input_chunk);
    free(decoded_chunk);
    fclose(input_file);
    fclose(output_file);
}

// int main() {
//     clock_t start = clock();

//     const char *input_path = "/home/ebad/Desktop/server.txt";
//     const char *output_path = "encoded_file.txt";

//     if (base64_encode_file(input_path, output_path) != 0) {
//         printf("Error encoding file.\n");
//         return 1;
//     }

//     // Perform decoding in chunks
//     base64_decode_file(output_path, "decoded.txt");

//     printf("Decoding complete.\n");

//     clock_t end = clock();
//     double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
//     printf("Time taken for decoding: %f seconds\n", time_taken);

//     return 0;
// }
