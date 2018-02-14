# normal pip install won't work pq Visual Studio can't compile it properly, so!:
# install mysql-python as a wheel from i can't remember where (i have these downloaded)

import MySQLdb as db
import serial
import sys
import datetime

# ---------------------------------------------------------------------------------------------------------------
# sql functions
# ---------------------------------------------------------------------------------------------------------------

def db_connect():
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

def db_disconnect(conn):
    conn.close()

#  do not use
def member_create(columns, data):
    dbhandler.execute("INSERT INTO `users` (columns) VALUES (data);")
    # dbhandler.execute("INSERT INTO `users`(`name`, `UID`, `membersince`, `lastvisit`, `waiver`, `series1`, \
    #                 `uprint`, `bantam`, `pls475`) VALUES ([value-1],[value-2],[value-3],[value-4],[value-5],\
    #                 [value-6],[value-7],[value-8],[value-9])");

def get_column_names(cur, table_name):
    cur.execute("DESCRIBE {tn}"\
        .format(tn=table_name))
    return [tup[0] for tup in cur.fetchall()]

def get_all(cur, table_name):
    cur.execute("SELECT * FROM {tn}"\
        .format(tn=table_name))
    return cur.fetchall()

def get_user(cur, table_name, user_id):
    cur.execute("SELECT * FROM {tn} WHERE UID='{uid}'"\
        .format(tn=table_name, uid=user_id))
    return cur.fetchall()

def get_access(cur, table_name, column_name, user_id):
    cur.execute("SELECT {cn} FROM {tn} WHERE UID='{uid}'"\
        .format(tn=table_name, cn=column_name, uid=user_id))
    return cur.fetchall()

def set_access(cur, table_name, column_name, user_id, column_access):
    cur.execute("UPDATE {tn} SET {cn}={cv} WHERE UID='{uid}'"\
        .format(tn=table_name, cn=column_name, uid=user_id))

def insert_swiped_user(cur, table_name1, table_name2, user_name, user_id, dt_date, dt_time):
    cur.execute("UPDATE `{tn1}` SET `lastaccessdate`='{dtd}', `lastaccesstime`='{dtt}' WHERE `UID`='{uid}'"\
        .format(tn1=table_name1, dtd=dt_date, dtt=dt_time, uid=user_id))
    cur.execute("INSERT INTO `{tn2}` (`name`, `UID`, `accessdate`, `accesstime`) VALUES ('{un}','{uid}','{dtd}','{dtt}')"\
        .format(tn2=table_name2, un=user_name, uid=user_id, dtd=dt_date, dtt=dt_time))

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
HOST = "10.60.191.10"
PORT = 3306
USER = "root"
PASSWORD = ""
DB = "DaBL"

# ---------------------------------------------------------------------------------------------------------------
# everything else
# ---------------------------------------------------------------------------------------------------------------

# def main():
value = ""
print "#################################################"
print "#      DaBL front desk swipe access reader      #"
ports = serial_find()
if ports == []:
    print "#               no ports found                 #"
    quit()
else:
    # port = ports[-1]
    port = 'COM3' # HARDWIRED RIGHT NOW
    teensy = serial.Serial(port, 115200)
    print "#           Teensy connected to", port, "           #"
    print "#################################################\n"
    while True:
        value = teensy.readline()
        if value[-1] == "\n": # is the last character a newline?
            msg = value.rstrip()
            print "received UID:\t\t", msg
            if (str(msg) == str("Are you there, python?")):
                msg = "I am here, teensy!\n"
                print "sending:", msg
                teensy.write(msg)
            # send information
            else:
                user_table = 'users'
                user_id = msg
                print "-- member name:\t\t",
                dt = datetime.datetime.now()
                dt_date = dt.strftime("%Y/%m/%d")
                dt_time = dt.strftime("%H:%M:%S")
                connection, cursor = db_connect()
                db_result = get_user(cursor, user_table, user_id)
                if (db_result):
                    db_result = db_result[0]
                    user_name = db_result[0]
                    access_request = 'waiver'
                    print user_name
                    print "-- safety waiver:\t",
                    db_result = get_access(cursor, user_table, access_request, user_id)
                    swipe_table = 'accesslogs'
                    try:
                        insert_swiped_user(cursor, user_table, swipe_table, user_name, user_id, dt_date, dt_time)
                        connection.commit()
                    except:
                        connection.rollback()
                    for item in db_result:
                        item = item[0]
                        if item == 1:
                            print "user approved"
                            teensy.write("RESULT: user approved\n")

                        elif item == 0:
                            print "USER REJECTED, NO SAFETY WAIVER"
                            teensy.write("RESULT: user rejected\n")
                else:
                    print "USER NOT FOUND"
                    teensy.write("RESULT: user not found\n")
                db_disconnect(connection)
                print "-------------------------------------------------"
        else:
            continue

# if __name__ == "__main__":
#     main()