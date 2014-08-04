from mininet.net import Mininet
from mininet.node import Controller, RemoteController, UserSwitch, Node
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.link import Link, TCLink
from mininet.util import quietRun
from mininet.topo import Topo
from functools import partial
import time
import os

si = 1
hi = 1


class DatacenterTopo2(Topo):
        def __init__(self, **opts):
                Topo.__init__(self, **opts)
		
		linkopts10 = dict(bw=10, delay='0ms', loss=0)
		linkopts20 = dict(bw=20, delay='0ms', loss=0)
		
		h1 = self.addHost("h1")
		h2 = self.addHost("h2")

		s1 = self.addSwitch("s1", **dict(listenPort=13001))
		s2 = self.addSwitch("s2", **dict(listenPort=13002))
		#s3 = self.addSwitch("s3", **dict(listenPort=13003))
		
		self.addLink(h1, s1, **linkopts20)
		self.addLink(s1, s2, **linkopts10)
		#self.addLink(s2, s3, **linkopts10)
		self.addLink(h2, s2, **linkopts20)
		

class DatacenterTopo(Topo):
	def __init__(self, **opts):
		Topo.__init__(self, **opts)

		print 'creating core switches...'
		def getS() :
			global si
			si = si +1
			return 's%d' % (si-1)
	
		def getH() : 
			global hi
			hi = hi +1
			return 'h%d' % (hi-1)

		linkopts10 = dict(bw=10, delay='0ms', loss=0)
		linkopts20 = dict(bw=20, delay='0ms', loss=0)

		#add Core Switches:
		r1 = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
		r2 = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
		r3 = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
		r4 = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
		self.addLink(r2, r3, **linkopts10)
		self.addLink(r1, r4, **linkopts10)


		print 'creating ToR switches...'

		#create 4 rows of ToR switches:
		for row in range(4):
			aA = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
			aB = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))

			print 'row=' + str(row)

			#add ToR switches:
			for s in range(4):
				print 's=' + str(s)
				sw = self.addSwitch(getS(), **dict(listenPort=(13000+si-1)))
				host = self.addHost(getH())
				print 'creating links..'
				self.addLink(sw, aA, **linkopts10)
				self.addLink(sw, aB, **linkopts10)
				self.addLink(host, sw, **linkopts20)
				#host.setIP('10.' + str(row) + '.' + str(s) + '.' + '1/24')

				print 'done link creation'

	
			#add links to core switches:
			if row in range(2):
				self.addLink(aA, r1, **linkopts10)
				self.addLink(aA, r2, **linkopts10)
				self.addLink(aB, r1, **linkopts10)
				self.addLink(aB, r2, **linkopts10)
			else:
				self.addLink(aA, r3, **linkopts10)
				self.addLink(aA, r4, **linkopts10)
				self.addLink(aB, r3, **linkopts10)
				self.addLink(aB, r4, **linkopts10)
			

def startExperiment():
	print 'starting up..'
	topo = DatacenterTopo()
	net = Mininet(topo=topo, switch=UserSwitch, link=TCLink, controller=partial(RemoteController, ip='kermit.cs.upb.de'))
	#net = Mininet(topo=topo, link=TCLink, controller=partial(RemoteController, ip='192.168.56.1'))


	trafficServer = "/home/wette/trafficGen/server/server"
	trafficGen = "/home/wette/trafficGen/trafficGenerator/trafficGenerator/traffGen"

	flowFile = "/home/wette/trafficGen/flows16Racks20Hosts.csv"
	scaleFactorSize = "0.01"
	scaleFactorTime = "10"

	print 'set up individual hosts'
	hostsPerRack = 20
	racks = 16


	#set up the ip addresses of the hosts
	for i in range(1,racks+1):
		name = 'h' + str(i)
		node = net.getNodeByName(name)
		
		#set base ip
		node.setIP("10.0." + str(i) + ".1/8")
		#set the rest:
		dev = name + "-eth0"

		for j in range(2,hostsPerRack+1):
			addr = "10.0." + str(i) + "." + str(j) + "/8"
			node.cmd("ip addr add " + addr + " dev " + dev)
		
		


	net.start()

	os.system("killall server plotEthHelper traffGen 2>&1 > /dev/null")
	print 'sleeping for 10 seconds...'
	time.sleep(10)

	os.system("./../ethMonitor.sh &")

	print 'starting traffic generation...'
	for i in range(1,racks+1):
		name = 'h' + str(i)
		node = net.getNodeByName(name)
		for j in range(1,hostsPerRack+1):
			node.cmd(trafficServer + " 10.0." + str(i) + "." + str(j) + " > /home/wette/logs/10.0." + str(i) + "." + str(j) + ".log &")

	
	for i in range(1,racks+1):
		name = 'h' + str(i)
		node = net.getNodeByName(name)
		startid = (i-1) * hostsPerRack
		endid = i * hostsPerRack
		node.cmd(trafficGen + " " + str(hostsPerRack) + " 10.0 " + str(startid) + " " + str(endid) + " " + flowFile + " " + scaleFactorSize + " " + scaleFactorTime + " 1>&2 > /tmp/" +name+"TG.log &")


	time.sleep(630)

	os.system("killall server plotEthHelper traffGen 2>&1 > /dev/null")

	net.stop()




if __name__ == '__main__':
	startExperiment()
