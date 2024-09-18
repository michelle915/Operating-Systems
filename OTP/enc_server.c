/* Encryption Server */

#include <stdio.h>              // Input/output operations
#include <stdlib.h>             // General utilities like exit()
#include <string.h>             // String operations like memset()
#include <unistd.h>             // POSIX operating system API
#include <sys/types.h>          // Definitions of data types used in system calls
#include <sys/socket.h>         // Socket programming
#include <netinet/in.h>         // Internet domain address structures
#include <string.h>             // String library

#define FILE_SIZE 55000
#define CLIENT_ID "ENC_CLIENT"
#define SERVER_ID "ENC_SERVER"

/* Print an error message to stderr and exit */
void error(const char *msg) {
  perror(msg);
  exit(1);
} 

/* Initialize and set up a socket address structure for the server */
void setupAddressStruct(struct sockaddr_in* address, int portNumber){
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 
  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);
  // Allow a client at any address to connect to this server
  address->sin_addr.s_addr = INADDR_ANY;

  printf("Encryption Server setupAddressStruct debug: Address struct setup complete for Port '%d'\n", portNumber);
}

/* Modulo 27 encryption */
void encrypt(const char* plaintext, const char* key, char* ciphertext, int textLength) {
    for (int i = 0; i < textLength; i++) {
        // Convert plaintext character p to number
        int p = (plaintext[i] == ' ') ? 26 : plaintext[i] - 'A';
        // Convert key character k to number
        int k = (key[i] == ' ') ? 26 : key[i] - 'A';
        // Combine plaintext and key using modular addition
        int c = (p + k) % 27;
        // Add encrypted character c to ciphertext
        ciphertext[i] = (c == 26) ? ' ' : 'A' + c;
    }
    ciphertext[textLength] = '\0';
}

void cleanUpZombieProcesses() {
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Cleaned up zombie process PID: %d\n", pid);
    }
}

// Function to send data in chunks
int sendInChunks(int sockfd, const char *data, int totalBytes) {
  int bytesSent = 0;                    // Total bytes sent
  int bytesLeft = totalBytes;           // Bytes left to send
  int n;

  while (bytesSent < totalBytes) {
      n = send(sockfd, data + bytesSent, bytesLeft, 0);
      if (n == -1) { break; }           // Handle errors or disconnection
      bytesSent += n;
      bytesLeft -= n;
  }

  return n == -1 ? -1 : bytesSent;      // Return -1 on failure, bytesSent on success
}

// Function to receive data in chunks
int receiveInChunks(int sockfd, char *buffer, int totalBytes) {
  int bytesReceived = 0;                // Total bytes received
  int n;

  while (bytesReceived < totalBytes) {
      n = recv(sockfd, buffer + bytesReceived, totalBytes - bytesReceived, 0);
      if (n == -1 || n == 0) { break; } // Handle errors or disconnection
      bytesReceived += n;
  }

  buffer[bytesReceived] = '\0';
  return n <= 0 ? -1 : bytesReceived;   // Return -1 on failure or disconnection, bytesReceived on success
}

/* Handle a single connection
1. Verify the client
2. Receive plaintext and key
3. Encrypt the message
4. Send encrypted text back 
*/
void handleConnection(int connectionSocket) {
    printf("Encryption Server handleConnection debug: Starting to handle connection...\n");

    // Step 1: Receive and Verify CLIENT_ID
    char clientIDBuffer[13]; // "ENC_CLIENT" + '\0'
    memset(clientIDBuffer, '\0', sizeof(clientIDBuffer));
    if (receiveInChunks(connectionSocket, clientIDBuffer, strlen(CLIENT_ID)) < 0) {
        printf("Encryption Server ERROR: Failed to receive CLIENT_ID.\n");
        close(connectionSocket);
        return;
    }
    clientIDBuffer[12] = '\0'; // Ensure null-termination

    if (strcmp(clientIDBuffer, CLIENT_ID) != 0) {
        printf("Encryption Server ERROR: Client verification failed.\n");
        close(connectionSocket);
        return;
    } else {
        printf("Encryption Server: Client verified successfully.\n");
    }

    // Step 2: Send SERVER_ID back to Client for verification
    if (send(connectionSocket, SERVER_ID, strlen(SERVER_ID), 0) == -1) {
        printf("Encryption Server ERROR: Failed to send server identifier.\n");
        close(connectionSocket);
        return;
    }

    // Step 3: Receive the actual message (plaintext and key) from the client
    char plaintext[FILE_SIZE];
    char key[FILE_SIZE];
    int plaintextLength, keyLength;

    // Assuming the client sends the length of the plaintext first
    if (recv(connectionSocket, &plaintextLength, sizeof(plaintextLength), 0) <= 0) {
        printf("Encryption Server ERROR: Failed to receive plaintext length.\n");
        close(connectionSocket);
        return;
    }

    // Receive the plaintext based on its length
    if (receiveInChunks(connectionSocket, plaintext, plaintextLength) < 0) {
        printf("Encryption Server ERROR: Failed to receive plaintext.\n");
        close(connectionSocket);
        return;
    }

    // Assuming the client sends the length of the key next
    if (recv(connectionSocket, &keyLength, sizeof(keyLength), 0) <= 0) {
        printf("Encryption Server ERROR: Failed to receive key length.\n");
        close(connectionSocket);
        return;
    }
  
    // Receive the key based on its length
    if (receiveInChunks(connectionSocket, key, keyLength) < 0) {
        printf("Encryption Server ERROR: Failed to receive key.\n");
        close(connectionSocket);
        return;
    }

    // Encrypt the message
    char ciphertext[FILE_SIZE]; 
    encrypt(plaintext, key, ciphertext, plaintextLength);

    // Step 4: Send back the encrypted message
    if (sendInChunks(connectionSocket, ciphertext, strlen(ciphertext)) < strlen(ciphertext)) {
        printf("Encryption Server ERROR: Failed to send encrypted message.\n");
        close(connectionSocket);
        return;
    }

    // Step 5: Wait for client's acknowledgment
    char ackMsg[4]; // "ACK" + null terminator
    if (receiveInChunks(connectionSocket, ackMsg, 3) < 0) {
        error("Encryption Server ERROR: Failed to read acknowledgment from client.");
    } else {
        ackMsg[3] = '\0'; 
        if (strcmp(ackMsg, "ACK") != 0) {
            printf("Encryption Server ERROR: Unexpected message received instead of ACK.\n");
        } else {
            printf("Encryption Server: ACK received from client.\n");
        }
    }

    printf("Encryption Server handleConnection debug: Closing connection socket.\n");
    close(connectionSocket);
}

/* Main
1. Setup listening socket
2. Bind, listen, and handle connections
3. Fork a new process for each connection
4. Ensure up to five concurrent connections
*/
int main(int argc, char *argv[]){
  int listenSocket, connectionSocket;                                   
  struct sockaddr_in serverAddress, clientAddress;     
  socklen_t sizeOfClientInfo = sizeof(clientAddress);   

  // Check for correct number of arguments
  if (argc < 2) { 
    fprintf(stderr,"ENCRYPTION SERVER USAGE: %s port\n", argv[0]); 
    exit(1);
  } 
  
  // Create the socket that will listen for connections
  listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket < 0) error("ENCRYPTION SERVER ERROR opening socket");
  else printf("Encryption Server main debug: Listening socket created successfully. Socket FD: %d\n", listenSocket);

  // Set up the address struct for the server socket
  setupAddressStruct(&serverAddress, atoi(argv[1]));

  // Associate the socket to the port
  if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
    error("ENCRYPTION SERVER ERROR on binding");
  } else {
    printf("Encryption Server main debug: Successfully bound to port %d\n", atoi(argv[1]));
  }

  printf("Encryption Server main debug: Server is now listening on port %d\n", atoi(argv[1]));
  // Start listening for connections. Allow up to 5 connections to queue up
  if (listen(listenSocket, 5) < 0) error("ENCRYPTION SERVER ERROR on listen");
  
  // Accept a connection, blocking if one is not available until one connects
  while(1){
    // Clean up any zombie child processes
    cleanUpZombieProcesses();

    printf("Encryption Server main debug: Server awaiting connection...\n");

    // Accept the connection request which creates a connection socket
    connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); 
    if (connectionSocket < 0) error("ENCRYPTION SERVER ERROR on accept");
    else printf("Encryption Server main debug: Accepted connection from client. Connection Socket FD: %d\n", connectionSocket);

    pid_t pid = fork();
    if (pid < 0) error("ENCRYPTION SERVER ERROR on fork");

    // Child process
    if (pid == 0) {
      printf("Encryption Server child process debug: Child process (PID: %d) handling connection.\n", getpid());
      close(listenSocket);
      handleConnection(connectionSocket);
      close(connectionSocket);
      exit(0);

    // Parent process    
    } else { 
      close(connectionSocket);
    }
  }
  close(listenSocket);
  return 0;
}