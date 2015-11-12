
const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <pthread.h>
#include <stdbool.h>
#include <limits.h>

#include <signal.h>
#include <sys/wait.h>
 
#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>
#include <assert.h>

#include <vector>
#include <iostream>
#include <algorithm>

#include <sstream>
#include <dlfcn.h>

#include <arpa/inet.h>

#define DEFAULTPORT 3333

struct directoryFile {
	char *name; // name of the file
	char *directory; // the directory of the file
	int type; // 0 - file, 1-directory, 2 - parent directory
	char *path;
	size_t size;
	time_t modifiedTime;
};


int numRequests = 0;
char *requests[1024];
int numMods = 0;
char *mods[1024];

char* maxRequest;
char* minRequest;
double maxTime = 0;
double minTime = 0;

struct timespec rstart={0,0}, rend={0,0};

struct in_addr clientIP;


struct timespec tstart={0,0}, tend={0,0};




bool nameAscending (const directoryFile i, const directoryFile j) {    
    return (strcmp(i.name, j.name) > 0);
}

bool nameDescending (const directoryFile i, const directoryFile j) {    
    return (strcmp(i.name, j.name) < 0);
}

bool sizeAscending (const directoryFile i, const directoryFile j) {    
    return (i.size > j.size);
}

bool sizeDescending (const directoryFile i, const directoryFile j) {    
    return (i.size < j.size);
}

bool modifiedTimeAscending (const directoryFile i, const directoryFile j) {    
    return (i.modifiedTime > j.modifiedTime);
}

bool modifiedTimeDescending (const directoryFile i, const directoryFile j) {    
    return (i.modifiedTime < j.modifiedTime);
}



int QueueLength = 5;

const char *protocol = "HTTP/1.1 200 Document follows \r\nServer: CS 252 lab5\r\nContent-type: ";

const char *crlf = "\r\n";

const char *notFound = "HTTP/1.1 404 File not found\r\n";
const char *errorMessage = "The file you have requested could not be found.\r\n";

void killzombie(int signum);


pthread_mutex_t mutex;

// Processes time request
void processRequest(int socket);
void processRequestThread(int socket);
void poolSlave(int masterSocket);

void rDirectories(char* actualPath);
int
main( int argc, char ** argv )
{
  //timer

  //serverStartTime = time(NULL);
  clock_gettime(CLOCK_MONOTONIC, &tstart);

  struct sigaction signalAction;
  signalAction.sa_handler = killzombie;
  sigemptyset(&signalAction.sa_mask);
  signalAction.sa_flags = SA_RESTART;

  int sigact = sigaction(SIGCHLD, &signalAction, NULL);
  if(sigact) {
  	perror("sigaction");
	exit(-1);
  }



  char mode[3];
  int port;

  if ( argc < 2 ) {

	port = DEFAULTPORT;
  }
  

  if (argc == 3){
		strcpy(mode, argv[1]);
		port = atoi( argv[2] );
  }
  else if (argc == 2)
		if(argv[1][0] == '-'){		
			strcpy(mode, argv[1]);
			port = DEFAULTPORT;	
		}
		else{		
			port = atoi( argv[1] );
			strcpy(mode, "-n");
		}
  //eg: f/t/p

  fprintf(stderr, "Mode: %s\nPort:%d\n", mode, port);


  //fprintf(stderr, "Mode: %s\nPort:%d\n", mode, port);

  // Get the port from the arguments

  
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

	if(strcmp(mode, "-p") == 0){
		pthread_t tid[5];
			pthread_attr_t attr;
			pthread_attr_init( &attr );
			//pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_mutex_init(&mutex, 0);
		for(int i = 0; i < 5; i++){
			pthread_create(&(tid[i]), &attr, (void*(*)(void*))poolSlave, (void*)(long)masterSocket); 
		}
		for(int i = 0; i < 5; i++)
			pthread_join(tid[i],0);

	}
	else if(strcmp(mode, "-f") == 0){
		while(1) {
			struct sockaddr_in clientIPAddress;
			clientIP = clientIPAddress.sin_addr;
			int alen = sizeof( clientIPAddress );
			int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);		
			if ( slaveSocket < 0 ) {
			  perror( "accept" );
			  exit( -1 );
			}
			int ret = fork();
			if(ret == 0) {
				processRequest( slaveSocket); 
				close(slaveSocket);
				exit(EXIT_SUCCESS);
			}
			close(slaveSocket);
		}
	}
	else if(strcmp(mode, "-t") == 0){
		while ( 1 ) {

			// Accept incoming connections
			struct sockaddr_in clientIPAddress;
			clientIP = clientIPAddress.sin_addr;
			int alen = sizeof( clientIPAddress );
			int slaveSocket = accept( masterSocket,
						  (struct sockaddr *)&clientIPAddress,
						  (socklen_t*)&alen);
			if ( slaveSocket < 0 ) {
			  perror( "accept" );
			  exit( -1 );
			}
			pthread_t tid;
			pthread_attr_t attr;
			pthread_attr_init( &attr );
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&tid,&attr,(void*(*)(void*))processRequestThread,(void*)(long)slaveSocket);

		
		}
	}
	else{
		while ( 1 ) {

			// Accept incoming connections
			struct sockaddr_in clientIPAddress;
			clientIP = clientIPAddress.sin_addr;
			int alen = sizeof( clientIPAddress );
			int slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);		
			if ( slaveSocket < 0 ) {
			  perror( "accept" );
			  exit( -1 );
			}
			processRequest( slaveSocket); 
			//shutdown(slaveSocket, 2);			
			close(slaveSocket);
		 }
	}
	  
}

void
poolSlave(int masterSocket){
		
	while(1){
		struct sockaddr_in clientIPAddress;
		clientIP = clientIPAddress.sin_addr;
		int alen = sizeof(clientIPAddress);
		pthread_mutex_lock(&mutex);		
		int slaveSocket = accept( masterSocket,(struct sockaddr *)&clientIPAddress,(socklen_t*)&alen);
		pthread_mutex_unlock(&mutex);
		if(slaveSocket >= 0){
			processRequest(slaveSocket);
		}
		close(slaveSocket);
	}
		
}

void
processRequestThread(int socket){
	processRequest(socket);
	close(socket);
} 

void
processRequest(int socket){

	clock_gettime(CLOCK_MONOTONIC, &rstart);

  	unsigned char newChar;
  	unsigned char oldChar;
  	unsigned char oldChar2;
  	unsigned char oldChar3;
	ssize_t n;
	int gotGet = 0;
	int gotDocPath = 0;
	char curr_string[ 1024 + 1 ];
	char docpath[ 1024 + 1 ];
	
	int length = 0;
	
	while((n = read(socket, &newChar, sizeof(newChar))))
	{
		if(gotGet == 0){
			if(newChar == ' '){
				gotGet = 1;
			}
		}
		else{
			if(gotDocPath == 0){
				if(newChar == ' ')
					gotDocPath = 1;
				else if(newChar == '\n' && oldChar == '\r')
					break;
				else if(newChar != '\n' && newChar != '\r'){			
						curr_string[length] = newChar;	
						length++;		
				}
			}
			else{
				if(newChar == '\n' && oldChar == '\r' && oldChar2 == '\n' && oldChar3 == '\r')
					break;
			}
		}
		oldChar3 = oldChar2;
		oldChar2 = oldChar;
		oldChar = newChar;
	}
	
	curr_string[length] = '\0';
	//printf("len: %d\n", length);
	requests[numRequests] = (char *)malloc(strlen(curr_string)+1);
	strcpy(requests[numRequests], curr_string);
	numRequests++;

	if(curr_string[strlen(curr_string) - 1] != '/')
		strcat(curr_string, "/");



	char* cwd = (char *)malloc(256);


	if(strstr(curr_string, "/cgi-bin/") != NULL){

		char* pointer = strstr(curr_string, "/cgi-bin/");
		pointer += 9;
		char* scriptName = (char *)malloc(256);
		int i = 0;
		while(*(pointer) != '?' &&  *(pointer) != '\0' && *(pointer) != '/'){
			scriptName[i] = *(pointer);
			i++;
			pointer++;
		}
		scriptName[i] = '\0';
			


		cwd = getcwd(cwd, 256);

		strcat(cwd, "/http-root-dir/cgi-bin/");
		strcat(cwd, scriptName);


			
		write(socket, "HTTP/1.1 200 Document follows\r\n Server: CS 252 lab5\r\n", strlen("HTTP/1.1 200 Document follows\r\n Server: CS 252 lab5\r\n"));

		char* query_string;
		pointer = strstr(curr_string, "?");
		if(pointer != NULL){
			pointer++;
			query_string = strdup(pointer);
			setenv("REQUEST_METHOD", "GET", 1);
			setenv("QUERY_STRING", query_string, 1);
		}
		else
			query_string = "";

		if(strcmp(scriptName + strlen(scriptName)- 3, ".so") == 0){
			

			mods[numMods] = strdup(scriptName);
			numMods++;

			void* handle = dlopen(cwd, RTLD_LAZY);
				
			typedef void (*func)(int, char *);

			func myhttprun = (func)dlsym(handle, "httprun");

			myhttprun(socket, query_string);

			clock_gettime(CLOCK_MONOTONIC, &rend);
			double requestTime = ((double)rend.tv_sec + 1.0e-9*rend.tv_nsec) - ((double)rstart.tv_sec + 1.0e-9*rstart.tv_nsec);	
			if(maxTime == 0 || minTime == 0){
				maxTime = requestTime;
				minTime = requestTime;
				minRequest = strdup(curr_string);
				maxRequest = strdup(curr_string);			
			}
			if(minTime >  requestTime){
				minTime = requestTime;
				minRequest = strdup(curr_string);
			}
			if(maxTime < requestTime){
				maxTime = requestTime;
				maxRequest = strdup(curr_string);
			}		

			return;
		}

		int tmpout = dup(1);

		//printf("TEST:%s\n\n",query_string);
		int ret;
		ret = fork();
		dup2(socket,1);
								
		if(ret == 0){
			int err = execvp(cwd , NULL);
			if(err < 0)
			perror("execvp");
		}
		dup2(tmpout, 1);	
		//waitpid(ret,0,0);
		//close(socket);
			clock_gettime(CLOCK_MONOTONIC, &rend);
			double requestTime = ((double)rend.tv_sec + 1.0e-9*rend.tv_nsec) - ((double)rstart.tv_sec + 1.0e-9*rstart.tv_nsec);	
			if(maxTime == 0 || minTime == 0){
				maxTime = requestTime;
				minTime = requestTime;
				minRequest = strdup(curr_string);
				maxRequest = strdup(curr_string);			
			}
			if(minTime >  requestTime){
				minTime = requestTime;
				minRequest = strdup(curr_string);
			}
			if(maxTime < requestTime){
				maxTime = requestTime;
				maxRequest = strdup(curr_string);
			}		

			return;
	}

	//implement stats
	if(strstr(curr_string, "stats") != NULL){

		write(socket, protocol, strlen(protocol));	
		write(socket, "text/html\r\n\r\n", strlen("text/html\r\n\r\n"));		
		write(socket, "<html>\n<head>\n<title>Statistics", strlen("<html>\n<head>\n<title>Statistics"));	
		write(socket, "</title>\n</head>\n<body>\n<h1>\nStatistics", strlen("</title>\n</head>\n<body>\n<h1>\nStatistics"));
		write(socket, "\n</h1>\n", strlen("\n</h1>\n"));
		write(socket, "<p>Written by: Aaron Kar Ee Ho</p>\n", strlen("<p>Written by: Aaron Kar Ee Ho</p>\n"));
		write(socket, "<p>Sever Up Time: </p>\n", strlen("<p>Sever Up Time: "));
   		clock_gettime(CLOCK_MONOTONIC, &tend);
   		double serverUpTime = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);	
		char buffer [50];	
		sprintf (buffer, "%f seconds", serverUpTime);
		write(socket, buffer, strlen(buffer));
		write(socket, "</p>\n", strlen("</p>\n"));
		write(socket, "<p>Number of requests: ", strlen("<p>Number of requests: "));
		char buffer2 [50];	
		sprintf (buffer2, "%d", numRequests);
		write(socket, buffer2, strlen(buffer2));
		write(socket, "</p>\n", strlen("</p>\n"));

		write(socket, "<p>The minimum service time: ", strlen("<p>The minimum service time: "));
		write(socket, minRequest, strlen(minRequest)+1);
		write(socket, "     ", strlen("     "));
		char buffer3 [50];	
		sprintf (buffer3, "%f", minTime);
		write(socket, buffer3, strlen(buffer3));
		write(socket, "</p>\n", strlen("</p>\n"));

		write(socket, "<p>The maximum service time: ", strlen("<p>The maximum service time: "));
		write(socket, maxRequest, strlen(maxRequest)+1);
		write(socket, "     ", strlen("     "));
		char buffer4 [50];	
		sprintf (buffer4, "%f", maxTime);
		write(socket, buffer4, strlen(buffer4));
		write(socket, "</p>\n", strlen("</p>\n"));

		write(socket, "</body></html>", strlen("</body></html>"));
			clock_gettime(CLOCK_MONOTONIC, &rend);
			double requestTime = ((double)rend.tv_sec + 1.0e-9*rend.tv_nsec) - ((double)rstart.tv_sec + 1.0e-9*rstart.tv_nsec);	
			if(maxTime == 0 || minTime == 0){
				maxTime = requestTime;
				minTime = requestTime;
				minRequest = strdup(curr_string);
				maxRequest = strdup(curr_string);			
			}
			if(minTime >  requestTime){
				minTime = requestTime;
				minRequest = strdup(curr_string);
			}
			if(maxTime < requestTime){
				maxTime = requestTime;
				maxRequest = strdup(curr_string);
			}		

			return;
	}

	if(strstr(curr_string, "logs") != NULL){

		write(socket, protocol, strlen(protocol));	
		write(socket, "text/html\r\n\r\n", strlen("text/html\r\n\r\n"));		
		write(socket, "<html>\n<head>\n<title>Statistics", strlen("<html>\n<head>\n<title>Statistics"));	
		write(socket, "</title>\n</head>\n<body>\n<h1>\nStatistics", strlen("</title>\n</head>\n<body>\n<h1>\nStatistics"));
		write(socket, "\n</h1>\n", strlen("\n</h1>\n"));

		for(int j = 0; j < numRequests; j++){
			write(socket, "<p>", strlen("<p>"));
			write(socket, inet_ntoa(clientIP), strlen(inet_ntoa(clientIP))+1);
			write(socket, ": ", strlen(": "));
			write(socket, requests[j], strlen(requests[j])+1);
			write(socket, "</p>\n", strlen("</p>\n"));
		}
		write(socket, "</body></html>", strlen("</body></html>"));
			clock_gettime(CLOCK_MONOTONIC, &rend);
			double requestTime = ((double)rend.tv_sec + 1.0e-9*rend.tv_nsec) - ((double)rstart.tv_sec + 1.0e-9*rstart.tv_nsec);	
			if(maxTime == 0 || minTime == 0){
				maxTime = requestTime;
				minTime = requestTime;
				minRequest = strdup(curr_string);
				maxRequest = strdup(curr_string);			
			}
			if(minTime >  requestTime){
				minTime = requestTime;
				minRequest = strdup(curr_string);
			}
			if(maxTime < requestTime){
				maxTime = requestTime;
				maxRequest = strdup(curr_string);
			}		

			return;
	}

	if(curr_string[strlen(curr_string) - 1] != '/')
		strcat(curr_string, "/");

	char* ptr;
	int sortName = 0;
	int sortDate = 0;
	int sortSize = 0;	
	int sortOrder = 0; // 0 - ascending, 1 - descending

	// check if sorting needed
	if((ptr = strstr(curr_string, "?C=N;O=A")) != NULL){
		sortName = 1;
		sortOrder = 0;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);

	}
	else if((ptr = strstr(curr_string, "?C=N;O=D")) != NULL){
		sortName = 1;
		sortOrder = 1;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);
	}
	else if((ptr = strstr(curr_string, "?C=M;O=A")) != NULL){
		sortDate = 1;
		sortOrder = 0;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);

	}
	else if((ptr = strstr(curr_string, "?C=M;O=D")) != NULL){
		sortDate = 1;
		sortOrder = 1;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);
	}
	else if((ptr = strstr(curr_string, "?C=S;O=A")) != NULL){
		sortSize = 1;
		sortOrder = 0;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);

	}
	else if((ptr = strstr(curr_string, "?C=S;O=D")) != NULL){
		sortSize = 1;
		sortOrder = 1;

		if(*(ptr-1) != '/')
			strncpy (ptr,"/",8);
		else
			strncpy (ptr,"",8);
	}


	cwd = getcwd(cwd, 256);

	if(curr_string[0] == '/' && curr_string[1] == '\0')
		strcat(cwd, "/http-root-dir/htdocs/index.html");
	else if(strstr(curr_string, "/icons") != NULL){
		strcat(cwd, "/http-root-dir");
		strcat(cwd, curr_string);
	}
	else if(strstr(curr_string, "/htdocs") != NULL){
		strcat(cwd, "/http-root-dir");
		strcat(cwd, curr_string);
	}
	else{
		strcat(cwd, "/http-root-dir/htdocs/");
		strcat(cwd, curr_string);
	}
	

	char actualPath[PATH_MAX];
	realpath(cwd, actualPath);

	char contentType[1024];
	if(strcmp(actualPath + strlen(actualPath)- 5, ".html") == 0 || strcmp(actualPath + strlen(actualPath)- 6, ".html/") == 0 )
		strcpy(contentType, "text/html\r\n\r\n");
	else if(strcmp(actualPath + strlen(actualPath)- 4, ".svg") == 0 || strcmp(actualPath + strlen(actualPath)- 5, ".svg/") == 0 )
		strcpy(contentType, "image/svg+xml\r\n\r\n");
	else if(strcmp(actualPath + strlen(actualPath)- 4, ".gif") == 0 || strcmp(actualPath + strlen(actualPath)- 5, ".gif/") == 0 )
		strcpy(contentType, "image/gif\r\n\r\n");
	else if(strcmp(actualPath + strlen(actualPath)- 4, ".xbm") == 0 || strcmp(actualPath + strlen(actualPath)- 5, ".xbm/") == 0 )
		strcpy(contentType, "image/x-xbitmap\r\n\r\n");
	else
		strcpy(contentType, "text/plain\r\n\r\n");



	struct stat s; // check if file or directory
	if( stat(actualPath, &s) == 0) {
		if(s.st_mode & S_IFDIR) { 
			//if it's a directory, store it

			write(socket, protocol, strlen(protocol));	
			write(socket, "text/html\r\n\r\n", strlen("text/html\r\n\r\n"));		

			write(socket, "<html>\n<head>\n<title>Index of ", strlen("<html>\n<head>\n<title>Index of "));
			write(socket, actualPath, strlen(actualPath));			
			write(socket, "</title>\n</head>\n<body>\n<h1>\nIndex of ", strlen("</title>\n</head>\n<body>\n<h1>\nIndex of "));
			write(socket, actualPath, strlen(actualPath));
			write(socket, "\n</h1>\n", strlen("\n</h1>\n"));

			DIR* dir;
			struct dirent *ent;
			dir = opendir(actualPath);

			std::vector<directoryFile> fileVector;

			while((ent = readdir(dir)) !=NULL) {
				if(strcmp(ent->d_name,".")== 0)
					continue;

				directoryFile df;
				

				df.name = strdup(ent->d_name);

				char* dirc = (char *)malloc(sizeof(char) * (100 + strlen(actualPath)));
				strcpy(dirc, curr_string);

				strcat(dirc, df.name);
				strcat(dirc, "/");

				df.directory = dirc;
				df.path = (char *)malloc(1024);
				strcpy(df.path, actualPath);
				strcat(df.path, "/");
				strcat(df.path, df.name);

				struct stat ss; // check if file or directory
				if( stat(df.path, &ss) == 0) {
					if(ss.st_mode & S_IFDIR)
						df.type = 1;
					else
						df.type = 0;
	
				}


				struct stat st;
				stat(df.path, &st);
				df.size = st.st_size;

				
				struct stat st2;
				stat(df.path, &st2);
				df.modifiedTime = st2.st_mtime;


				fileVector.push_back(df);
			}
				



			if(sortName == 1){
				if(sortOrder == 0)
					std::sort(fileVector.begin(), fileVector.end(), nameAscending);
				else
					std::sort(fileVector.begin(), fileVector.end(), nameDescending);
			}else if(sortSize == 1){
				if(sortOrder == 0)
					std::sort(fileVector.begin(), fileVector.end(), sizeAscending);					
				else
					std::sort(fileVector.begin(), fileVector.end(), sizeDescending);	
			}else if(sortDate == 1){
				if(sortOrder == 0)
					std::sort(fileVector.begin(), fileVector.end(), modifiedTimeAscending);					
				else
					std::sort(fileVector.begin(), fileVector.end(), modifiedTimeDescending);				
			}else{
				sortName = 1;
				std::sort(fileVector.begin(), fileVector.end(), nameAscending);
			}


			write(socket, "	 <table><tr><th><img src=\"/icons/blank.xbm\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O=", strlen("	 <table><tr><th><img src=\"/icons/blank.xbm\" alt=\"[ICO]\"></th><th><a href=\"?C=N;O="));
			if(sortName == 1){
				if(sortOrder == 0)
					write(socket, "D", 1);
				else
					write(socket, "A", 1);
			}else{
				write(socket, "A", 1);
			}
		
			write(socket, "\">Name</a></th><th><a href=\"?C=M;O=", strlen("\">Name</a></th><th><a href=\"?C=M;O="));
			if(sortDate == 1){
				if(sortOrder == 0)
					write(socket, "D", 1);
				else
					write(socket, "A", 1);
			}else{
				write(socket, "A", 1);
			}
			write(socket, "\">Last modified</a></th><th><a href=\"?C=S;O=", strlen("\">Last modified</a></th><th><a href=\"?C=S;O="));
			if(sortSize == 1){
				if(sortOrder == 0)
					write(socket, "D", 1);
				else
					write(socket, "A", 1);
			}else{
				write(socket, "A", 1);
			}
			write(socket, "\">Size</a></th></tr><tr><th colspan=\"5\"><hr></th></tr>\n", strlen("\">Size</a></th></tr><tr><th colspan=\"5\"><hr></th></tr>\n"));
			


			for(int i = 0; i < fileVector.size(); i++){
				if(strcmp(fileVector[i].name, "..") == 0){
					write(socket, "<tr><td valign=\"top\">", strlen("<tr><td valign=\"top\">"));
					write(socket, "<img src=\"/icons/menu.gif\" alt=\"[DIR]\"></td>", strlen("<img src=\"/icons/folder.gif\" alt=\"[DIR]\"></td>"));
					write(socket, "<td><a href=\"", strlen("<td><a href=\""));
					write(socket, fileVector[i].directory, strlen(fileVector[i].directory));
					write(socket, "\">", 2);
					write(socket, "Parent Directory", strlen("Parent Directory")); // file name
					write(socket, "</a>", 4);
					write(socket, "</td><td align=\"right\">", strlen("</td><td align=\"right\">"));	
					write(socket, "  - ", strlen("  - "));
					write(socket, "</td><td>&nbsp;</td></tr>", strlen("</td><td>&nbsp;</td></tr>"));

					fileVector.erase(fileVector.begin()+i);


					break;
				}
			}


			while(fileVector.size() != 0){
				write(socket, "<tr><td valign=\"top\">", strlen("<tr><td valign=\"top\">"));
				if(fileVector.back().type == 0){
					//it's a file
					write(socket, "<img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td>", strlen("<img src=\"/icons/unknown.gif\" alt=\"[   ]\"></td>"));
				}
				else{
					//it's a directory
					write(socket, "<img src=\"/icons/menu.gif\" alt=\"[DIR]\"></td>", strlen("<img src=\"/icons/folder.gif\" alt=\"[DIR]\"></td>"));
				}

				write(socket, "<td><a href=\"", strlen("<td><a href=\""));
				write(socket, fileVector.back().directory, strlen(fileVector.back().directory)); //filedirectory
				write(socket, "\">", 2);
				write(socket, fileVector.back().name, strlen(fileVector.back().name)); // file name
				write(socket, "</a>", 4);
				write(socket, "</td><td align=\"right\">", strlen("</td><td align=\"right\">"));

				struct stat info; 
				char buff[20]; 
				struct tm * timeinfo;
				stat(fileVector.back().path, &info); 
				timeinfo = localtime (&(info.st_mtime)); 
				strftime(buff, 20, "%b %d %H:%M", timeinfo);
				write(socket, buff, strlen(buff));

				write(socket, "</td><td align=\"right\">", strlen("</td><td align=\"right\">"));
 				size_t s = fileVector.back().size;
 				char sizeBuf[256] = "";
  				snprintf(sizeBuf, sizeof(sizeBuf), "%zu", s);
				fflush(stdout);
				write(socket, sizeBuf, strlen(sizeBuf));
				write(socket, "</td><td>&nbsp;</td></tr>", strlen("</td><td>&nbsp;</td></tr>"));

				fileVector.pop_back();
			}
						
			write(socket, "<tr><th colspan=\"5\"><hr></th></tr>\n</table>\n", strlen("<tr><th colspan=\"5\"><hr></th></tr>\n</table>\n"));

			write(socket, "</body></html>", strlen("</body></html>"));

		}
		else{
			//if it's a file, write it
			int fd = open(actualPath, O_RDONLY);
			write(socket, protocol, strlen(protocol));	
			write(socket, contentType, strlen(contentType));	

			char buf[1024];
			int bytesread;
			bytesread = read(fd, buf, 1024);
			while(bytesread != 0){
				write(socket, buf, bytesread);
				bytesread = read(fd, buf, 1024);
			}
			close(fd);
		}
	}
	else{
			//file not found, throw a 404
			write(socket, notFound, strlen(notFound));
			write(socket, protocol, strlen(protocol));
			write(socket, contentType, strlen(contentType));	
			write(socket, crlf, strlen(crlf));
			write(socket, crlf, strlen(crlf));
			write(socket, errorMessage, strlen(errorMessage));
	

	}

			clock_gettime(CLOCK_MONOTONIC, &rend);
			double requestTime = ((double)rend.tv_sec + 1.0e-9*rend.tv_nsec) - ((double)rstart.tv_sec + 1.0e-9*rstart.tv_nsec);	
			if(maxTime == 0 || minTime == 0){
				maxTime = requestTime;
				minTime = requestTime;
				minRequest = strdup(curr_string);
				maxRequest = strdup(curr_string);			
			}
			if(minTime >  requestTime){
				minTime = requestTime;
				minRequest = strdup(curr_string);
			}
			if(maxTime < requestTime){
				maxTime = requestTime;
				maxRequest = strdup(curr_string);
			}		


}

void killzombie(int signum) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

