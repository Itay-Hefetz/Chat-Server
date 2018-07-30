#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#define BUFLEN 4096
typedef struct fds{ //fd node
	int fd;
	struct fds *next;
}fds;
typedef struct list{// list of fds
	fds *head; // Pointer to head of list
}list;
int isNumber(char* str);
void badUsage();
void add_fd(int fd);
void destAndNull (void* x);
void error(char* err);
void remove_fd(int fd);
void sigHandler(int val);
list* listfds;
int serverfd;
int main(int argc , char *argv[]){
	if (argc !=2 ||!isNumber(argv[1])) //check input from argvs
		badUsage();
	signal(SIGINT, sigHandler);// Signal ctrl^c - when the program will end clean memory.
	char buffer[BUFLEN];
	struct sockaddr_in address;
	int nbytes, act, sockfd, addrlen, max_sd, sd, current_sock, length, flag;
	fds* sd_node, *node;
	if( (serverfd = socket(AF_INET , SOCK_STREAM , 0)) == 0) // create server socket
		error("socket failed");
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(atoi(argv[1]));
	if (bind(serverfd, (struct sockaddr *)&address, sizeof(address))<0) //bind the socket
		error("bind");
	if (listen(serverfd, 5) < 0)
		error("listen");
	addrlen = sizeof(address);
	listfds = (list*) malloc(sizeof(list));
	if (listfds == NULL)
		error("malloc");
	listfds->head = NULL;
	fd_set readfds;
	fd_set writefds;
	while(1){
		FD_ZERO(&readfds); //initialize the set to the null set
		FD_SET(serverfd, &readfds); //add server socket to set
		FD_ZERO(&writefds);
		FD_SET(serverfd, &writefds);
		max_sd = serverfd;
		for (sd_node = listfds->head; sd_node!= NULL ;sd_node = sd_node->next ){
			sd = sd_node->fd;
			FD_SET( sd , &readfds); //add client socket to reads set
			FD_SET( sd , &writefds);//add client socket to writes set
			if(sd > max_sd)//max file descriptor number
				max_sd = sd;
		}
		act = select(max_sd + 1 , &readfds , &writefds , NULL , NULL);
		if ((act < 0) && (errno!=EINTR)) // in case of fail perror and exit
			printf("select error");  
		if (FD_ISSET(serverfd, &readfds)) {
			if ((sockfd = accept(serverfd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
				error("accept");
			add_fd(sockfd);
		}
		sd_node = listfds->head;
		while (sd_node != NULL){
			flag = 0;
			sd = sd_node->fd;
			if (FD_ISSET( sd , &readfds)) {
				printf("server is ready to read from socket %d\n", sd);
				if ((nbytes = recv(sd , buffer, BUFLEN, MSG_PEEK)) < 0){
					error("recv");
				}
				else if (nbytes == 0){//Check if it was for closing
					fds* temp = sd_node->next;
					FD_CLR(sd, &readfds);
					FD_CLR(sd, &writefds);
					close(sd);
					remove_fd(sd);
					flag = 1;
					sd_node =temp;
				}
				else{ //echo back the message that came in
					length = strstr(buffer, "\r\n")-buffer+2;
					if ( (nbytes = recv(sd, buffer, length, 0)) < 0 )
						error("recv");
					buffer[nbytes] = '\0';//set NULL byte on the end of the data read
					char message[BUFLEN];
   					sprintf(message, "guest%d: %s",sd, buffer);
					for (node = listfds->head; node!= NULL ;node = node->next ){
						current_sock = node->fd;
						if (FD_ISSET( current_sock , &writefds)){
							printf("Server is ready to write to socket %d\n", current_sock);
							if ((write(current_sock, message, strlen(message))) < 0)
								error("write");
						}
					}
				}
			}
			if (flag == 0)
				sd_node = sd_node->next;
		}
	}
}
/* Signal Handler - clears memory when server is closed */
void sigHandler(int val) {
	fds *temp = NULL;
	if (listfds!=NULL){
		while (listfds->head!=NULL){
			temp = listfds->head;
			listfds->head = listfds->head->next;
		close(temp->fd);
		destAndNull(temp);
		}
		destAndNull(listfds);
	}
	close(serverfd);
	exit(0);
}
/* Remove fd - delete a node from the list */
void remove_fd(int fd){
	fds* tempNode = listfds->head;
	fds* nextNode = listfds->head;
	while (tempNode != NULL && tempNode->fd != fd)
		tempNode = tempNode->next;
	if (tempNode == NULL )
		return;
	if (listfds->head == tempNode)
		listfds->head = listfds->head->next;
	else{
		while(nextNode->next->fd != fd)
			nextNode = nextNode->next;
		nextNode->next = nextNode->next->next;
	}
	destAndNull(tempNode);
}
/* Add fd - add a node to the beginning of the list */
void add_fd(int numfd){
	fds *new_head = (fds*)malloc(sizeof(fds));
	if (new_head == NULL)
		error("malloc");
	new_head->fd = numfd;
	if (listfds->head!=NULL)
		new_head->next = listfds->head;
	else
		new_head->next = NULL;
	listfds->head = new_head;
}
/* Check if String is a number - return 1 with success, 0 if fail*/
int isNumber(char* str){
	int i=0;
	for (; i<strlen(str); i++)// check if str[i] is a number. only integer
		if ( *(str+i) > '9' || *(str+i) <'0'|| i==8)
			return 0;
	return 1;
}
/* show an error meesage and exit(EXIT_FAILURE) */
void badUsage(){
	fprintf(stderr,"Usage: chatserver <port>\n");
	exit(EXIT_FAILURE);
}
/* Error- show an error meesage and exit(EXIT_FAILURE) */
void error(char* err){
	perror(err);
	exit(EXIT_FAILURE);
}
/* Destroy And Null- Destroys and set as Null */
void destAndNull (void* x){
	free(x);
	x = NULL;
}
