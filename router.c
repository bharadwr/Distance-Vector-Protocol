#include <sys/socket.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "ne.h"
#include "router.h"

#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

FILE* logger;
int secondsCount;
struct nbr_cost nbrLoadState[MAX_ROUTERS];
int failTiming[MAX_ROUTERS];
int checkValue = 0;

int open_listenfd_udp(int port);
int timer(int sec, int fd, char mode);
void sendInitRequest(struct pkt_INIT_REQUEST initRequest, int distServer, struct sockaddr_in networkClient, socklen_t neClientSize, int routerID);
struct pkt_INIT_RESPONSE getInitResponse(int distServer, struct pkt_INIT_RESPONSE initResponse, socklen_t initResponseSize);
int initializeRouter(struct pkt_INIT_RESPONSE initResponse, int routerID);
int maxFinder (int distServer, int distUpdate, int distConverge, int distSeconds, fd_set *setSocket);
void isset_helper(int *distServer, int *distConverge, int *distUpdate, int *distSeconds, fd_set *setSocket, struct sockaddr_in networkClient, int routerID, int nambiar);
void update_server_routes(int *distServer, int *distConverge, fd_set *setSocket, int routerID, int nambiar);
void send_server_updates(int *distServer, int *distUpdate, fd_set *setSocket, struct sockaddr_in networkClient, int routerID, int nambiar);
void time_converge(int *distConverge, fd_set *setSocket);
void time_increment(int *distSeconds, fd_set *setSocket); 
void update_server_timer(struct pkt_RT_UPDATE result, int *distConverge, int idx, int routerID);
void send_server_updates_helper(int nbrLoadStateNbr, struct pkt_RT_UPDATE packetUpdate, int *distServer, struct sockaddr_in networkClient, int idx, int routerID);


// Send the initialization response.
void sendInitRequest(struct pkt_INIT_REQUEST initRequest, int distServer, struct sockaddr_in networkClient, socklen_t neClientSize, int routerID)
{
    initRequest.router_id = htonl(routerID);
    sendto(distServer, &initRequest, sizeof(initRequest), 0, (struct sockaddr *)&networkClient, neClientSize);
    checkValue++;
}

// Receive the initialization response.
struct pkt_INIT_RESPONSE getInitResponse(int distServer, struct pkt_INIT_RESPONSE initResponse, socklen_t initResponseSize)
{
    recvfrom(distServer, &initResponse, initResponseSize, 0, NULL, NULL);
    ntoh_pkt_INIT_RESPONSE(&initResponse);
    checkValue++;
    return initResponse;
}

// Initialize the router.
int initializeRouter(struct pkt_INIT_RESPONSE initResponse, int routerID)
{
    if (checkValue >= 0)
    {
        InitRoutingTbl(&initResponse, routerID);
        int i = -1;
        int nambiar = initResponse.no_nbr;
        // Cost details.    
        while (++i < nambiar) nbrLoadState[i] = initResponse.nbrcost[i];
        i = -1;
        // Failure timing.
        while (++i < nambiar) failTiming[i] = timer(FAILURE_DETECTION, 0, 'C');  
        // Print the routes.
        PrintRoutes(logger, routerID);
	    return nambiar;
    }
    else
    {
        checkValue = 2;
        return -1;
    }
}

// UDP Connection. --- Obtained from 463 Sample Code and used in Project 1.
int open_listenfd_udp(int port)  
{ 
    int listenfd, optval=1; 
    struct sockaddr_in serveraddr; 

    /* Create a socket descriptor */ 
    if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    	return -1; 

    /* Eliminates "Address already in use" error from bind. */ 
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,  
	     (const void *)&optval , sizeof(int)) < 0) 
    	return -1; 

    /* Listenfd will be an endpoint for all requests to port 
     on any IP address for this host */ 
    bzero((char *) &serveraddr, sizeof(serveraddr)); 
    serveraddr.sin_family = AF_INET;  
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);  
    serveraddr.sin_port = htons((unsigned short)port);  
    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
    	return -1; 
    return listenfd; 
}

// Create a new timer or set the time.
int timer(int seconds, int interval, char mode) 
{
    // Timer is in set mode.
    if (mode == 'S')
    {
        struct itimerspec tOut = {.it_interval.tv_sec = 0, .it_interval.tv_nsec = 0, .it_value.tv_nsec = 0, .it_value.tv_sec = seconds};
        timerfd_settime(interval, 0, &tOut, NULL);
    }
    // Timer is in create mode.
    else if (mode == 'C')
    {
        interval = timerfd_create(CLOCK_MONOTONIC, 0);
        timer(seconds, interval, 'S');
        return interval;
    }
    return 0;

}

// Find the max.
int maxFinder (int distServer, int distUpdate, int distConverge, int distSeconds, fd_set *setSocket)
{
    int maxNum = 0;

    FD_SET(distServer, setSocket);
    maxNum = (distServer <= maxNum) ? maxNum : distServer;

    FD_SET(distUpdate, setSocket);
    maxNum = (distUpdate <= maxNum) ? maxNum : distUpdate;

    FD_SET(distConverge, setSocket);
    maxNum = (distConverge <= maxNum) ? maxNum : distConverge;

    FD_SET(distSeconds, setSocket);
    maxNum = (distSeconds <= maxNum) ? maxNum : distSeconds;

    return maxNum;
}

void isset_helper(int *distServer, int *distConverge, int *distUpdate, int *distSeconds, fd_set *setSocket, struct sockaddr_in networkClient, int routerID, int nambiar)
{
	if (distServer != NULL && distConverge != NULL && distUpdate != NULL && distSeconds != NULL)
	{
        if (checkValue >= 0)
        {
    	    update_server_routes(distServer, distConverge, setSocket, routerID, nambiar);
    	    send_server_updates(distServer, distUpdate, setSocket, networkClient, routerID, nambiar);
    	    time_converge(distConverge, setSocket);
    	    time_increment(distSeconds, setSocket);
            checkValue = 3;
        }
        else
        {
            return;
        }
	}
    else
    {
        return;
    }
}

void update_server_timer(struct pkt_RT_UPDATE result, int *distConverge, int idx, int routerID)
{
	if (distConverge == NULL)
	{
		return;
	}
	else if (checkValue >= 0)
	{
    	timer(FAILURE_DETECTION, failTiming[idx], 'S');
    	int updateCheck = UpdateRoutes(&result, nbrLoadState[idx].cost, routerID);
    	// The route has been updated.
    	if (updateCheck)
    	{
        	PrintRoutes(logger, routerID);
            checkValue = 1;
        	timer(CONVERGE_TIMEOUT, *distConverge, 'S');
    	}
        else
        {
            checkValue++;
            return;
        }
	}
    else
    {
        return;
    }
}

void update_server_routes(int *distServer, int *distConverge, fd_set *setSocket, int routerID, int nambiar)
{
    if (FD_ISSET(*distServer, setSocket))
    {
		if (distServer != NULL)
		{
		    struct pkt_RT_UPDATE result;
            if (checkValue >= 0)
            {
			    socklen_t updateResSize = sizeof(result);
		        recvfrom(*distServer, &result, updateResSize, 0, NULL, NULL);
		        ntoh_pkt_RT_UPDATE(&result);
            }
            else
            {
                return;
            }
    
		    int i = -1;

			if (distConverge != NULL)
			{

				// Iterate through neighbors.
				while (++i < nambiar) 
				{
				    bool equalCheck = (nbrLoadState[i].nbr == result.sender_id);
				    if (equalCheck == true && checkValue >= 0)
				    {
                        checkValue = 2;
				        break;
				    }
				    else
				    {
				        continue;
				    }
				}
				update_server_timer(result, distConverge, i, routerID);
			}
		}
    }
}

void send_server_updates_helper(int nbrLoadStateNbr, struct pkt_RT_UPDATE packetUpdate, int *distServer, struct sockaddr_in networkClient, int idx, int routerID)
{
    if (checkValue < 0)
    {
        return;
    }
    else
    {
        packetUpdate.dest_id = nbrLoadStateNbr;
	    socklen_t neClientSize = sizeof(networkClient);
        ConvertTabletoPkt(&packetUpdate, routerID);

	    if (distServer != NULL)
	    {
            hton_pkt_RT_UPDATE(&packetUpdate);
		    struct sockaddr *client = (struct sockaddr*)&networkClient;
    	    sendto(*distServer, &packetUpdate, sizeof(packetUpdate), 0, client, neClientSize);
	    }
        else
        {
            checkValue = 0;
            return;
        }
    }
    return;
}

void send_server_updates(int *distServer, int *distUpdate, fd_set *setSocket, struct sockaddr_in networkClient, int routerID, int nambiar)
{
	if (distServer == NULL || distUpdate == NULL || checkValue < 0)
	{
		return;
	}
	else
	{
    	if (FD_ISSET(*distUpdate, setSocket))
    	{
        	int i = -1;
        	struct pkt_RT_UPDATE packetUpdate;
        	while (++i < nambiar) send_server_updates_helper(nbrLoadState[i].nbr, packetUpdate, distServer, networkClient, i, routerID);
        	timer(UPDATE_INTERVAL, *distUpdate, 'S');
    	}
	}
}

void time_converge(int *distConverge, fd_set *setSocket)
{
	if (distConverge == NULL)
	{
		return;
	}
	else
	{
    	if (FD_ISSET(*distConverge, setSocket))
    	{
        	timer(0, *distConverge, 'S');
        	fprintf(logger, "%d:Converged\n", secondsCount);
        	fflush(logger);
    	}
	}
}

void time_increment(int *distSeconds, fd_set *setSocket)
{
	if (distSeconds == NULL)
	{
		return;
	}
	else if (checkValue >= 0)
	{
    	if (FD_ISSET(*distSeconds, setSocket))
    	{
        	timer(1, *distSeconds, 'S');
        	secondsCount += 1;
    	}
	}
    else
    {
        return;
    }
}

int uninstallFunc(int nbrLoadState, int failTime, int routerID)
{
    UninstallRoutesOnNbrDeath(nbrLoadState);
    PrintRoutes(logger, routerID);
    timer(0, failTime, 'S');
	return 1;
}

int main(int argc, char **argv) 
{
    // Input arguments.
    if (argc != 5) 
    {
        fprintf(stderr, "USAGE ./router <router id> <ne hostname> <ne UDP port> <router UDP port> \n");
        return EXIT_FAILURE;
    }
	else 
	{
		int routerID = atoi(argv[1]);
		int distServer = open_listenfd_udp(atoi(argv[4]));
		char* neHostName = argv[2]; 
		int neUDPPort = atoi(argv[3]);
		
		// Create the NE Client.
		struct sockaddr_in networkClient;
		socklen_t neClientSize = sizeof(networkClient);
		memset(&networkClient, 0, neClientSize);
		unsigned short conv = (unsigned short)neUDPPort;
		networkClient.sin_port = htons(conv);
		networkClient.sin_family = AF_INET;
		inet_aton(neHostName, &networkClient.sin_addr);
		
		// Logfiles.
		char fileName[20];
		sprintf(fileName, "router%d.log", routerID);
		logger = fopen(fileName, "w");
		
		int distUpdate = timer(UPDATE_INTERVAL, 0, 'C');
		int distConverge = timer(CONVERGE_TIMEOUT, 0, 'C');
		int distSeconds = timer(1, 0, 'C');
		secondsCount = 0;

		// Send Init Request.
		struct pkt_INIT_REQUEST initRequest;
		sendInitRequest(initRequest, distServer, networkClient, neClientSize, routerID);
		
		// Receive Init Response.
		struct pkt_INIT_RESPONSE initResponse;
		socklen_t initResponseSize = sizeof(initResponse);
		initResponse = getInitResponse(distServer, initResponse, initResponseSize);
		
		// Initialize Router 
		int nambiar = initializeRouter(initResponse, routerID);


		fd_set setSocket;

		while (true) 
		{
		    int i = -1;
			int max_dist = 0;
		    
			FD_ZERO(&setSocket);

            if (checkValue >= 0)
            {
		        max_dist = maxFinder(distServer, distUpdate, distConverge, distSeconds, &setSocket);
            }
            else
            {
                return EXIT_FAILURE;
            }
			
		    while (++i < nambiar && checkValue >= 0) 
		    {
		        FD_SET(failTiming[i], &setSocket);
		        max_dist = (failTiming[i] <= max_dist)? max_dist : failTiming[i];
		    }
		    
			select(max_dist + 1, &setSocket, NULL, NULL, NULL);
		    isset_helper(&distServer, &distConverge, &distUpdate, &distSeconds, &setSocket, networkClient, routerID, nambiar);
			
			i = -1;
			int a = 0;
		    while (++i < nambiar) a = (FD_ISSET(failTiming[i], &setSocket))? uninstallFunc(nbrLoadState[i].nbr, failTiming[i], routerID) : -1;
		}
		return EXIT_SUCCESS;
	}
}
