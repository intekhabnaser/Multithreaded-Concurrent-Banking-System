/*
Client reads list of transactions from a file and sends across the requests to server for processing based on particular account numbers 
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
using namespace std;

int num_of_transactions;	//variable to store total number of transactions

/* Structure for storing and triggering the transaction details to server */
struct client_requests
{
	float start_time;
	long acc_number;
	char type;
	double amount;
}transaction[1000];

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

int main(int argc, char *argv[])
{
	int i=0;	// i:= variable to count number of transactions
	double total_time = 0;
	long sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[256];
	
	//Opening the Transactions.txt file
	ifstream transaction_file("Transactions.txt");

	string line;	//string for temporarily storing each line from file while reading it line-by-line

	if(argc < 3) {
		fprintf(stderr,"usage %s hostname port\n", argv[0]);
		exit(0);
	}
	
	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) 
		error("ERROR opening socket");
	
	server = gethostbyname(argv[1]);
	
	if(server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	
	if(connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");

	while(getline(transaction_file, line))	//Reading file line by line, storing the values in respective elements of the structure and sending the transaction request to server
	{       
		bzero(buffer,256);
		strcpy(buffer,line.c_str());		
		
		istringstream ss(line);
		ss >> transaction[i].start_time >> transaction[i].acc_number >> transaction[i].type >> transaction[i].amount;

		float t=0;	//variable for storing timestamp for each transaction and sending request to server at that timestamp 
		if(i==0){
			t = transaction[i].start_time;	//First transaction timestamp
			cout << "TS: [" << t << "] > for transaction: " << transaction[i].acc_number << " " << transaction[i].type << " " << transaction[i].amount;
			usleep(t);
		}
		else{
			t = (transaction[i].start_time - transaction[i-1].start_time);	//Calculating current timestamp with respect to timestamp of previous transaction
			cout << "TS: [" << t << "] > for transaction: " << transaction[i].acc_number << " " << transaction[i].type << " " << transaction[i].amount;
			usleep(t);
		}
		
		i++;

		float start_s=clock();	// fetching the start time of this transaction request

		n = write(sockfd,buffer,strlen(buffer));	//Send/Write the transaction request to server

		if (n < 0) 
			error("ERROR writing to socket");

		bzero(buffer,256);
		n = read(sockfd,buffer,255);	//Read the result of the transaction from server
		
		float stop_s=clock();	// fetching the end time of this transaction request
		
		double trans_time = (stop_s-start_s)*1000/double(CLOCKS_PER_SEC);

		cout << " | Transaction Time: " << trans_time << " | ";
	
		if(n < 0) 
			error("ERROR reading from socket");
		
		cout << buffer << endl;
		
		total_time += trans_time;
	}

	num_of_transactions = i;

	bzero(buffer,256);
	strcpy(buffer,"end");	//To send a value "end" to server to denote it's transactions are now completed. So that server may terminate the connection.
	n = write(sockfd,buffer,strlen(buffer));

	if (n < 0) 
		error("ERROR writing to socket");

	
	bzero(buffer,256);
	n = read(sockfd,buffer,255);	//Read the final message from server
	
	if(n < 0) 
		error("ERROR reading from socket");
	cout << "Total Time for All Transactions: [" << total_time << "] secs" << endl;
	cout << buffer << endl;
	close(sockfd);	//Close the client socket and terminate the connection
	return 0;
}

