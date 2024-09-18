/* DECRYPTION Client 
1. Create a socket and connect to the server specified in the command arugments.
2. Prompt the user for input and send that input as a message to the server.
3. Print the message received from the server and exit the program.
*/

#include <stdio.h>      // For printf(), fprintf(), perror()
#include <stdlib.h>     // For exit()
#include <unistd.h>     // For close()
#include <string.h>     // For memset(), strlen()
#include <sys/types.h>  // Defines data types used in system calls
#include <sys/socket.h> // For socket functions like send() and recv()
#include <netinet/in.h> // For sockaddr_in
#include <netdb.h>      // For gethostbyname()
#include <sys/wait.h> // For waitpid()

#define FILE_SIZE 55000
#define CLIENT_ID "DEC_CLIENT"
#define SERVER_ID "DEC_SERVER"
#define HOSTNAME "localhost"

/* Error function used for reporting issues */
void error(const char *msg) { 
  perror(msg); 
  exit(2); 
} 

/* Set up the address struct */
void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){
  // Debug print
  // printf("DECRYPTION Client setupAddressStruct debug: Configuring for Hostname '%s' and Port '%d'\n", hostname, portNumber);

  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);

  // Get the DNS entry for this host name
  struct hostent* hostInfo = gethostbyname(hostname); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "DECRYPTION CLIENT ERROR, no such host\n"); 
    exit(0); 
  }
  // Copy the first IP address from the DNS entry to sin_addr.s_addr
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
  
  // Debug print
//   printf("DECRYPTION Client setupAddressStruct debug: Address setup complete.\n");
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

/* Main */
int main(int argc, char *argv[]) {
  if (argc != 4) { 
      fprintf(stderr, "USAGE: %s ciphertext key port\n", argv[0]); 
      exit(1); 
  }

  int socketFD, portNumber = atoi(argv[3]);
  struct sockaddr_in serverAddress;
  setupAddressStruct(&serverAddress, portNumber, HOSTNAME);

  socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD < 0) error("DECRYPTION CLIENT: ERROR opening socket");

  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) 
      error("DECRYPTION CLIENT: ERROR connecting");
  printf("DECRYPTION Client main debug: Client connected to server successfully.\n");

  // Send CLIENT_ID to Server
  if (sendInChunks(socketFD, CLIENT_ID, strlen(CLIENT_ID)) < strlen(CLIENT_ID))
      error("DECRYPTION CLIENT: ERROR sending client identifier");

  // Receive and verify SERVER_ID
  char serverIDBuffer[12];
  if (receiveInChunks(socketFD, serverIDBuffer, strlen(SERVER_ID)) < strlen(SERVER_ID))
      error("DECRYPTION CLIENT: ERROR receiving server identifier");
  if (strcmp(serverIDBuffer, SERVER_ID) != 0)
      error("DECRYPTION CLIENT: Server verification failed.");

  // Open the ciphertext file
  FILE *ciphertextFile = fopen(argv[1], "r");
  if (ciphertextFile == NULL) {
      fprintf(stderr, "Could not open ciphertext file %s\n", argv[1]);
      exit(1);
  }

  // Read the ciphertext into a buffer
  char ciphertext[FILE_SIZE];
  if (fgets(ciphertext, sizeof(ciphertext), ciphertextFile) == NULL) {
      fprintf(stderr, "Failed to read ciphertext from file %s\n", argv[1]);
      fclose(ciphertextFile);
      exit(1);
  }
  fclose(ciphertextFile);
  // Remove the newline character if present
  ciphertext[strcspn(ciphertext, "\n")] = '\0';

  // Open the key file
  FILE *keyFile = fopen(argv[2], "r");
  if (keyFile == NULL) {
      fprintf(stderr, "Could not open key file %s\n", argv[2]);
      exit(1);
  }

  // Read the key into a buffer
  char key[FILE_SIZE];
  if (fgets(key, sizeof(key), keyFile) == NULL) {
      fprintf(stderr, "Failed to read key from file %s\n", argv[2]);
      fclose(keyFile);
      exit(1);
  }
  fclose(keyFile);
  // Remove the newline character if present
  key[strcspn(key, "\n")] = '\0';

  // Verify that the key is long enough for the ciphertext
  if (strlen(key) < strlen(ciphertext)) {
      fprintf(stderr, "Error: key '%s' is shorter than ciphertext '%s'.\n", argv[2], argv[1]);
      exit(1);
  }

  // Concatenate ciphertext and key, with a delimiter if necessary
  char message[FILE_SIZE]; 
  int messageLength = sprintf(message, "%s\n%s", ciphertext, key);

  // Send message length first
  if (send(socketFD, &messageLength, sizeof(messageLength), 0) < 0)
      error("DECRYPTION CLIENT: ERROR sending message length");
  
  // Send message in chunks
  if (sendInChunks(socketFD, message, messageLength) < messageLength)
      error("DECRYPTION CLIENT: ERROR sending message in chunks");

  // Wait for DECRYPTION message
  char decryptedMessage[FILE_SIZE];
  if (receiveInChunks(socketFD, decryptedMessage, messageLength) < messageLength)
      error("DECRYPTION CLIENT: ERROR receiving decrypted message");
  printf("DECRYPTION message: %s\n", decryptedMessage);

  // Send ACK
  char ackMsg[] = "ACK";
  if (send(socketFD, ackMsg, strlen(ackMsg), 0) < strlen(ackMsg))
      error("DECRYPTION CLIENT: ERROR sending ACK");

  close(socketFD);
  printf("DECRYPTION Client main debug: Connection closed.\n");
  return 0;
}