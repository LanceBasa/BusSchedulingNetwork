
import sys
import os
import datetime




def main():
    filenm = sys.argv[1]
    # current time currently hard coded for testing purposes.
    # this will come as sys.argval[2]
    # also when current time is recieve, reset the seconds to 00 so it will also return the exact time match. 
    # e.g 10.03.55 will not match 10.03 in text file. the incomming time second should be rounded.
    currentTime = datetime.time(hour=10, minute=3, second=00)

    
    print(f"Current time is: {currentTime}")

    # createTime= time.ctime (os.path.getctime(filenm))
    modT= os.path.getmtime(filenm)

    #load the file name
    timetable=loadfile(filenm)
    

    # update file
    if (modT != os.path.getmtime(filenm)):
        print("Timetable Updated\n")
        timetable = update(filenm)

    #store the earliest timetable in a list
    paths = earliest(currentTime, timetable)

    # printing for testing purposes. This will show what it returns
    for entry in paths:
        print(entry)

    print("\n") 
    for entry in timetable:
        print(entry)
    return paths, timetable



def loadfile(filename):
    #list of dictionary timetable to return storing depart time, route name, depart from, arrival time, arrival station
    timetable=[]
    with open (filename) as fread:

        # header of timetables, can remove if not needed. if removed modify earliest function to include the very first entry
        for line in fread:
            if not line.startswith("#"):
                stationInfo= line.strip().split(',')
                timetable.append({
                    'StationName': stationInfo[0],
                    'Longitude': stationInfo[1],
                    'Latitude': stationInfo[2],
                })
                break

        # actual timetable data
        for line in fread:
            if not line.startswith("#") and line.strip():
                datarow = line.strip().split(',')
                timetable.append({
                    'departTime':datarow[0],
                    'routeName': datarow[1],
                    'departFrom': datarow[2],
                    'arriveTime': datarow[3],
                    'arriveAt': datarow[4]
                })
        return timetable

#check happens in main. if file has been modified. reload the file by rereading the function
def update(filename):    
    return loadfile(filename)

def earliest(curTime, timetable):
    
    unique_destinations = {}

    #The for loop will do the following
    #filter the times that are not the past
    #if the destination is not in the unique_destinations entry add it
    #otherwise check the uniq_destination entry and update if required (keeping earliest time)
    for entry in timetable[1:]:
        destination = entry['arriveAt']
        depart_time = entry['departTime']
        depart_time_obj = datetime.datetime.strptime(depart_time, '%H:%M').time()   #convert text time into datetime time
        if depart_time_obj >= curTime:     
            if destination not in unique_destinations:
                unique_destinations[destination] = entry
            else:
                if depart_time < unique_destinations[destination]['departTime']:
                    unique_destinations[destination] = entry
                                                              

    return list(unique_destinations.values()) #convert back to list



if __name__ == "__main__":
    main()