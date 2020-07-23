from bottle import route, run, template, request
import pyodbc
import json
import decimal

def decimal_default(obj):
    if isinstance(obj, decimal.Decimal):
        return float(obj)
    raise TypeError

@route('/')
def index():
    print("/ ......\n");
    cnxn = pyodbc.connect("DRIVER={PostgreSQL Unicode};SERVER=localhost;USERNAME=sa;PASSWORD=sa;PORT=5435;DATABASE=test;Trusted_Connection=yes")

    cursor = cnxn.cursor()

    cursor.execute("select * from sensor_data")
    columns = [column[0] for column in cursor.description]
    result = []
    data = cursor.fetchall()
    for row in data:
        result.append(dict(zip(columns, row)))

    cursor.close()
    cnxn.close()
    result = json.dumps(result)
    
    return result

@route('/query/<sensortype>')
def submit(sensortype):
    print("/query/ ......\n");
    cnxn = pyodbc.connect("DRIVER={PostgreSQL Unicode};SERVER=localhost;USERNAME=sa;PASSWORD=sa;PORT=5435;DATABASE=test;Trusted_Connection=yes")

    cursor = cnxn.cursor()

    # print(bottle.json)
    cursor.execute("select * from sensor_data where sensortype is ?", sensortype)
    columns = [column[0] for column in cursor.description]
    result = []
    data = cursor.fetchall()
    for row in data:
        result.append(dict(zip(columns, row)))

    cursor.close()
    cnxn.close()
    result = json.dumps(result)
    
    return result


@route('/query', method='POST')
def submit():
	print("POST getdata....\n")
	cnxn = pyodbc.connect("DRIVER={PostgreSQL Unicode};SERVER=localhost;USERNAME=sa;PASSWORD=sa;PORT=5435;DATABASE=test;Trusted_Connection=yes")
	
	cursor = cnxn.cursor()
	
	print(request.json)
	data = request.json
	print("{:s}\n".format(data['cmd']));
	
	cursor.execute(data['cmd'])
	
	columns = [column[0] for column in cursor.description]
	result = []
	data = cursor.fetchall()
	for row in data:
		result.append(dict(zip(columns, row)))
	
	cursor.close()
	cnxn.close()
	result = json.dumps(result, default=decimal_default)
	print(result);
	return result

@route('/submitjson', method='POST')
def submit():
    print("POST cmd....\n")
    cnxn = pyodbc.connect("DRIVER={PostgreSQL Unicode};SERVER=localhost;USERNAME=sa;PASSWORD=sa;PORT=5435;DATABASE=test;Trusted_Connection=yes")

    cursor = cnxn.cursor()

    print(request.json)
    #data = json.loads(request.json)
    data = request.json
    # print(data['id'])
    # print(data.id)
    print("{:2d}, {:2d}, {:2d}, {:2d}, {:2f}, {:2f}, {:2f}, {:2f}, {:2f}, {:2f}\n".format( data['sensortype'], data['sensorid'], data['objectid'], data['objecttype'], data['x'], data['y'], data['x_vel'], data['y_vel'], data['x_next'], data['y_next']))

    cursor.execute("insert into leddar_data (sensortype, sensorid, objectid, objecttype, x, y, x_vel, y_vel, x_next, y_next) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", data['sensortype'], data['sensorid'], data['objectid'], data['objecttype'], data['x'], data['y'], data['x_vel'], data['y_vel'], data['x_next'], data['y_next'])
    ##cursor.execute("insert into data_tb (id, name) values (?, ?)", data['id'], data['name'])
    cnxn.commit()

    # columns = [column[0] for column in cursor.description]
    result = []
    # data = cursor.fetchall()
    # # data = list(cursor)
    # for row in data:
    #     result.append(dict(zip(columns, row)))

    cursor.close()
    cnxn.close()
    # result = json.dumps(result)
    # print(result)
    return result

run(server='tornado', host='localhost', port=8081)
