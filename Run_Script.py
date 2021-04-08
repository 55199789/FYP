#!/usr/bin/env python
# coding: utf-8

# # Generate data

# In[1]:


import os
dims = [100000*i for i in range(1, 6)]
rhos = ['', 0.02, 0.1]
clientNums = [100*i for i in range(1, 6)]


# In[2]:


os.system("make clean")
os.system("make -j4")


# In[ ]:


for dim in dims:
    for rho in rhos:
        for clientNum in clientNums:
            cmd = "./app "+str(clientNum)+" "+str(dim)+" "+str(rho)+" >> output/"+str(clientNum)+"_"+str(dim)+"_"+str(rho)+".out"
            print(cmd)
            for t in range(100):
                os.system(cmd)
            print("\tcompleted!\n")


# # Process Practical Secure Aggregation Data

# In[ ]:


import numpy as np
import matplotlib.pyplot as plt
dims = [100000*i for i in range(1, 6)]
survRates = [1, 0.9]
clientNums = [100*i for i in range(1, 6)]
rhos = [1, 0.1, 0.02]
labels = ["[12] w/o dropout", "[12] w/ 10% dropout",           "Ours w/o compression", r"Ours w/ $rho=0.1$", r"Ours w/o $rho=0.02$"]
colors = ["C0", "C1", "C2", "C3", "C4"]


# In[ ]:


clientTimesP = {}
serverTimesP = {}
clientTimesP_std = {}
serverTimesP_std = {}

for clientNum in clientNums:
    clientTimesP[clientNum]={}
    serverTimesP[clientNum]={}
    clientTimesP_std[clientNum]={}
    serverTimesP_std[clientNum]={}
    for dim in dims:
        clientTimesP[clientNum][dim]={}
        serverTimesP[clientNum][dim]={}
        clientTimesP_std[clientNum][dim]={}
        serverTimesP_std[clientNum][dim]={}
        for survRate in survRates:
            fileName = "../PracSecure/output/"+str(clientNum)+"_"+str(dim)+"_"+str(survRate)+".out"
            f = open(fileName)
            clientTime = []
            serverTime = []
            lines = f.readlines()
            times = len(lines) // 11
            for i in range(times):
                l1 = lines[i*11+9]
                l2 = lines[i*11+10]
                clientTime.append(float(l1[l1.rfind("\t"):-3]))
                serverTime.append(float(l2[l2.rfind("\t"):-3]))
            clientTimesP[clientNum][dim][survRate]=np.mean(clientTime)
            serverTimesP[clientNum][dim][survRate]=np.mean(serverTime)
            clientTimesP_std[clientNum][dim][survRate]=np.std(clientTime)
            serverTimesP_std[clientNum][dim][survRate]=np.std(serverTime)


# # Process Our Protocol

# In[ ]:


clientTimes = {}
serverTimes = {}
clientTimes_std = {}
serverTimes_std = {}

for clientNum in clientNums:
    clientTimes[clientNum]={}
    serverTimes[clientNum]={}
    clientTimes_std[clientNum]={}
    serverTimes_std[clientNum]={}
    for dim in dims:
        clientTimes[clientNum][dim]={}
        serverTimes[clientNum][dim]={}
        clientTimes_std[clientNum][dim]={}
        serverTimes_std[clientNum][dim]={}
        for rho in rhos:
            fileName = "output/"+str(clientNum)+"_"+str(dim)+"_"+str(rho)+".out"
            f = open(fileName)
            clientTime = []
            serverTime = []
            serverKA = []
            serverAggr = []
            serverComp = []
            lines = f.readlines()
            times = 100
            for i in range(times):
                if (rho==''):
                    st = i*29
                    l1 = lines[st+14]
                    l2 = lines[st+17]
                    l3 = lines[st+12]
                    l4 = lines[st+21]
                    l5 = lines[st+25]
                    l6 = ": 0ms"
                else:
                    st = i*32
                    l1 = lines[st+14]
                    l2 = lines[st+18]
                    l3 = lines[st+12]
                    l4 = lines[st+22]
                    l5 = lines[st+28]
                    l6 = lines[st+26]
                clientTime.append(float(l1[l1.find(":")+1:-3]) +
                                    float(l2[l2.find(":")+1:l2.find("ms")]))
                serverTime.append(float(l5[l5.find(":")+1:l5.find("ms")]))
                serverKA.append(float(l3[l3.find(":")+1:-3]))
                serverAggr.append(float(l4[l4.find(":")+1:l4.find("ms")]))
                serverComp.append(float(l6[l6.find(":")+1:l6.find("ms")]))                
            clientTimes[clientNum][dim][rhos]=np.mean(clientTime)
            serverTimes[clientNum][dim][rhos]=np.mean(serverTime)
            serverTimes[clientNum][dim][rhos]['KA']=np.mean(serverKA)
            serverTimes[clientNum][dim][rhos]['Aggr']=np.mean(serverAggr)
            serverTimes[clientNum][dim][rhos]['Comp']=np.mean(serverComp)
            clientTimes_std[clientNum][dim][rhos]=np.std(clientTime)
            serverTimes_std[clientNum][dim][rhos]=np.std(serverTime)


# # Plot line chart

# ## x-axis: dim - client

# In[ ]:


for clientNum in clientNums:
    y = [[] for i in range(2+3)]
    e = [[] for i in range(2+3)]
    for dim in dims:
        for i in range(2):
            y[i].append(clientTimesP[clientNum][dim][survRates[i]])
            e[i].append(clientTimesP_std[clientNum][dim][survRates[i]])
        for i in range(3):
            y[i].append(clientTimes[clientNum][dim][rhos[i]])
            e[i].append(clientTimes_std[clientNum][dim][rhos[i]])
    plt.figures()
    for i in range(5):
        plt.plot(dims, y[i], label = labels[i], color=colors[i])
        plt.errorbar(dims, y[i], e[i], color=colors[i])
    plt.xlabel("Update vector size", fontweight="bold")
    plt.ylabel("Running time for each client", fontweight="bold")
    plt.legend(prop={'weight':'bold'})
    plt.title("The number of clients is fixed to "+str(clientNum))
    plt.savefig("imgs/ClientCN"+str(clientNum)+".png", dpi=300)


# ## x-axis: dim - server

# In[ ]:


for clientNum in clientNums:
    y = [[] for i in range(2+3)]
    e = [[] for i in range(2+3)]
    for dim in dims:
        for i in range(2):
            y[i].append(serverTimesP[clientNum][dim][survRates[i]])
            e[i].append(serverTimesP_std[clientNum][dim][survRates[i]])
        for i in range(3):
            y[i].append(serverTimes[clientNum][dim][rhos[i]])
            e[i].append(serverTimes_std[clientNum][dim][rhos[i]])
    plt.figures()
    for i in range(5):
        plt.plot(dims, y[i], label = labels[i], color=colors[i])
        plt.errorbar(dims, y[i], e[i], color=colors[i])
    plt.xlabel("Update vector size", fontweight="bold")
    plt.ylabel("Running time for the server", fontweight="bold")
    plt.legend(prop={'weight':'bold'})
    plt.title("The number of clients is fixed to "+str(clientNum))
    plt.savefig("imgs/ServerCN"+str(clientNum)+".png", dpi=300)


# ## x-axis: clientNum - client

# In[ ]:


for dim in dims:
    y = [[] for i in range(2+3)]
    e = [[] for i in range(2+3)]
    for clientNum in clientNums:
        for i in range(2):
            y[i].append(clientTimesP[clientNum][dim][survRates[i]])
            e[i].append(clientTimesP_std[clientNum][dim][survRates[i]])
        for i in range(3):
            y[i].append(clientTimes[clientNum][dim][rhos[i]])
            e[i].append(clientTimes_std[clientNum][dim][rhos[i]])
    plt.figures()
    for i in range(5):
        plt.plot(dims, y[i], label = labels[i], color=colors[i])
        plt.errorbar(dims, y[i], e[i], color=colors[i])
    plt.xlabel("The number of clients", fontweight="bold")
    plt.ylabel("Running time for each client", fontweight="bold")
    plt.legend(prop={'weight':'bold'})
    plt.title("The length of update vector is fixed to "+str(dim))
    plt.savefig("imgs/ClientDIM"+str(dim)+".png", dpi=300)


# ## x-axis: clientNum - server

# In[ ]:


for dim in dims:
    y = [[] for i in range(2+3)]
    e = [[] for i in range(2+3)]
    for clientNum in clientNums:
        for i in range(2):
            y[i].append(serverTimesP[clientNum][dim][survRates[i]])
            e[i].append(serverTimesP_std[clientNum][dim][survRates[i]])
        for i in range(3):
            y[i].append(serverTimes[clientNum][dim][rhos[i]])
            e[i].append(serverTimes_std[clientNum][dim][rhos[i]])
    plt.figures()
    for i in range(5):
        plt.plot(dims, y[i], label = labels[i], color=colors[i])
        plt.errorbar(dims, y[i], e[i], color=colors[i])
    plt.xlabel("The number of clients", fontweight="bold")
    plt.ylabel("Running time for the server", fontweight="bold")
    plt.legend(prop={'weight':'bold'})
    plt.title("The length of update vector is fixed to "+str(dim))
    plt.savefig("imgs/ServerDIM"+str(dim)+".png", dpi=300)


# In[ ]:




