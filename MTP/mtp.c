// Previously attempted course in Fall 2023.

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define MAX_LINE_LENGTH 1000
#define MAX_LINES 50
#define BUFFER_SIZE MAX_LINES
#define POISON_PILL "POISON_PILL"

// Buffers
char buffer_1[MAX_LINES][MAX_LINE_LENGTH];
char buffer_2[MAX_LINES][MAX_LINE_LENGTH];
char buffer_3[MAX_LINES][MAX_LINE_LENGTH];

// Number of items in each buffer   
int count_1 = 0;  
int count_2 = 0; 
int count_3 = 0;       

// Next producer position index in each buffer 
int prod_idx_1 = 0;
int prod_idx_2 = 0;
int prod_idx_3 = 0;

// Next consumer position index in each buffer                 
int con_idx_1 = 0; 
int con_idx_2 = 0; 
int con_idx_3 = 0;                   

// Mutexes and Condition Variables
pthread_mutex_t mutex_buffer_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_buffer_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_buffer_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_buffer_1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_buffer_2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_buffer_3 = PTHREAD_COND_INITIALIZER;

/* FORWARD DECLARATIONS: inform the compiler about the function signature before use 
 */
void *get_input(void *args);
void *replace_line_separator(void *args);
void *replace_plus_sign(void *args);
void *write_output(void *args);
void put_in_buffer(char buffer[MAX_LINES][MAX_LINE_LENGTH], int *prod_idx, int *count, pthread_mutex_t *mutex, pthread_cond_t *cond, const char *item);
char *get_from_buffer(char buffer[MAX_LINES][MAX_LINE_LENGTH], int *prod_idx, int *con_idx, int *count, pthread_mutex_t *mutex, pthread_cond_t *cond);

int main() {
    srand(time(0));
    pthread_t input_thread, line_separator_thread, plus_sign_thread, output_thread;

    // Create threads
    pthread_create(&input_thread, NULL, get_input, NULL);
    pthread_create(&line_separator_thread, NULL, replace_line_separator, NULL);
    pthread_create(&plus_sign_thread, NULL, replace_plus_sign, NULL);
    pthread_create(&output_thread, NULL, write_output, NULL);

    // Wait for threads to finish
    pthread_join(input_thread, NULL);
    pthread_join(line_separator_thread, NULL);
    pthread_join(plus_sign_thread, NULL);
    pthread_join(output_thread, NULL);

    return EXIT_SUCCESS;
}

/* Input Thread Function: Reads in lines of characters from the standard input
*/
void *get_input(void *args) {
    char line[MAX_LINE_LENGTH];

    while (fgets(line, MAX_LINE_LENGTH, stdin)) {
        // Read a line from standard input
        if (strncmp(line, "STOP\n", 5) == 0) {
            put_in_buffer(buffer_1, &prod_idx_1, &count_1, &mutex_buffer_1, &cond_buffer_1, POISON_PILL);
            break;
        } else {
            put_in_buffer(buffer_1, &prod_idx_1, &count_1, &mutex_buffer_1, &cond_buffer_1, line);
        }
    }

    return NULL;
}

/* Line Separator Thread Function: Replaces every line separator in the input by a space
*/
void *replace_line_separator(void *args) {
    while (1) {
        char *line = get_from_buffer(buffer_1, &prod_idx_1, &con_idx_1, &count_1, &mutex_buffer_1, &cond_buffer_1);

        // Check for the stop-processing condition, i.e., poison pill
        if (strcmp(line, POISON_PILL) == 0) {
            // Put the poison pill in buffer_2 and break
            put_in_buffer(buffer_2, &prod_idx_2, &count_2, &mutex_buffer_2, &cond_buffer_2, POISON_PILL);
            break;
        }

        // Iterate through the entire string and replace newline characters with spaces
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == '\n') {
                line[i] = ' ';
            }
        }

        // Put the modified line into buffer_2
        put_in_buffer(buffer_2, &prod_idx_2, &count_2, &mutex_buffer_2, &cond_buffer_2, line);
    }

    return NULL;
}

/* Plus Sign Thread Function:Replaces every pair of plus signs, i.e., "++", by a "^"
*/
void *replace_plus_sign(void *args) {
    while (1) {
        char* line = get_from_buffer(buffer_2, &prod_idx_2, &con_idx_2, &count_2, &mutex_buffer_2, &cond_buffer_2);

        // Check for the stop-processing condition/poison pill
        if (strcmp(line, POISON_PILL) == 0) {
            put_in_buffer(buffer_3, &prod_idx_3, &count_3, &mutex_buffer_3, &cond_buffer_3, POISON_PILL);
            break;
        }

        // Allocate memory for the modified string. It might be shorter, but never longer.
        char* new_line = malloc(strlen(line) + 1);
        if (new_line == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1); 
        }

        // Iterate through the string and replace "++" with "^"
        int i = 0, j = 0;
        while (line[i] != '\0') {
            if (line[i] == '+' && line[i + 1] == '+') {
                new_line[j++] = '^';
                i += 2; // Skip over the second "+"
            } else {
                new_line[j++] = line[i++];
            }
        }
        new_line[j] = '\0';

        // Put the modified line into buffer_3
        put_in_buffer(buffer_3, &prod_idx_3, &count_3, &mutex_buffer_3, &cond_buffer_3, new_line);

        // Free the memory allocated for the new_line
        free(new_line);
    }

    return NULL;
}

/* Output Thread Function: Writes the processed data to standard output as lines of exactly 80 characters
*/
void *write_output(void *args) {
    char output_line[81] = {0}; // Buffer for exactly 80 characters + '\n'
    int output_index = 0; // Index for adding characters to output_line

    while (1) {
        char *line = get_from_buffer(buffer_3, &prod_idx_3, &con_idx_3, &count_3, &mutex_buffer_3, &cond_buffer_3);

        // Check for the stop-processing condition, i.e., poison pill
        if (strcmp(line, POISON_PILL) == 0) {
            break; // Exit loop
        }

        int line_length = strlen(line);
        for (int i = 0; i < line_length; ++i) {
            output_line[output_index++] = line[i];
            
            // Once we have accumulated 80 characters, print them and reset the index
            if (output_index == 80) {
                output_line[80] = '\n'; 
                write(STDOUT_FILENO, output_line, 81); // Write the 80 characters + '\n' to stdout
                output_index = 0; 
            }
        }
    }

    return NULL;
}

/* Put an item in a buffer
*/
void put_in_buffer(char buffer[MAX_LINES][MAX_LINE_LENGTH], int *prod_idx, int *count, pthread_mutex_t *mutex, pthread_cond_t *cond, const char *item) {
    // 1. Lock mutex to ensure exclusive access
    pthread_mutex_lock(mutex);
    
    // Wait until there is space in the buffer?
    while (*count == MAX_LINES) {
        pthread_cond_wait(cond, mutex);
    }
    
    // 2. Copy the item into the buffer
    strncpy(buffer[*prod_idx], item, MAX_LINE_LENGTH - 1);

    // 3. Ensure null termination
    buffer[*prod_idx][MAX_LINE_LENGTH - 1] = '\0'; 

    // 4. Update producer index in a circular manner
    *prod_idx = (*prod_idx + 1) % MAX_LINES; 

    // 5. Increment the count of items in the buffer
    (*count)++; 

    // 6. Signal that a new item has been added
    pthread_cond_signal(cond); 

    // 7. Unlock the mutex to allow other threads to access the buffer
    pthread_mutex_unlock(mutex);
}

/* Get the next item from a buffer
*/
char *get_from_buffer(char buffer[MAX_LINES][MAX_LINE_LENGTH], int *prod_idx, int *con_idx, int *count, pthread_mutex_t *mutex, pthread_cond_t *cond) {
    // 1. Lock buffer
    pthread_mutex_lock(mutex);

    // 2. If buffer is empty, wait for the producer to signal that the buffer has data
    while (*count == 0) {
        pthread_cond_wait(cond, mutex);
    }

    // 3. Retrieve the item
    char *item = buffer[*con_idx];

    // 4. Update consumer index in a circular manner
    *con_idx = (*con_idx + 1) % MAX_LINES;

    // 5. Decrease the count of items in the buffer
    (*count)--;

    // 6. Signal that space might be available in the buffer
    pthread_cond_signal(cond);

    // 7. Unlock the mutex
    pthread_mutex_unlock(mutex);

    // 8. Return the item
    return item;
}
