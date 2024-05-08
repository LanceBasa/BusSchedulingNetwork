# ./station-server JunctionA 4001 4002 localhost:4010 localhost:4012 &
# ./station-server BusportB 4003 4004 localhost:4006 localhost:4010 &
# ./station-server StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 &
# ./station-server TerminalD 4007 4008 localhost:4006 &
# ./station-server JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 &
# ./station-server BusportF 4011 4012 localhost:4002 &


python3 ./station-server.py BusportB 4003 4004 localhost:4006 localhost:4010 &
python3 ./station-server.py StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 &
python3 ./station-server.py JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006&
python3 ./station-server.py JunctionA 4001 4002 localhost:4010 localhost:4012 &
python3 ./station-server.py TerminalD 4007 4008 localhost:4006 &
python3 ./station-server.py BusportF 4011 4012 localhost:4002 &
