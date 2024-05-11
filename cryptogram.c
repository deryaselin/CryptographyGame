/*Derya Ertik
cryptogram.c
This program will combine the
cryptogram logic and web server functionality
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

//PROTOTYPES
void *clientSide(void *arg);
char* handleGame(const char *url);

char encryption_key[26]; 
char player_key[2024];
int entries = 0; 

//A macro to be used to fill the linked list structure
#define SIZE 2048 
#define FILL_FIELDS()\
	struct Quote *newQuote = quoteAllocator();\
        newQuote->phrase = strdup(currentPhrase ? currentPhrase: "");\
        newQuote->author = strdup(currentAuthor ? currentAuthor : "");\
        newQuote->next = NULL;\
	if (head == NULL){ \
		head = last = newQuote;\
	} else { \
		last->next = newQuote;\
		last = newQuote; \
	}\
	free(currentPhrase);\
	free(currentAuthor);\
	currentPhrase = currentAuthor = NULL;		

//Quote structure
struct Quote { 
    char *phrase;
    char *author;
    struct Quote *next;
};

struct Quote *head = NULL; 

struct Quote *quoteAllocator() { 
    struct Quote *newQuote = (struct Quote *)malloc(sizeof(struct Quote));
    if (newQuote != NULL) {
        newQuote->phrase = NULL;
        newQuote->author = NULL;
        newQuote->next = NULL;
    }
    return newQuote;
}

//Funtion to free the memory
void teardownAll() { 
    while (head != NULL) {
        struct Quote *temp = head;
        head = head->next;
        free(temp->phrase);
        free(temp->author);
        free(temp);
    }
}

//Function to load the linked list structure
void loadPuzzles() {
    FILE *fp = fopen("quotes.txt", "r"); 
    if (fp == NULL) { 
        printf("Error on opening the file\n");
    }

    struct Quote *last = NULL; 
    char line[256]; 
    char *currentPhrase = NULL; 
    char *currentAuthor = NULL;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strlen(line) < 3) { 
            if (currentPhrase || currentAuthor) { 
                FILL_FIELDS(); 
                entries++; 
            }
        } else if (line[0] == '-' && line[1] == '-') { 
            if (currentAuthor) { 
                FILL_FIELDS();
            }

            if (currentAuthor) {
                free(currentAuthor);
            }

            currentAuthor = strdup(line + 2);
        } else { 
            char *temp = strdup(line); 
            if (!currentPhrase) { 
                currentPhrase = temp; 
            } else { 
                currentPhrase = realloc(currentPhrase, strlen(currentPhrase) + strlen(temp) + 1); 
                strcat(currentPhrase, temp); 
                free(temp); 
            }
            if (line[strlen(line) - 1] == '-') {  
                FILL_FIELDS(); 
            }
        }
    }

    if (currentPhrase || currentAuthor) { 
	FILL_FIELDS();
    }

    fclose(fp);
}

//Fisher-Yates Algorithm
void shuffle(char key[26], size_t key_size){ 
	int i = key_size - 1; 
	for (i; i > 0; i--){
		int j = rand() % (i + 1); 
		char temp = key[i]; 
		key[i] = key[j]; 
		key[j] = temp;
	}
}

//A function to return a random puzzle from the list
char *getPuzzle(){
	char *random_puzzle;
	if (entries == 0){
		loadPuzzles();
	}
	int index = rand() % entries; 
	struct Quote *current = head;
	for (int i = 0; i < index; i++) {
        	current = current -> next; 
    	}
    	random_puzzle = strdup(current->phrase);
    	return random_puzzle; 
}

//Starts the game
char *initialization(){
	for(size_t i = 0; i < sizeof(encryption_key); i++){ 
		encryption_key[i] = 'A' + i;
	}

	shuffle(encryption_key, 26); 

	for(size_t i = 0; i < sizeof(player_key); i++){ 
		player_key[i] = '\0';
	}
	char *new_puzzle = getPuzzle(); 
	size_t len = strlen(new_puzzle);
	char *dynamic_puzzle = (char *)malloc(len + 1); 
	strcpy(dynamic_puzzle, new_puzzle);
	free(new_puzzle);

	for(size_t i = 0; i < len; i++){ 
		if (isalpha(dynamic_puzzle[i])){ 
			dynamic_puzzle[i] = toupper(dynamic_puzzle[i]); 
			size_t distance = dynamic_puzzle[i] - 'A'; 
			dynamic_puzzle[i] = encryption_key[distance];
			}
		else{
			dynamic_puzzle[i] = dynamic_puzzle[i]; 
		}
	}
	dynamic_puzzle[len] = '\0';
	
	return dynamic_puzzle;
}

//Multithreading Structue
struct MultiThread {
	int clientSocket;
   char *filePath;
};


int main(int argc, char *argv[]) {
	loadPuzzles(); //loads the linked list
	srand(time(NULL)); //sets the seed for random numbers
   
	if (argc != 2) { //Returns a usage message if the path is not provided
		fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
      exit(EXIT_FAILURE);
   }

	char *filePath = argv[1];
   int clientSocket;
   int serverSocket;
	
   struct sockaddr_in server_addr;
   socklen_t client_length;
   pthread_t tid;

	//Sets the hints
   struct addrinfo hints, *server_info;
   memset(&hints, 0, sizeof hints); 
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE;

   int address = getaddrinfo(NULL, "8000", &hints, &server_info); 

   serverSocket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol); 
   bind(serverSocket, server_info->ai_addr, server_info->ai_addrlen); 
   freeaddrinfo(server_info); 

	if (listen(serverSocket, 10) < 0) { 
		perror("Error listening on socket");
   	exit(EXIT_FAILURE);
   }

   printf("Server is currently listening on port %d\n", 8000);

	while (1) { 
		client_length = sizeof(struct sockaddr_in);
      clientSocket = accept(serverSocket, (struct sockaddr *)&server_addr, &client_length);
       
      if(clientSocket>1){
			struct MultiThread *threads = malloc(sizeof(struct MultiThread));
     		threads->clientSocket = clientSocket;
      	threads->filePath = filePath;

        	pthread_create(&tid, NULL, clientSide, (void *)threads);
        	pthread_detach(tid);
		} 
	}
	close(serverSocket);
	teardownAll();
   return 0;
}

//A function to handle the connections between the client and the server
void *clientSide(void *arg) { 
	struct MultiThread *threads = (struct MultiThread *)arg;
   int clientSocket = threads->clientSocket;
   char *filePath = threads->filePath;

   char buffer[SIZE];
   int bufferRead;

   bufferRead = read(clientSocket, buffer, SIZE); 
   if (bufferRead > 0) {
		buffer[bufferRead] = '\0'; 
      printf("\nTHE REQUEST SENT BY THE CLIENT:\n%s\n", buffer);

      char *ptr;
      char *readLine = strtok_r(buffer, "\n", &ptr);
      char *clientRequest = strtok_r(readLine, " ", &ptr);
      char *fileName = strtok_r(NULL, " ", &ptr);

      if (strncmp(clientRequest, "GET", 3) == 0) { 
			memmove(fileName, fileName + 1, strlen(fileName)); 

         char directoryPath[SIZE];
         snprintf(directoryPath, sizeof(directoryPath), "%s/%s", filePath, fileName); 

			if(strncmp(fileName, "crypt", 5) == 0){ //The case url starts with "crypt"
				char *htmlContent = handleGame(fileName);
				long content_length = strlen(htmlContent);
    			char header_buffer[SIZE];
    			snprintf(header_buffer, SIZE, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", content_length);
   			write(clientSocket, header_buffer, strlen(header_buffer));
   			write(clientSocket, htmlContent, content_length);
			}else{ //The case url does not start with "crypt"
					struct stat file_info; 
        			if (stat(directoryPath, &file_info) == 0) { //The case file exists in the directory
						int fd = open(directoryPath, O_RDONLY); 
           			if (fd >= 0) { 
							char header[SIZE]; 
               		sprintf(header, "HTTP/1.1 200 OK\nContent-Length: %ld\n\n", file_info.st_size);
               		printf("HTTP/1.1 200 OK\nContent-Length: %ld\n\n%s\n\n", file_info.st_size, fileName);
               		write(clientSocket, header, strlen(header));
                    
              			char fileContent[file_info.st_size]; 
              		 	read(fd, fileContent, file_info.st_size);
               		write(clientSocket, fileContent, file_info.st_size);
               		close(fd);
            		} 
					} else { // The case file does not exist in the directory
						const char *notExist= "HTTP/1.1 404 Not Found\n\n404 Not Found";
            		write(clientSocket, notExist, strlen(notExist));
            		printf("%s\n\n%s\n\n", notExist, fileName);
         			}
				}
      }
	}

    close(clientSocket); 
    free(threads); 
    pthread_exit(NULL); 
}

//A function to display the game state
void displayWorld(char *word1, char *word2, char *htmlContent){ 
	for(int i = 0; i < strlen(word1); i++){
		if(isalpha(word1[i])){ 
			if (word2[i] == '\0'){ 
                		word2[i] = '_'; 
			}
			else{
				word2[i] = word2[i]; 
			}
		}
		else {
		    	word2[i] = word1[i]; 
		}
  	}
	sprintf(htmlContent, "<!DOCTYPE html>\n" // An HTML output to display as long as the player did not solve the puzzle
                           "<html>\n"
                           "<head>\n"
                           "<title>Crypto Game</title>\n"
                           "</head>\n"
                           "<body>ENCRYPTED: \r\n%s\n"                  
                           "<p>DECRYPTED: \n%s</p>\n"
									"<p>**THE PUZZLE HAS NOT BEEN DECODED YET! \n</p>\n"
                           "<form method=\"get\" action=\"/crypto\">\n"
                           "  <label for=\"move\">Enter a Letter to Replace:</label><br>\n"
                           "  <input type=\"text\" id=\"move\" name=\"move\" autofocus maxlength=\"2\"><br><br>\n"
                           "  <input type=\"submit\" value=\"Submit\">\n"
                           "</form>\n"
                           "</body>\n"
                           "</html>\n", word1, player_key);
}

//A function to check if the game is over
bool isGameOver(char *word1, char* word2){
	bool decoded = true;
	for(int i = 0; i < strlen(word1); i++){ 
		if(word2[i] == '_'){
			decoded = false;
		}
		for(int j = 0; j < strlen(word2); j++){
			if(word1[i] != word1[j]){
				if(word2[i] == word2[j]){
					decoded = false;
				}
			}
		}	
	}
	return decoded;		
}

//A function to handle the game 
char* handleGame(const char *url){
	char *htmlContent = (char *)malloc(SIZE * sizeof(char));
	static char *newPuzzle = NULL;
	
	if(newPuzzle == NULL){ //Starts the game
		newPuzzle = initialization(); //Initialization calls the getPuzzle() to return a random puzzle
	}

	if (strchr(url,'?') == NULL){ //If the has just started
		displayWorld(newPuzzle, player_key, htmlContent);
	}

	else if (strchr(url, '?') != NULL){ //If the game is in process
		char move[3];
		char *index = strchr(url, '='); //the index of the equal sign in ?move=
		index ++; //moves the equal sign pointer to parse the letters
		strncpy(move, index, 2);
		move[2] = '\0';
		char initial_letter; //replaces the letters in the puzzle
		char replacement_letter;
		initial_letter = move[0];
		replacement_letter = move[1];
			for(int i = 0; i < strlen(newPuzzle); i++){ 
				if (newPuzzle[i] == initial_letter){
					player_key[i] = replacement_letter;		
				}
			}
		displayWorld(newPuzzle, player_key, htmlContent);
		if(!isGameOver(newPuzzle, player_key)){ //The case game is not over yet
		displayWorld(newPuzzle, player_key, htmlContent);
	
		}
		else if(isGameOver(newPuzzle, player_key)){ //The case player solved the puzzle
			sprintf(htmlContent, "<html><body>Congratulations! You solved it! <p><a href=\"crypto\">Another?</a></p></body></html>"); //Prints link for a new game
			newPuzzle = initialization(); //If the user clicks on the link, a new random puzzle is returned
		}
	
	}
	return htmlContent;
	free(htmlContent);
}





