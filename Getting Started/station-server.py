#Needs to start server from this .py
#Invokes this way: ./station-server.py [station name] [TCP port] [UDP port] [neighbour(s)]

#accept query from tcp port and bidirectional from UDP port 
#call timetable.py to get fastest times to reach my neighbours --> in the form of --> 
#---> (APPEND DESTINATION HERE BECAUSE WE GET THIS FROM HTML) Departure time, route name, departing from, arrival time, arrival station 
#
#./stn-server.py; timetable(tt-busportB, currenttime [which is given to us by html])
#./stn-server.py; dict timetable = timetable.py(timetablename, currenttime);
#
# if destination in timetable: return constructed query back to html
# else: send query to all neighbours 
# query format: [Destination] [station] [departure time] [arrival time] ... 