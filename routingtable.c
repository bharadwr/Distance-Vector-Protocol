#include <stdio.h>
#include "ne.h"
#include "router.h"
#include <stdbool.h>

int numRoutes;
struct route_entry routingTable[MAX_ROUTERS];

/* Routine Name    : InitRoutingTbl
 * INPUT ARGUMENTS : 1. (struct pkt_INIT_RESPONSE *) - The INIT_RESPONSE from Network Emulator
 *                   2. int - My router's id received from command line argument.
 * RETURN VALUE    : void
 * USAGE           : This routine is called after receiving the INIT_RESPONSE message from the Network Emulator. 
 *                   It initializes the routing table with the bootstrap neighbor information in INIT_RESPONSE.  
 *                   Also sets up a route to itself (self-route) with next_hop as itself and cost as 0.
 */

void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID)
{
    // Invalid initial response.
    if (InitResponse == NULL)
    {
        return;
    } 
    else 
    {
	    int currentRoutes = InitResponse -> no_nbr;
	    currentRoutes += 1;	

	    // Initialize Self Routes
	    routingTable[0].dest_id = myID;
	    routingTable[0].next_hop = myID;
	    routingTable[0].cost = 0;

	    numRoutes = currentRoutes;

	    int i = 0;
	    while (++i < numRoutes)
	    {
		    routingTable[i].dest_id = InitResponse -> nbrcost[i - 1].nbr;
		    routingTable[i].next_hop = InitResponse -> nbrcost[i - 1].nbr;
		    routingTable[i].cost = (InitResponse -> nbrcost[i - 1].cost < INFINITY) ? InitResponse -> nbrcost[i - 1].cost : routingTable[i].cost;
	    }
    }
    return;
}

/* Routine Name    : UpdateRoutes
 * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - The Route Update message from one of the neighbors of the router.
 *                   2. int - The direct cost to the neighbor who sent the update. 
 *                   3. int - My router's id received from command line argument.
 * RETURN VALUE    : int - Return 1 : if the routing table has updated on running the function.
 *                         Return 0 : Otherwise.
 * USAGE           : This routine is called after receiving the route update from any neighbor. 
 *                   The routing table is then updated after running the distance vector protocol. 
 *                   It installs any new route received, that is previously unknown. For known routes, 
 *                   it finds the shortest path using current cost and received cost. 
 *                   It also implements the forced update and split horizon rules. My router's id
 *                   that is passed as argument may be useful in applying split horizon rule.
 */
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID) 
{
    // Validity Check.
    if (RecvdUpdatePacket == NULL) 
    {
        return 0;
    }    
    else 
    {
        int outerLoop = 0;
        int updated = 0;
        while (outerLoop < RecvdUpdatePacket -> no_routes)
        {
            int innerLoop = 0;
            while (innerLoop < numRoutes)
            {
                if (RecvdUpdatePacket -> route[outerLoop].dest_id == routingTable[innerLoop].dest_id) 
                {
                    // Check the total cost.
                    int totalCost = costToNbr + RecvdUpdatePacket->route[outerLoop].cost;
                    if (totalCost >= INFINITY)
                    {
                        totalCost = INFINITY;
                    }
                    else
                    {
                        totalCost = costToNbr + RecvdUpdatePacket->route[outerLoop].cost;
                    }

                    // Split Horizon Check.
                    bool splitHorizonCheck;
                    if (RecvdUpdatePacket -> route[outerLoop].next_hop == myID)
                    {
                        splitHorizonCheck = false;
                    }
                    else
                    {
                        splitHorizonCheck = true;
                    }


                
                    // Forced Update Check.
                    bool forcedUpdateCheck;
                    if (routingTable[innerLoop].next_hop == RecvdUpdatePacket -> sender_id)
                    {
                        if (routingTable[innerLoop].cost != totalCost)
                        {
                            forcedUpdateCheck = true;
                        }
                        else
                        {
                            forcedUpdateCheck = false;
                        }
                    }
                    else
                    {
                        forcedUpdateCheck = false;
                    }

                    // Check for updated cost.
                    bool costCheck;
                    if (routingTable[innerLoop].cost <= totalCost)
                    {
                        costCheck = false;
                    }
                    else
                    {
                        costCheck = true;
                    }

                    // Distance Vector Algorithm.
                    if (forcedUpdateCheck || (splitHorizonCheck && costCheck)) 
                    {
                        updated = 1;
                        routingTable[innerLoop].cost = (totalCost > INFINITY) ? INFINITY : totalCost;
                        routingTable[innerLoop].next_hop = RecvdUpdatePacket -> sender_id;
                        routingTable[innerLoop].dest_id = RecvdUpdatePacket -> route[outerLoop].dest_id;
                    }
                    break;
                }
                innerLoop++;
            }
            
            // Not in routing table.
            bool innerCheck = (innerLoop != numRoutes);
            if (!innerCheck)
            {
                int subCost = costToNbr + RecvdUpdatePacket->route[outerLoop].cost;
                routingTable[innerLoop].cost = (subCost > INFINITY) ? INFINITY : subCost;

                routingTable[innerLoop].dest_id = RecvdUpdatePacket->route[outerLoop].dest_id;
                routingTable[innerLoop].next_hop = RecvdUpdatePacket->sender_id;
                numRoutes += 1;
                updated = 1;
            }
            outerLoop++;
        }

        return updated;
    }
}

/* Routine Name    : ConvertTabletoPkt
 * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - An empty pkt_RT_UPDATE structure
 *                   2. int - My router's id received from command line argument.
 * RETURN VALUE    : void
 * USAGE           : This routine fills the routing table into the empty struct pkt_RT_UPDATE. 
 *                   My router's id  is copied to the sender_id in pkt_RT_UPDATE. 
 *                   Note that the dest_id is not filled in this function. When this update message 
 *                   is sent to all neighbors of the router, the dest_id is filled.
 */
void ConvertTabletoPkt (struct pkt_RT_UPDATE *UpdatePacketToSend, int myID)
{
    // If the update packet is invalid.
    if (UpdatePacketToSend == NULL)
    {
        return;
    }
    else
    {
	    int i = -1;
	    // Update packet to send for all routes.
	    while (++i < numRoutes) UpdatePacketToSend -> route[i] = routingTable[i];
	    
        // Copy the parameters.
	    UpdatePacketToSend -> no_routes = numRoutes;
	    UpdatePacketToSend -> sender_id = myID;
    }
    return;
}

/* Routine Name    : PrintRoutes
 * INPUT ARGUMENTS : 1. (FILE *) - Pointer to the log file created in router.c, with a filename that uses MyRouter's id.
 *                   2. int - My router's id received from command line argument.
 * RETURN VALUE    : void
 * USAGE           : This routine prints the routing table to the log file 
 *                   according to the format and rules specified in the Handout.
 */
void PrintRoutes (FILE * Logfile, int myID)
{
    // Invalid log file.
    if (Logfile == NULL)
    {
        printf("ERROR: Invalid log file, could not write\n");
        return;
    }
    else
    {
	    fprintf (Logfile, "Routing Table:\n");
	    int i = -1;

	    // Print the table.
	    while (++i < numRoutes)
	    {
		    fprintf (Logfile, "R%d -> ", myID);
		    fprintf (Logfile, "R%d: ", routingTable[i].dest_id);
		    fprintf (Logfile, "R%d, ", routingTable[i].next_hop);
		    fprintf (Logfile, "%d\n", routingTable[i].cost);
	    }
	    fflush (Logfile);
    }
    return;
}

/* Routine Name    : UninstallRoutesOnNbrDeath
 * INPUT ARGUMENTS : 1. int - The id of the inactive neighbor 
 *                   (one who didn't send Route Update for FAILURE_DETECTION seconds).
 *                   
 * RETURN VALUE    : void
 * USAGE           : This function is invoked when a nbr is found to be dead. The function checks all routes that
 *                   use this nbr as next hop, and changes the cost to INFINITY.
 */
void UninstallRoutesOnNbrDeath (int DeadNbr)
{
	int i = -1;

	// Check all the dead routes.
	while (++i < numRoutes) routingTable[i].cost = (routingTable[i].next_hop == DeadNbr) ? INFINITY : routingTable[i].cost;

    return;
}
