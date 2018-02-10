# normal pip install won't work pq Visual Studio can't compile it properly, so!:
# install mysql-python as a wheel from i can't remember where (i have these downloaded)

import MySQLdb as db
import serial
import sys

HOST = "localhost"
PORT = 3306
USER = "root"
PASSWORD = ""
DB = "test"

def member_create(columns, data):
    dbhandler.execute("INSERT INTO `users` (columns) VALUES (data);")

def get_columns():
    # example: (`name`, `ID`, `type a`, `bantam`)
    columns = ""
    return columns

def connect():
    try:
        conn = db.Connection(host=HOST,
                             port=PORT,
                             user=USER,
                             passwd=PASSWORD,
                             db=DB)

        cur = conn.cursor()
        return conn, cur
    except Exception as e:
        print e

def close(conn):
    conn.close()

def get_column_names(cur, table_name):
    cur.execute("DESCRIBE {tn}"\
        .format(tn=table_name))
    return [tup[0] for tup in cur.fetchall()]

def get_all(cur, table_name):
    cur.execute("SELECT * FROM {tn}"\
        .format(tn=table_name))
    return cur.fetchall()

def get_access(cur, table_name, column_name, user_id):
    cur.execute("SELECT {cn} FROM {tn} WHERE ID={uid}"\
        .format(tn=table_name, cn=column_name, uid=user_id))
    return cur.fetchall()

def set_data(cur, table_name, column_name, data_value):
    cur.execute("INSERT INTO {tn} ({cn}) VALUES ({dv})"\
        .format(tn=table_name, cn=column_name, dv=data_value))

# ----------------------------------------------------------------------------------------------------------------------------
def serial_find():
    ports = ['COM%s' % (i + 1) for i in range(256)]
    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
        except IOError: # if port is already opened, close it and open it again and print message // will this work?
            s.close()
            s.open()
            s.close()
            print ("port was already open, was closed and opened again!")
    return result

# ----------------------------------------------------------------------------------------------------------------------------
# def main():
port = ''
value = ""
print "start"
ports = serial_find()
count = 0
if ports == []:
    print 'no ports found'
    quit()
else:
    # port = ports[-1]
    port = 'COM10'
    teensy = serial.Serial(port, 115200)
    print 'connected to', port
    while True:
        value = teensy.readline()
        if value[-1] == "\n": # is the last character a newline?
            msg = value.rstrip()
            if (str(msg) == str("Are you there, python?")):
                print 'received:', msg
                msg = "I am here, teensy!\n"
                print 'sending:', msg
                teensy.write(msg)
            # else:
            #     a = 0
                #send information
                # table_name = 'users'
                # column_name = 'name'
                # connection, cursor = connect()
                # print "get all",
                # for item in get_all(cursor, table_name):
                #     print item
                # print "get column names",
                # for item in get_column_names(cursor, table_name):
                #     print item,
                # print "\nget access",
                # machine = 'typea'
                # user_id = "'e14caf2af799f271f2729ba77266acbf'"
                # for item in get_access(cursor, table_name, machine, user_id):
                #     print item,
                # print ""
                # close(connection)
        else:
            continue



# if __name__ == "__main__":
#     main()