=======================================
Comments for User to Run Leddar System
=======================================
Prerequest:
	i)	In H2 database "Leddar_table" must be present. Or use followint SQL command to create one.
		"CREATE TABLE LEDDAR_DATA(ID INT AUTO_INCREMENT PRIMARY KEY, SENSORTYPE INT, SENSORID INT, OBJECTID INT, OBJECTTYPE INT, X DECIMAL, Y DECIMAL, X_VEL DECIMAL, Y_VEL DECIMAL,  X_NEXT DECIMAL, Y_NEXT DECIMAL);"

Steps to Run:
	i)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	ii) 	Run "python server_8080.py"
	iii)	Run "python server_8081.py"
	vi) 	Run "./IS16read" to start reading leddar data and saving it to DB
	v) 	Run "./display" to read data from DB and use it for visiualization

------------------------------------------
Comments for Developer - Debugging the Code
------------------------------------------

1. Steps to test leddar algorithum with real time data with loca visiualization:
	i) 	Enable local visiualization function in leddar algorithum by set "localvisualizationenable" in dev_config.json
	ii) 	define "testwithfile" as 0 in leddar_v4_0.cpp
	iii) 	Compile project with "make IS16read" command.
	v)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	vi) 	Run "python server_8080.py"
	vii) 	Run "./IS16read"

2. Steps to test leddar algorithum with real time data and display interface visiualization:
	i) 	Disable local visiualization function in leddar algorithum by resetting "localvisualizationenable" in dev_config.json.
	ii)	define "testwithfile" as 0 in leddar_v4_0.cpp
	iii) 	Compile project with "make IS16read" command
	iv) 	Compile project with "make display" command
	v)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	vi) 	Run "python server_8080.py"
	vii)	Run "python server_8081.py"
	viii) 	Run "./IS16read"
	ix) 	Run "./display"

3. Steps to test leddar algorithum using raw data file with local visiualization:
	i) 	Enable visiualization function in leddar algorithum by set "localvisualizationenable" in dev_config.json
	ii) 	define "testwithfile" as 1 in leddar_v4_0.cpp
	iii) 	Compile project with "make IS16read_test" command.
	v)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	vi) 	Run "python server_8080.py"
	vii) 	Run "./IS16read_test raw_data.txt"
NOTE: This is the best way to test algorithum

4. Steps to test leddar algorithum using raw data file and display interface visiualization:
	i) 	Disable local visiualization function in leddar algorithum by resetting "localvisualizationenable" in dev_config.json.
	ii) 	define "testwithfile" as 1 in leddar_v4_0.cpp
	iii) 	Compile project with "make IS16read_test" command
	iv) 	Compile project with "make display" command
	v)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	vi) 	Run "python server_8080.py"
	vii)	Run "python server_8081.py"
	viii) 	Run "./IS16read_test raw_data.txt"
	ix) 	Run "./display"

4. Steps to only test visiualization:
	i) 	To modify display configuration, change parameters into dev_config.json and user_config.json
	ii) 	Compile project with "make display" command
	iii)	Start H2 server with "java -cp h2*.jar org.h2.tools.Server -baseDir ~ -pg" command from "~/h2/bin" folder
	vi) 	Run "python server_8081.py"	
	vii) 	Run ./display

------
Files
------

1) Leddar algorithum 
leddar_v4_0.cpp
leddar4.h

2) Visiualization 
display.cpp
display.h

3) DB managment
dbclient.cpp
dbclient.h

4) Sample data
server_8080.py
server_8081.py
raw_data.txt
testdata.txt

5) Configuration File
dev_config.json
user_config.json

5) Makefile

--------------------
Package installation 
--------------------

JsonCpp
-------
1. Follow following command for package installation for cpp Json:
	sudo apt-get install libjsoncpp-dev
2. For compilation use "-ljsoncpp" flag

Jansson
-------
1. Follow following command for package installation for cpp Json:
	sudo apt-get update
	sudo apt-get install libjansson-dev
2. For compilation use "-ljansson" flag
	
