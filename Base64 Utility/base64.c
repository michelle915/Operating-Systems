// Previously attempted course in Fall 2023.

#include <stdio.h>      // Standard input and output 
#include <errno.h>      // Access to errno and Exxx macros 
#include <stdint.h>     // Extra fixed-width data types 
#include <string.h>     // String utilities 
#include <err.h>        // Convenience functions for error reporting (non-standard)
#include <stdbool.h>    // Boolean type and values

static char const b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
                                   "abcdefghijklmnopqrstuvwxyz" 
                                   "0123456789" 
                                   "+/";  

// int argc - represents the number of items entered on the command line. 
//            EX: given './program input.txt' argc would be: 2
// char *argv[] - array of pointers to arguments passed to the program. Each element of the array points to a null-terminated 
//                string that represents one argument.
//                EX: given './program input.txt' argv would be:
//                      argv[0] would be "./program"
//                      argv[1] would be "input.txt"
int main(int argc, char *argv[]) {
    FILE *input;

    if (argc > 2) { 
        // Invalid: too many arguments
        errno = EINVAL;                                     /* "Invalid Argument" */ 
        err(1, "Too many arguments"); 
    } else if (argc == 2 && strcmp(argv[1], "-")) { 
        // 2 arguments (path, file): open file
        input = fopen(argv[1], "r");                        /* "r" = Open text file for reading */ 
        if (!input) {
            err(1, "Failed to open file: %s", argv[1]);
        }
    } else { 
        // 1 argument: use standard input (default: keyboard)
        input = stdin;
    } 
    
    size_t char_count = 0;                                  /* For wrapping encoded lines every 76 characters */
    size_t total_bytes_read = 0;
    bool need_newline = false;

    for (;;) {
        uint8_t input_bytes[3] = {0};                       /* 3 bytes or 24 bits is least common multiple of 8-bit ASCII input character and 6-bit Base64 output character */ 
        size_t n_read = fread(input_bytes, 1, 3, input);    /* # of bytes read = n_read = fread(destination for read data, read byte-by-byte, read 3 bytes, data to read) */ 
        total_bytes_read += n_read;

        if (n_read != 0) {
            // Convert 3 bytes (24 bits) of ASCII input data into 4 characters of Base64 output
            int alph_ind[4];                                
            alph_ind[0] = input_bytes[0] >> 2;                                  /* Right shift two bits/discard last two bits. Ex: ABCDEFGH -> 00ABCEDF */ 
            alph_ind[1] = (input_bytes[0] << 4 | input_bytes[1] >> 4) & 0x3Fu;  /* Last two bits of first byte + first 4 bits of second byte */
            alph_ind[2] = (input_bytes[1] << 2 | input_bytes[2] >> 6) & 0x3Fu;  /* Last four bits of second byte + first 2 bits of third byte */
            alph_ind[3] = input_bytes[2] & 0x3Fu;                               /* Last six bits of third byte */

            char output[5];                                                     /* 4 Base64 characters + null terminator */
            int output_length = 0;
            for (size_t i = 0; i < 4; i++) {
                output[i] = (i <= n_read) ? b64_alphabet[alph_ind[i]] : '=';
                output_length++;
            }
            output[4] = '\0';                                                   /* Null-terminate the string */ 

            // Write to standard output
            size_t n_write = fwrite(output, 1, output_length, stdout);          /* # of bytes written = n_write = fwrite(array of Base64-encoded char, write 1 char/byte, write all all output, display in terminal) */ 
            char_count += n_write;

            if (char_count >= 76) {                                             /* Wrap output to 76 characters */
                putchar('\n');
                char_count = 0;
                need_newline = false;                                           // Set to false when we manually print a newline.
            } else if (char_count > 0) {
                need_newline = true;                                            // If any characters were printed since the last newline, set to true.
            }

            if (ferror(stdout)) {
                err(1, "Write error");                                          /* Write error */
            }
        }

        // If less than three bytes were read:
        if (n_read < 3) {
            if (feof(input)) {                                  /* End of file */
                if (need_newline && total_bytes_read > 0) {
                    putchar('\n');
                }
                break;                                  
            }
            if (ferror(input)) {                
                err(1, "Read error");                           /* Read error */
            }
        }
    }

    // If input was file, close open file
    if (input != stdin) {
        fclose(input);
    }

    return 0;
} 