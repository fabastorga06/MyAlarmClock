from tkinter import *
from tkinter import messagebox
import serial

serial_port = serial.Serial('COM3', 9600, timeout=1)

def set_time():
    if (int(e_time_h.get()) < 24 and int(e_time_min.get()) < 60):
        time_msg = "st" + e_time_h.get() + e_time_min.get() + "fx"
        print (time_msg)
        time_as_bytes = str.encode(time_msg)
        serial_port.write( time_as_bytes )
    else:
        messagebox.showerror("Error Time", "Invalid clock time, try again")

def set_alarm():
    if (int(e_alarm_h.get()) < 24 and int(e_alarm_min.get()) < 60):
        alarm_msg = "sa" + e_alarm_h.get() + e_alarm_min.get() + "fx"
        print (alarm_msg)
        alarm_as_bytes = str.encode(alarm_msg)
        serial_port.write( alarm_as_bytes )
    else:
        messagebox.showerror("Error Alarm", "Invalid clock alarm, try again")

master = Tk()
master.title("Clock")

Label(master, text="Set Time").grid(row=0,column=0)
Label(master, text="Set Alarm").grid(row=1,column=0)

e_time_h = Entry(master)
e_time_min = Entry(master) 
e_alarm_h = Entry(master) 
e_alarm_min = Entry(master) 

e_time_h.grid(row=0, column=1)
e_time_min.grid(row=0, column=2)
e_alarm_h.grid(row=1, column=1)
e_alarm_min.grid(row=1, column=2)

Button(master, text='Set Time', command=set_time).grid(row=0, column=3, sticky=W, pady=4)
Button(master, text='Set Alarm', command=set_alarm).grid(row=1, column=3, sticky=W, pady=4)

mainloop( )