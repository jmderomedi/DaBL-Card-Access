# normal pip install won't work pq Visual Studio can't compile it properly, so!:
# install mysql-python as a wheel from i can't remember where (i have these downloaded)

import MySQLdb as db
import serial
import sys

# ---------------------------------------------------------------------------------------------------------------
# sql functions
# ---------------------------------------------------------------------------------------------------------------

def member_create(columns, data):
    dbhandler.execute("INSERT INTO `users` (columns) VALUES (data);")
    # dbhandler.execute("INSERT INTO `users`(`name`, `UID`, `membersince`, `lastvisit`, `waiver`, `series1`, \
    #                 `uprint`, `bantam`, `pls475`) VALUES ([value-1],[value-2],[value-3],[value-4],[value-5],\
    #                 [value-6],[value-7],[value-8],[value-9])");

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

def disconnect(conn):
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
    cur.execute("SELECT {cn} FROM {tn} WHERE UID='{uid}'"\
        .format(tn=table_name, cn=column_name, uid=user_id))
    thing = cur.fetchall()
    print thing
    return thing

def set_data(cur, table_name, column_name, data_value):
    cur.execute("INSERT INTO {tn} ({cn}) VALUES ({dv})"\
        .format(tn=table_name, cn=column_name, dv=data_value))

# ---------------------------------------------------------------------------------------------------------------
# teensy-specific functions
# ---------------------------------------------------------------------------------------------------------------

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

# ---------------------------------------------------------------------------------------------------------------
# globals
# ---------------------------------------------------------------------------------------------------------------
HOST = "localhost"
PORT = 3306
USER = "root"
PASSWORD = ""
DB = "DaBL"

# ---------------------------------------------------------------------------------------------------------------
# everything else
# ---------------------------------------------------------------------------------------------------------------

# def main():

port = 'COM10' # HARDWIRED RIGHT NOW
value = ""
print "start"
ports = serial_find()
if ports == []:
    print 'no ports found'
    quit()
else:
    # port = ports[-1]
    teensy = serial.Serial(port, 115200)
    print 'connected to', port
    while True:
        value = teensy.readline()
        if value[-1] == "\n": # is the last character a newline?
            msg = value.rstrip()
            print 'received:', msg
            if (str(msg) == str("Are you there, python?")):
                msg = "I am here, teensy!\n"
                print 'sending:', msg
                teensy.write(msg)
            else:
                #send information
                table_name = 'users'
                column_name = 'name'
                user_id = msg
                connection, cursor = connect()
                # print "get all",
                # for item in get_all(cursor, table_name):
                #     print item
                # print "get column names",
                # for item in get_column_names(cursor, table_name):
                #     print item,
                access_request = 'waiver'
                print "\nget access",
                result = get_access(cursor, table_name, access_request, user_id)
                if (result):
                    for item in result:
                        item = item[0]
                        if item == 1:
                            print "-- RESULT: user approved"
                            teensy.write("RESULT: user approved\n")
                        elif item == 0:
                            print "-- RESULT: user rejected"
                            teensy.write("RESULT: user rejected\n")
                else:
                    print "-- RESULT: user not found"
                    teensy.write("RESULT: user not found\n")
                disconnect(connection)
        else:
            continue



# if __name__ == "__main__":
#     main()


# if (input_string == "RESULT: user approved") {              // green
#                 blink_led(green, 3);
#             } else if (input_string == "RESULT: user rejected") {       // red
#                 blink_led(red, 3);
#             } else if (input_string == "RESULT: user not found") {      // yellow
#                 blink_led(yellow, 2);
#             } else if (input_string == "RESULT: db not accessible") {   // magenta
#                 blink_led(magenta, 2);
#             }