#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "Block.h"
#include "Transaction.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "43012"
//using namespace std;

int timePerBlock = 10;
int complexity;
int totalPower = 0;

std::vector<Block> Blockchain;

std::map<SOCKET, bool> MinerClients;
std::map<SOCKET, bool> UserClients;

int totalTransactionNumber = 0;
std::map<int,Transaction> UnusedTransactions;

//Update each miner client on current block mining complexity when a new miner connects/disconnects
void SendComplexityToEveryone() {
	
	std::string s = std::string("Complexity:") + std::to_string(complexity) + std::string(";");
	char const* sendbuf = s.c_str();
	
	for (std::map<SOCKET, bool>::iterator it = MinerClients.begin(); it != MinerClients.end(); ++it) {
		SOCKET ClientSocket = it->first;
		int iSendResult;

		//std::cout << "Sending out complexity data: " << sendbuf << std::endl;

		iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}
	}
	std::cout << "LOG: Sent out updated complexity data to everyone: " << sendbuf << std::endl;
}

//Send a newly received and checked block to everyone
void SendBlockToEveryone(std::string compressedBlock) {

	char const* sendbuf = compressedBlock.c_str();

	for (std::map<SOCKET, bool>::iterator it = MinerClients.begin(); it != MinerClients.end(); ++it) {
		SOCKET ClientSocket = it->first;
		int iSendResult;

		//std::cout << "Sending out block data: " << sendbuf << std::endl;

		iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}
	}
	std::cout << "LOG: Sent out block data to everyone:\n" << sendbuf << std::endl;
}

//Send all blocks to a newly connected client
void SendBlockchainToClient(SOCKET ClientSocket) {
	
	int iSendResult;
	//std::cout << "LOG: Sending out blockchain to a new client\n";
	
	for (int i = 0; i <= Blockchain.size(); ++i) {
		Block blockToSend = Block();
		if (i < Blockchain.size())
			blockToSend = Blockchain[i];
		
		std::string stringToSend = "Block:" + blockToSend.CompressBlock() + ";";
		char const *sendbuf = stringToSend.c_str();

	
		iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}
		Sleep(10);
	}

	std::cout << "LOG: Sent out the blockchain to the new client\n";

}

//Send all unhandled transactions to a newly connected client
void SendUnusedTransactionsToClient(SOCKET ClientSocket) {

	int iSendResult;
	//std::cout << "Sending out all transactions to client\n";

	for (std::map<int, Transaction>::iterator it = UnusedTransactions.begin(); it != UnusedTransactions.end(); ++it) {
		Transaction transactionToSend = it->second;
		std::string stringToSend = "Transaction:" + transactionToSend.CompressTransaction() + ";";
		char const *sendbuf = stringToSend.c_str();

		iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}
		Sleep(10);
	}

	Transaction transactionToSend = Transaction();
	std::string stringToSend = "Transaction:" + transactionToSend.CompressTransaction() + ";";
	char const *sendbuf = stringToSend.c_str();

	iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		//WSACleanup();
	}

	std::cout << "LOG: Sent out all unconfirmed transactions to the new client\n";

}

//Send a new received transaction to all connected miners
void SendTransactionToMiners(std::string compressedTransaction) {
	
	char const* sendbuf = compressedTransaction.c_str();
	
	for (std::map<SOCKET, bool>::iterator it = MinerClients.begin(); it != MinerClients.end(); ++it) {
		SOCKET ClientSocket = it->first;
		int iSendResult;

		//std::cout << "Sending out transaction data: " << sendbuf << std::endl;

		iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}
	}
	std::cout << "LOG: Sent out the new transaction to everyone: " << sendbuf << std::endl;
}

//Check the legality of a transaction, currently does nothing
void CheckTransaction(std::string transactionData, SOCKET ClientSocket) {
	bool isTransactionCorrect = true;
	
	std::string message = "Confirmation:";
	if (isTransactionCorrect) {
		message += "True;";
		Transaction newTr = Transaction(++totalTransactionNumber, transactionData);
		UnusedTransactions.insert(std::pair<int, Transaction>(newTr.number, newTr));
		SendTransactionToMiners("Transaction:"+newTr.CompressTransaction()+";");
	}
	else {
		message += "False;";
	}

	int iSendResult;
	char const *sendbuf = message.c_str();

	//std::cout << "Sending out confirmation: " << sendbuf << std::endl;

	iSendResult = send(ClientSocket, sendbuf, (int)strlen(sendbuf), 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		//WSACleanup();
	}
}

//Remove a transaction that has been added to a mined block from the pool
void RemoveTransactionsUsedInBlock(std::string data) {
	int pos = data.find("!");
	while (pos > 0) {
		int num = stoi(data.substr(0, pos), nullptr, 10);
		UnusedTransactions.erase(num);
		data = data.substr(pos + 1);
		pos = data.find("!");
		data = data.substr(pos + 1);
		pos = data.find("!");
	}
}

//Parser of the data coming from clients
void HandleIncomingData(std::string data, SOCKET ClientSocket) {
	size_t end = data.find(';');
	if (end < 0) {
		printf("Received data in wrong format!\n");
		return;
	}
	size_t start = data.find(':');
	if (start < 0) {
		printf("Received data in wrong format!\n");
		return;
	}
	std::string type = data.substr(0, start);
	data = data.substr(start + 1, end - start - 1);
	if (type == "Block") {
		std::cout << "Received block data;\n";
		Block newBlock = Block(data);
		RemoveTransactionsUsedInBlock(newBlock.data);
		Blockchain.push_back(newBlock);
		SendBlockToEveryone("Block:" + newBlock.CompressBlock() + ";");
	}
	else if (type == "Transaction") {
		std::cout << "Received transaction data\n";
		CheckTransaction(data, ClientSocket);
	}
	else {
		printf("Received data in wrong format!\n");
		return;
	}
}

//Session with a client
DWORD WINAPI ClientSession(LPVOID data)
{
	// Process the client.

	SOCKET ClientSocket = (SOCKET)data;

	int iSendResult;
	int iResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int thisClientPower;

	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
	}

	//std::cout << "Got an incoming connection, accepting\n";

	int clientType;

	iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
	if (iResult > 0) {
		//printf("Data received: %.*s\n", iResult, (char*)recvbuf);
		std::string result = std::string(recvbuf);
		size_t end = result.find(';');
		result = result.substr(0, end);
		if (result == "Miner") {
			clientType = 1;
			std::cout << "LOG: A new miner has connected on socket " + std::to_string(ClientSocket) << std::endl;
		}
		else if (result == "Transactions") {
			clientType = 2;
			std::cout << "LOG: A new transaction-generating client has connected on socket " + std::to_string(ClientSocket) << std::endl;
		}
	}
	else if (iResult == 0)
		std::cout << "LOG: Connection closed with client on socket " + std::to_string(ClientSocket) << std::endl;
	else
		printf("recv failed with error: %d\n", WSAGetLastError());

	if (clientType == 1) {
		
		// Add client to table
		MinerClients.insert(std::pair<SOCKET, bool>(ClientSocket, true));

		// Sending existing blockchain to client
		SendBlockchainToClient(ClientSocket);

		// Sending existing transactions to client
		SendUnusedTransactionsToClient(ClientSocket);

		// Receiving computational power from client
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			//printf("Data received: %.*s\n", iResult, (char*)recvbuf);
			std::string result = std::string(recvbuf);
			std::string temp;
			std::size_t pos1 = result.find("Power:");
			if (pos1 >= 0) {
				std::size_t pos2 = result.find(";");
				temp = result.substr(pos1 + 6, pos2 - 1);
				thisClientPower = std::stoi(temp, nullptr, 10);
				totalPower += thisClientPower;
				complexity = floor(log2(totalPower*timePerBlock));
			}
		}
		else if (iResult == 0)
			std::cout << "LOG: Connection closed with client on socket " + std::to_string(ClientSocket) << std::endl;
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

		// Sending complexity to everyone
		SendComplexityToEveryone();
	}
	else {
		UserClients.insert(std::pair<SOCKET, bool>(ClientSocket, true));
	}

	// Receiving all types of new data until connection is closed
	do {
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			std::cout << "LOG: Received data from client on socket " + std::to_string(ClientSocket) << ": ";
			//printf("Bytes received: %d\n", iResult);
			HandleIncomingData(std::string(recvbuf), ClientSocket);
		}
		else if (iResult == 0)
			std::cout << "LOG: Connection closed with client on socket " + std::to_string(ClientSocket) << std::endl;
			//printf("Connection closing...\n");
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			//WSACleanup();
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		//WSACleanup();
	}

	// cleanup
	closesocket(ClientSocket);
	//WSACleanup();

	// Removing client data
	if (clientType == 1) {
		MinerClients.erase(ClientSocket);
		totalPower -= thisClientPower;
		complexity = floor(log2(totalPower*timePerBlock));

		SendComplexityToEveryone();
	}
	else {
		UserClients.erase(ClientSocket);
	}

	std::cout << "LOG: Connection closed with client on socket " + std::to_string(ClientSocket) << std::endl;

	return 0;

}

//Not mine, a function I found on the Internet
SOCKET SetUpListeningSocket() {
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return INVALID_SOCKET;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return INVALID_SOCKET;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	freeaddrinfo(result);

	std::cout << "LOG: Listening for an incoming connection\n";

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	return ListenSocket;
}

//Main function
int __cdecl main(void)
{
	std::cout << "LOG: Server started\n";

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	ListenSocket = SetUpListeningSocket();

	// Accept a client connection
	while ((ClientSocket = accept(ListenSocket, NULL, NULL))) {
		// Create a new thread for the accepted client (also pass the accepted client socket).
		DWORD dwThreadId;
		CreateThread(NULL, 0, ClientSession, (LPVOID)ClientSocket, 0, &dwThreadId);
	}

	std::string input;
	std::cin >> input;

	//std::cout << "Client closed the connection, shutting down\n";

	// shutdown the connection since we're done
	/*iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}*/

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}