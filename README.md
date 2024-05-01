# TransperthNetworking
Collaborative project



Each station is an instance of "station server" 
    Build a software (station server) and each bus/train station will be an instance (object) of this Sofware(class) 
    Maybe struct? Typedef 

 
Knows how to get from Vic Park to maddinton [could be direct or indirect]; Can query Vic park (source) webpage to find out buses and trains to take to get to maddinton 
    Vic park station --> needs to have knowledge of all station (incase routes are blocked or clogged)  
    Need adaptive algorithm (keeps changing current knowledge of network topology) 


One single webpage which has from (source) and to (destination) fields. Software queries the source server (instance), to possibly look at the adaptive algorithm table to find the quickest path to destination 


each bus or train station server only maintains the timetable information of buses and trains leaving that station, including the destination of each, and the (multiple) times throughout each day that each bus leaves the station and arrives at the destination. 
    DOUBT --> is the destination the FINAL destination of the bus/train, or the next stop 
    DOUBT --> is timetable info --> the stops the bus is going to make along the way 


if the source and destination stations are directly connected (via a single bus or train trip), then the source station server will be able to immediately respond to the query because it has all necessary information. 
    DOUBT --> Quickest? 


(Adaptive table --> built with flooding ------> and then uses that table to choose quicest path to dest?) 


if, however, the source and destination stations are not directly connected, the passenger will have to travel via two or more buses or trains, transferring at intermediate station(s). Because each station server only knows about buses and trains leaving that station, it will need to ask other stations' servers for information about the next segment (or hop) of the whole journey. 
    DOUBT --> potential updating of table using a time interval or account for the departure time and arrival time 

    
    INSERT PIC HERE FROM LANCE NOTEBOOK EXPLAINING TIMETABLE --> Along with how quickest path  

Weight of bus routes could be the current time + journey time/arrival time/time taken  


ADAPTIVE algo table keeps track of a soruce  node to the destination node; the quciekt time it takes to do that; and the path to do that 

 

Source  | Dest       | Total time | Path 
PERTH   | MADDINGTON | 20mins     |  PERTH -> VIC PARK -> CANNINGTON -> MADDINGTON 