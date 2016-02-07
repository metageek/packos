#!/usr/bin/python

import sys

class Vendor:
    def __init__(self,id,name):
        self.id=id
        self.name=name
        self.devices={}

    def __repr__(self):
        return "{"+self.id+": "+self.name+": "+str(self.devices)+"}"

class Device:
    def __init__(self,id,name):
        self.id=id
        self.name=name
        self.isBridge=False

    def __repr__(self):
        return "{"+self.id+": "+self.name+": "+self.isBridge+"}"

class BaseClass:
    def __init__(self,id,name):
        self.id=id
        self.name=name
        self.subclasses={}

    def __repr__(self):
        return "{"+self.id+": "+self.name+": "+str(self.subclasses)+"}"

class SubClass:
    def __init__(self,id,name):
        self.id=id
        self.name=name
        self.interfaces={}

    def __repr__(self):
        return "{"+self.id+": "+self.name+": "+str(self.interfaces)+"}"

class Interface:
    def __init__(self,id,name):
        self.id=id
        self.name=name

    def __repr__(self):
        return "{"+self.id+": "+self.name+"}"

def escape(str):
    return '"'+str.replace("?","\?").replace('"','\\"')+'"'

vendors={}
curVendor=None
classes={}
curClass=None
curSubclass=None

for line in open("pci.ids").readlines():
    line=line.split("\n")[0]
    if line=="":
        continue
    if line[0]=='#':
        continue
    if line[0]=='C':
        curVendor=None
        baseClassId=line[2:4]
        baseClassName=line[4:].strip()
        curClass=BaseClass(baseClassId,baseClassName)
        classes[baseClassId]=curClass
        continue
    
    if line[0]=='\t':
        if curClass==None:
            if line[1]=='\t':
                continue
            deviceId=line[1:5]
            deviceName=line[5:].strip()
            curVendor.devices[deviceId]=Device(deviceId,deviceName)
        else:
            if line[1]=='\t':
                interfaceId=line[2:4]
                interfaceName=line[4:].strip()
                curSubclass.interfaces[interfaceId]=Interface(interfaceId,interfaceName)
            else:
                subclassId=line[1:3]
                subclassName=line[3:].strip()
                curSubclass=SubClass(subclassId,subclassName)
                curClass.subclasses[subclassId]=curSubclass
    else:
        if curClass==None:
            vendorId=line[0:4]
            vendorName=line[4:].strip()
            curVendor=Vendor(vendorId,vendorName)
            vendors[vendorId]=curVendor

for line in open("bridges.ids").readlines():
    line=line.split("\n")[0]
    (vendorId,deviceId)=line.split(" ")
    vendors[vendorId].devices[deviceId].isBridge=True

k=vendors.keys()
k.sort()
print "static PCIKnownVendor knownVendors[]="
print "  {"
for vendorId in k:
    print "    {","0x"+vendorId,",", escape(vendors[vendorId].name), "},"
print "  };"
print
print "static PCIKnownDevice knownDevices[]="
print "  {"
for vendorId in k:
    devices=vendors[vendorId].devices
    k2=devices.keys()
    k2.sort()
    for deviceId in k2:
        device=devices[deviceId]
        print "    {","0x"+vendorId,",","0x"+deviceId,",",escape(device.name), ",", str(device.isBridge).lower(), "},"
print "  };"
print
print "PCIBaseClass knownBaseClasses[]={"

k=classes.keys()
k.sort()
for baseClassId in k:
    baseClass=classes[baseClassId]
    print "  {", "0x"+baseClassId, ",", escape(baseClass.name), "},"

print "};"
print
print "PCISubClass knownSubClasses[]={"

for baseClassId in k:
    baseClass=classes[baseClassId]
    k2=baseClass.subclasses.keys()
    k2.sort()
    for subClassId in k2:
        subClass=baseClass.subclasses[subClassId]
        k3=subClass.interfaces.keys()
        k3.sort()
        for interfaceId in k3:
            interface=subClass.interfaces[interfaceId]
            print "  {", "0x"+baseClassId, ",", "0x"+subClassId, ",", "0x"+interfaceId,",", escape(subClass.name+" ["+interface.name+"]"), "},"
        print "  {", "0x"+baseClassId, ",", "0x"+subClassId, ", -1,", escape(subClass.name), "},"

print "};"

