#include<stdio.h>

int write_file(const char *filename, const unsigned char *data, size_t data_size);


char *base64_encode_chunk(const unsigned char *data, size_t input_length, size_t *output_length);


int base64_encode_file(const char *input_file, const char *output_file,size_t* compressed_size);


size_t base64_decode_chunk(const char *input, size_t input_length, unsigned char *output);


void base64_decode_file(const char *input_filename, const char *output_filename);