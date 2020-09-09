#!/usr/bin/python

from __future__ import absolute_import, print_function, unicode_literals

from optparse import OptionParser, make_option
import os
import sys
import socket
import uuid
import dbus
import dbus.service
import dbus.mainloop.glib
import copy
import time

try:
	from gi.repository import GObject
except ImportError:
	import gobject as GObject

import ctypes
from ctypes import *

class bluetooth(Structure):
	_fields_=[('status',c_int),('buf',c_char * 128)]

class Profile(dbus.service.Object):
	fd = -1

	@dbus.service.method("org.bluez.Profile1",
					in_signature="", out_signature="")
	def Release(self):
		print("Release")
		mainloop.quit()

	@dbus.service.method("org.bluez.Profile1",
					in_signature="", out_signature="")
	def Cancel(self):
		print("Cancel")

	@dbus.service.method("org.bluez.Profile1",
				in_signature="oha{sv}", out_signature="")
	def NewConnection(self, path, fd, properties):
		self.fd = fd.take()
		print("NewConnection(%s, %d)" % (path, self.fd))


		server_sock = socket.fromfd(self.fd, socket.AF_UNIX, socket.SOCK_STREAM)
		server_sock.setblocking(False)
		server_sock.send("This is qbox10-cmr device\n")
		func.bluetooth_status_set(1)
		while True:
			time.sleep(0.02)
			#print("Bluetooth connect\n")
			recv_data = create_string_buffer(10240)
			ret = func.bluetooth_proxy_read(recv_data)
			if ret: 
				#print("ret is:%d\n" % ret)
				#print(recv_data.raw)
				server_sock.send(recv_data[0:ret])
			
			try:
				data = server_sock.recv(10240)
			#except IOError:
				#print("data len0:%d\n" % len(data))
				#if len(data) < 0:
				#	print("Bluetooth connect IOError")
				#	break;
				#pass
			except socket.error as e:
				#print("Bluetooth connect error:%s" % e)
				#print("error id:%d" % e.errno)
				if (e.errno == 104):
					break
				pass
			else:
				#if not len(data) : break;
				#print(data)
				#print("data len:%d\n" % len(data))
				#server_sock.send(data)  
				func.bluetooth_proxy_write(data, len(data))
			 
		server_sock.close()
		print("all done")
		func.bluetooth_status_set(0)


	@dbus.service.method("org.bluez.Profile1",
				in_signature="o", out_signature="")
	def RequestDisconnection(self, path):
		print("RequestDisconnection(%s)" % (path))

		if (self.fd > 0):
			os.close(self.fd)
			self.fd = -1

if __name__ == '__main__':

	func = ctypes.cdll.LoadLibrary("/usr/local/qbox10/bluetooth/libbluetooth_proxy.so")
	func.bluetooth_proxy_init()
	#s = bluetooth()
	#s.status = 555
	#s.buf = bytes('hello,world')
	#func.bluetooth_proxy_cb(s)		
	dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
	
	bus = dbus.SystemBus()
	#a = "123456789"
	#func.bluetooth_print(a, len(a))
	#func.bluetooth_print(byref("123456789"), len("123456789"))
	manager = dbus.Interface(bus.get_object("org.bluez",
				"/org/bluez"), "org.bluez.ProfileManager1")

	option_list = [
			make_option("-C", "--channel", action="store",
					type="int", dest="channel",
					default=None),
			]
	
	parser = OptionParser(option_list=option_list)

	(options, args) = parser.parse_args()

	options.uuid = "1101"
	options.psm = "3"
	options.role = "server"
	options.name = "qbox10 SPP Loopback"
	options.service = "spp char loopback"
	options.path = "/foo/bar/profile"
	options.auto_connect = False
	options.record = ""

	profile = Profile(bus, options.path)

	mainloop = GObject.MainLoop()

	opts = {
			"AutoConnect" :	options.auto_connect,
		}

	if (options.name):
		opts["Name"] = options.name

	if (options.role):
		opts["Role"] = options.role

	if (options.psm is not None):
		opts["PSM"] = dbus.UInt16(options.psm)

	if (options.channel is not None):
		opts["Channel"] = dbus.UInt16(options.channel)

	if (options.record):
		opts["ServiceRecord"] = options.record

	if (options.service):
		opts["Service"] = options.service

	if not options.uuid:
		options.uuid = str(uuid.uuid4())

	manager.RegisterProfile(options.path, options.uuid, opts)

	mainloop.run()

