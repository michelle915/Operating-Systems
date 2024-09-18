/* This program will generate a key file of specified length, using a set of 27 
 * allowed characters (26 uppercase letters and the space character).
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_CHARS 27

int main(int argc, char *argv[]) {
    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Error: Incorrect number of keygen arguments.\n");
        return 1;
    }

    // Convert argument to integer
    int keyLength = atoi(argv[1]);

    // Seed the random number generator
    srand(time(NULL));

    // Generate and print the key
    char valid_chars[NUM_CHARS] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    for (int i = 0; i < keyLength; i++) {
        char randomChar = valid_chars[rand() % NUM_CHARS];
        printf("%c", randomChar);
    }
    printf("\n"); // End with a newline

    return 0;
}