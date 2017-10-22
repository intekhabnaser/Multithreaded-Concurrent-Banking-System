/* Bank server using TCP sockets and multithreading to service multiple clients 
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

int num_of_accounts;	//variable to store total number of accounts in server

float temp;

struct bank_record	//structure to store bank record of all account holders.
{
	long acc_number;
	string name;
	double balance;
	pthread_mutex_t lock;	//mutex object to apply locks on shared resources in order to synchronize events
}account[1000];

struct client_requests	//structure to store all transaction requests of each client.
{
	float start_time;
	long acc_number;
	char type;
	double amount;
}transaction;

int update(string line)	//function to service the transaction request of deposit or withdrawal in/from a particular account number
{
	int flag = 0;
	temp = 0;
	istringstream ss(line);
	ss >> transaction.start_time >> transaction.acc_number >> transaction.type >> transaction.amount;
	for(int i=0;i<num_of_accounts;i++)
	{
		if(account[i].acc_number==transaction.acc_number)
		{
			flag = 1;	//Account number found
			if(transaction.type=='d')	//deposit request
			{
				pthread_mutex_lock(&account[i].lock);
				account[i].balance = account[i].balance + transaction.amount;
				temp = account[i].balance;
				pthread_mutex_unlock(&account[i].lock);
				return 0;
			}
			else	//withdrawal request
			{
				if(account[i].balance < transaction.amount)	//insufficient balance
				{
					return -1;
				}
				else
				{
					pthread_mutex_lock(&account[i].lock);
					account[i].balance = account[i].balance - transaction.amount;
					temp = account[i].balance;
					pthread_mutex_unlock(&account[i].lock);
					return 0;
				}
			}
		}
		else
		{
			continue;
		}
	}
	if(flag==0)	//Account number not found
		return -2;
}

void error(const char *msg) //function to print error messages
{
	perror(msg);
	exit(1);
}

char buffer[256];
int n;

void *NewConnection(void *newsockfd) //thread function for each client request
{
	if((long)newsockfd < 0) 
		error("ERROR on accept");
	client_requests tmp;
	while(1) {
		bzero(buffer,256);
		read((long)newsockfd,buffer,255);
		int n = strcmp(buffer,"end");	//If the client sends "end" as request message, server stops waiting further for reading  	
		if(n==0){
			break;
		}
		string line(buffer);	//storing the client transaction request in a string which will be used to update the server records of account numbers
		int result = update(line);
		istringstream s1(line);
		s1 >> tmp.start_time >> tmp.acc_number >> tmp.type >> tmp.amount;
		if(result==-1)	//case for transaction failure on withdrawal due to insufficient balance
		{
			cout << "Data received from client " << (long)newsockfd << " : " << tmp.acc_number << " " << tmp.type << " " << tmp.amount << " [Failure! : Insufficient balance]" << endl;
			n = write((long)newsockfd,"Transaction failed : Cannot withdraw due to insufficient balance",64);
			if (n < 0) error("ERROR writing to socket");
		}
		else if(result==-2)	//case for transaction failure due to invalid account number
		{
			cout << "Data received from client " << (long)newsockfd << " : " << tmp.acc_number << " " << tmp.type << " " << tmp.amount << " [Failure! : Invalid Account Number]" << endl;
			n = write((long)newsockfd,"Transaction failed : Account number does not exist",50);
			if (n < 0) error("ERROR writing to socket");
		}
		else	//Successful transaction
		{
			cout << "Data received from client " << (long)newsockfd << " : " << tmp.acc_number << " " << tmp.type << " " << tmp.amount << " [Transaction Successful] | Updated Balance: " << temp << endl;
			n = write((long)newsockfd,"Transaction passed",18);
			if (n < 0) error("ERROR writing to socket");
		}
	}

	n = write((long)newsockfd,"All transactions completed. Thank You!",38);	//Sending/Writing final message to client 
	if (n < 0) error("ERROR writing to socket");
	close((long)newsockfd);
//	pthread_exit(NULL);
}

void *AddInterest(void * arg) //thread function for adding periodic interest to all eligible accounts
{
	float rate_of_interest = 1.1;
	while(1)
	{
		sleep(15);
		for(int i=0; i<num_of_accounts; i++)
		{
			float temp = 0;
			if(account[i].balance >= 100)
			{
				pthread_mutex_lock(&account[i].lock);
				account[i].balance = account[i].balance * rate_of_interest;
				temp = account[i].balance;
				pthread_mutex_unlock(&account[i].lock);
				cout << "10pc Interest deposited in Acc.No: " << account[i].acc_number << ". Updated Balance: " << temp << endl;
			}
			else
				continue;
		}
	}
}

int main(int argc, char *argv[])
{
	long sockfd, newsockfd[200], portno;
	socklen_t clilen;
	
	struct sockaddr_in serv_addr, cli_addr;
	long i=0;	// i := will hold the number of records in server

	//Opening the Records.txt file
	ifstream bank_file("Records.txt");
	
	string line;	//string for temporarily storing each line from file while reading it line-by-line

	while(getline(bank_file, line))	//Reading file line by line and storing the values in respective elements of the structure
	{
		istringstream ss(line);
		ss >> account[i].acc_number >> account[i].name >> account[i].balance;
		i++;
	}
	
	num_of_accounts = i;	//Updating the variable with number of accounts in bank server 

	cout << "Initial Records:" << endl;
	for(int i=0;i<num_of_accounts;i++)	//Printing the values read from file
	{
		cout << account[i].acc_number << "	" << account[i].name << "	" << account[i].balance << endl;
		
		if(pthread_mutex_init(&account[i].lock, NULL) != 0)	//Initializing all mutex objects for each account
		{
			cout << "mutex init has failed" << endl;
		}

	}	

	pthread_t threads;	//threads for handling client requests
	pthread_t interest;	//thread for periodically adding interest to all acounts
     
	int rc = pthread_create(&interest, NULL, AddInterest, NULL);

	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) 
		error("ERROR opening socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)	//To reuse socket address in case of crashes and failures
		perror("setsockopt(SO_REUSEADDR) failed");

	#ifdef SO_REUSEPORT
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
		perror("setsockopt(SO_REUSEPORT) failed");
	#endif

	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");
     	
	listen(sockfd,100);
	clilen = sizeof(cli_addr);

	cout << "Transaction Logs:" << endl;
	
	int sock_index = 0;

	while(1)	//Spawning threads with a limit taken as command line parameter
	{	
		newsockfd[sock_index] = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);

		pthread_create(&threads, NULL, NewConnection, (void *)newsockfd[sock_index]);
		
		sock_index=(sock_index+1)%200;
	}

/* Code never reaches beyond this point, but I had written these lines just for better understanding the flow for myself */
	
	for(int i=0;i<num_of_accounts;i++)	//Destroying all mutex objects for each account
	{
		pthread_mutex_destroy(&account[i].lock);
	}


	cout << "Updated Records:" << endl << "AcNo" << "	" << "Name" << "	" << "Bal" << endl;
	for(int j=0;j<i;j++)	//Printing the updated values of the bank records
		cout << account[j].acc_number << "	" << account[j].name << "	" << account[j].balance << endl;


	return 0; 
	pthread_exit(NULL);
}

