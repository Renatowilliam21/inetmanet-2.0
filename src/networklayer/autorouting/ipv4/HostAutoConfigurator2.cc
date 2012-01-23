/*
* HostAutoConfigurator - automatically assigns IP addresses and sets up routing table
* Copyright (C) 2009 Christoph Sommer <christoph.sommer@informatik.uni-erlangen.de>
* Copyright (C) 2010 Alfonso Ariza
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <stdexcept>
#include <algorithm>

#include "HostAutoConfigurator2.h"

#include "IPvXAddressResolver.h"
#include "IPv4InterfaceData.h"
#include "IRoutingTable.h"
#include "IInterfaceTable.h"
#include "IPv4Address.h"

Define_Module(HostAutoConfigurator2);

static IPv4Address defaultAddr;

HostAutoConfigurator2::HostAutoConfigurator2()
{
    defaultAddr.set(0,0,0,0);
}

void HostAutoConfigurator2::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == 0)
    {
        debug = par("debug").boolValue();
    }
    else if (stage == 2)
    {
        setupNetworkLayer();
        if (par("isDefaultRoute"))
        {
            if (!defaultAddr.isUnspecified())
                opp_error("default router is defined yet");
            cModule* host = getParentModule();
            IInterfaceTable *ift = IPvXAddressResolver().interfaceTableOf(host);
            InterfaceEntry* ie = ift->getInterfaceByName(par("defaultAddressInterface"));
            if (ie==NULL)
                opp_error("default ID interface doesn't exist");
            defaultAddr=ie->ipv4Data()->getIPAddress();
        }
    }
    else if (stage == 3)
    {
        setupRoutingTable();
    }
}

void HostAutoConfigurator2::finish()
{
}

void HostAutoConfigurator2::handleMessage(cMessage* apMsg)
{
}

void HostAutoConfigurator2::handleSelfMsg(cMessage* apMsg)
{
}
namespace
{

void addToMcastGroup(InterfaceEntry* ie, IRoutingTable* routingTable, const IPv4Address& mcastGroup)
{
    IPv4InterfaceData::IPAddressVector mcg = ie->ipv4Data()->getMulticastGroups();
    if (std::find(mcg.begin(), mcg.end(), mcastGroup) == mcg.end()) mcg.push_back(mcastGroup);
    ie->ipv4Data()->setMulticastGroups(mcg);

    IPv4Route* re = new IPv4Route(); //TODO: add @c delete to destructor
    re->setDestination(mcastGroup);
    re->setNetmask(IPv4Address::ALLONES_ADDRESS); // TODO: can't set this to none?
    re->setGateway(IPv4Address()); // none
    re->setInterface(ie);
    re->setType(IPv4Route::DIRECT);
    re->setSource(IPv4Route::MANUAL);
    re->setMetric(1);
    routingTable->addRoute(re);
}

void addRoute(InterfaceEntry* ie, IRoutingTable* routingTable, const IPv4Address& ipaddress,const IPv4Address& maskaddress)
{
    //
	//
	IPv4Route* re = new IPv4Route(); //TODO: add @c delete to destructor
    re->setDestination(ipaddress);
    re->setNetmask(maskaddress); // TODO: can't set this to none?
    re->setGateway(IPv4Address::UNSPECIFIED_ADDRESS); // none
    re->setInterface(ie);
    re->setType(IPv4Route::DIRECT);
    re->setSource(IPv4Route::MANUAL);
    re->setMetric(1);
    routingTable->addRoute(re);
}
}




void HostAutoConfigurator2::addDefaultRoutes()
{
    // add default route to nodes with exactly one (non-loopback) interface
    // get our interface table
    // get our host module
    cModule* host = getParentModule();
    if (!host) throw std::runtime_error("No parent module found");
    IInterfaceTable *ift = IPvXAddressResolver().interfaceTableOf(host);
    IRoutingTable* rt = IPvXAddressResolver().routingTableOf(host);

    // count non-loopback interfaces
    int numIntf = 0;
    InterfaceEntry *ie = NULL;
    for (int k=0; k<ift->getNumInterfaces(); k++)
        if (!ift->getInterface(k)->isLoopback())
            {ie = ift->getInterface(k); numIntf++;}

    if (numIntf!=1)
        return; // only deal with nodes with one interface plus loopback
    int i=0;
    while (i<rt->getNumRoutes())
    {
        if (rt->getRoute(i)->getInterface()==ie)
    	{
            rt->deleteRoute(rt->getRoute(i));
            i=0;
    	}
    	else
    	    i++;
    }
    IPv4Route *e = new IPv4Route();
    e->setDestination(IPv4Address());
    e->setNetmask(IPv4Address());
    e->setInterface(ie);
    e->setType(IPv4Route::REMOTE);
    e->setSource(IPv4Route::MANUAL);
     //e->getMetric() = 1;
    rt->addRoute(e);
}


void HostAutoConfigurator2::setupNetworkLayer()
{
    EV << "host auto configuration started" << std::endl;

    std::string interfaces = par("interfaces").stringValue();
    std::string addressBaseToken = par("addressBaseList").stringValue();
    std::string addressMask = par("addressMask").stringValue();

    cStringTokenizer interfaceTokenizer(interfaces.c_str());
    cStringTokenizer addressBaseTokenizer(addressBaseToken.c_str());
    cStringTokenizer addressMaskTokenizer(addressMask.c_str());


    //IPv4Address addressBase = IPv4Address(par("addressBase").stringValue());
    //std::string mcastGroups = par("mcastGroups").stringValue();


    // get our host module
    cModule* host = getParentModule();
    if (!host) throw std::runtime_error("No parent module found");

    // get our routing table
    IRoutingTable* routingTable = IPvXAddressResolver().routingTableOf(host);
    if (!routingTable) throw std::runtime_error("No routing table found");

    // get our interface table
    IInterfaceTable *ift = IPvXAddressResolver().interfaceTableOf(host);
    if (!ift) throw std::runtime_error("No interface table found");

    IPv4Address myAddress;
    IPv4Address netmask;
    // look at all interface table entries
    const char *ifname;
    std::vector<std::string> vectorInterfaceToken;
    std::vector<std::string> vectorAddressToken;
    std::vector<std::string> vectorMaskToken;
    while((ifname = interfaceTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorInterfaceToken.push_back(val);
    }

    while((ifname = addressBaseTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorAddressToken.push_back(val);
    }

    while((ifname = addressMaskTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorMaskToken.push_back(val);
    }

    if (vectorAddressToken.size()>1 && (vectorAddressToken.size()!=vectorInterfaceToken.size()))
    	opp_error("interfaces and addressBaseList has different sizes");

    if (vectorAddressToken.size()!=vectorMaskToken.size())
    	opp_error("addressMask and addressBaseList has different sizes");


    bool interfaceFound = false;
    for (unsigned int i=0;i<vectorInterfaceToken.size();i++)
    {
    	ifname = vectorInterfaceToken[i].c_str();
        InterfaceEntry* ie = ift->getInterfaceByName(ifname);

        if (!ie)
            continue;

        // assign IP Address to all connected interfaces
        if (ie->isLoopback())
        {
            EV << "interface " << ifname << " skipped (is loopback)" << std::endl;
            continue;
        }
        interfaceFound=true;
        if (vectorAddressToken.size()>1)
        {
            IPv4Address addressBase(vectorAddressToken[i].c_str());
            myAddress = IPv4Address(addressBase.getInt() + uint32(getParentModule()->getId()));
            netmask = IPv4Address(vectorMaskToken[i].c_str());
        }
        else
        {
        	IPv4Address addressBase(vectorAddressToken[0].c_str());
        	myAddress = IPv4Address(addressBase.getInt() + uint32(getParentModule()->getId()));
        	netmask = IPv4Address(vectorMaskToken[0].c_str());
        }

        ie->ipv4Data()->setIPAddress(myAddress);
        ie->ipv4Data()->setNetmask(netmask);
        ie->setBroadcast(true);
        EV << "interface " << ifname << " gets " << myAddress.str() << "/" << netmask.str() << std::endl;
    }
    if (!interfaceFound)
    	throw std::runtime_error("Not interface register");
}

void HostAutoConfigurator2::setupRoutingTable()
{
    std::string interfaces = par("interfaces").stringValue();
    std::string addressBaseToken = par("addressBaseList").stringValue();
    std::string addressMask = par("addressMask").stringValue();

    cStringTokenizer interfaceTokenizer(interfaces.c_str());
    cStringTokenizer addressBaseTokenizer(addressBaseToken.c_str());
    cStringTokenizer addressMaskTokenizer(addressMask.c_str());


    cModule* host = getParentModule();
    // get our routing table
    IRoutingTable* routingTable = IPvXAddressResolver().routingTableOf(host);
    // get our interface table
    IInterfaceTable *ift = IPvXAddressResolver().interfaceTableOf(host);

    if (ift->getNumInterfaces()==2)
    {
        addDefaultRoutes();
        return;
    }

    const char *ifname;
    std::vector<std::string> vectorInterfaceToken;
    std::vector<std::string> vectorAddressToken;
    std::vector<std::string> vectorMaskToken;
    while((ifname = interfaceTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorInterfaceToken.push_back(val);
    }

    while((ifname = addressBaseTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorAddressToken.push_back(val);
    }

    while((ifname = addressMaskTokenizer.nextToken()) != NULL)
    {
    	std::string val(ifname);
    	vectorMaskToken.push_back(val);
    }

    if (vectorAddressToken.size()>1 && (vectorAddressToken.size()!=vectorInterfaceToken.size()))
    	opp_error("interfaces and addressBaseList has different sizes");

    if (vectorAddressToken.size()!=vectorMaskToken.size())
    	opp_error("addressMask and addressBaseList has different sizes");


    if (vectorAddressToken.size()==1)
    {
    	fillRoutingTables();
        return;
    }
    // get our host module


    for (unsigned int i=0;i<vectorInterfaceToken.size();i++)
    {
    	ifname = vectorInterfaceToken[i].c_str();
        InterfaceEntry* ie = ift->getInterfaceByName(ifname);
        if (!ie)
            continue;
        IPv4Address add(ie->ipv4Data()->getIPAddress().getInt() & ie->ipv4Data()->getNetmask().getInt());
        int i=0;
        while (i<routingTable->getNumRoutes())
        {
            if (routingTable->getRoute(i)->getInterface()==ie)
        	{
            	routingTable->deleteRoute(routingTable->getRoute(i));
                i=0;
        	}
        	else
        	    i++;
        }
        addRoute(ie,routingTable,add,ie->ipv4Data()->getNetmask());
    }

    if (!defaultAddr.isUnspecified())
    {
    	InterfaceEntry* ie = ift->getInterfaceByName(par("defaultInterface"));
        if (ie==NULL)
            opp_error("default ID interface doesn't exist");
        IPv4Route *e = new IPv4Route();
        e->setDestination(IPv4Address());
        e->setNetmask(IPv4Address());
        e->setGateway(defaultAddr);
        e->setInterface(ie);
        e->setType(IPv4Route::REMOTE);
        e->setSource(IPv4Route::MANUAL);
         //e->getMetric() = 1;
        routingTable->addRoute(e);
    }
}



void HostAutoConfigurator2::fillRoutingTables()
{
    // fill in routing tables with static routes
	cTopology topo("topo");
	topo.extractByProperty("node");

    cModule* host = getParentModule();
    // get our routing table
    IRoutingTable* routingTable = IPvXAddressResolver().routingTableOf(host);

    for (int i=0; i<topo.getNumNodes(); i++)
    {
    	if (getParentModule()== topo.getNode(i)->getModule())
    		continue;
        cTopology::Node *destNode = topo.getNode(i);

        // skip bus types

        std::string destModName = destNode->getModule()->getFullName();

        // calculate shortest paths from everywhere towards destNode
        topo.calculateUnweightedSingleShortestPathsTo(destNode);


        // get our interface table
        IInterfaceTable *ift = IPvXAddressResolver().interfaceTableOf(topo.getNode(i)->getModule());
        if (ift==NULL)
            continue;

        // add route (with host=destNode) to every routing table in the network
        // (excepting nodes with only one interface -- there we'll set up a default route)
        for (int j=0; j<topo.getNumNodes(); j++)
        {
            if (i==j) continue;

            cTopology::Node *atNode = topo.getNode(j);
            if (getParentModule()!=topo.getNode(j)->getModule())
            	continue;

            if (atNode->getNumPaths()==0)
                continue; // not connected

            IInterfaceTable *iftN = IPvXAddressResolver().interfaceTableOf(topo.getNode(j)->getModule());
            if (iftN==NULL)
            	continue;

          	int outputGateIdTarget =  atNode->getPath(atNode->getNumInLinks()-1)->getLocalGate()->getId();
            InterfaceEntry *ieTarget = ift->getInterfaceByNodeOutputGateId(outputGateIdTarget);
            if (!ieTarget)
                error("%s has no interface for output gate id %d", ift->getFullPath().c_str(), outputGateIdTarget);


            int outputGateId = atNode->getPath(0)->getLocalGate()->getId();
            InterfaceEntry *ie = iftN->getInterfaceByNodeOutputGateId(outputGateId);

            if (!ie)
                error("%s has no interface for output gate id %d", iftN->getFullPath().c_str(), outputGateId);

            IPv4Route *e = new IPv4Route();
            e->setDestination(ieTarget->ipv4Data()->getIPAddress());
            e->setNetmask(IPv4Address(255,255,255,255)); // full match needed
            e->setInterface(ie);
            e->setType(IPv4Route::DIRECT);
            e->setSource(IPv4Route::MANUAL);
            //e->getMetric() = 1;
            routingTable->addRoute(e);
        }
    }
}
