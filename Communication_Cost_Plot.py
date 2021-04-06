#!/usr/bin/env python
# coding: utf-8

# In[24]:


import matplotlib.pyplot as plt
import math
plt.rcParams.update({'figure.autolayout': True})


# In[25]:


def computeY(n, m, rho):
    if rho<0.1:
        v = 4856
    else:
        v = 18217
    y = [
        8*n*m,
        n*(64*n+(5*n-4)*32+8*m),
        0.5*n*m*math.ceil(math.log(2*n+1)/math.log(2)),
        16*n*m+n,
        0.5*n*m*math.ceil(math.log(2*n+1)/math.log(2))+16*n,
        5*n*m+0.5*n*v*math.ceil(math.log(2*n+1)/math.log(2))+16*n,
        8*rho*n*m+64
    ]
    for i in range(7):
        y[i] /= 1024*1024
    return y


# In[26]:


x = ['Raw Alg', '[12]', '[42] w/ TSS', '[42] w/ HE', '[38] w/ NoUnion', '[38] w/ SecureUnion', 'Our Protocol']
x_pos = [i for i in range(7)]


# In[37]:


for n in [100, 500, 1000]:
    for m in [60000, 60000000]:
        for rho in [0.025, 0.125]:
            y = computeY(n, m, rho)
            plt.tight_layout()
            fig = plt.figure()
            if m==60000:
                model_name = "LeNet-5"
                plt.ylabel('Communication cost per round (MB)', fontweight='bold')
            else:
                model_name = "AlexNet"
                for i in range(7):
                    y[i]/=1024
                plt.ylabel('Communication cost per round (GB)', fontweight='bold')
            plt.bar(x_pos, y)
            ymin, ymax = plt.ylim()
            plt.ylim(ymin, ymax * 1.05)
            for index, value in enumerate(y):
                plt.text(index-0.28, value+1, str(round(value, 2)), fontweight='bold')
            plt.xticks(x_pos, x, rotation='45', fontweight='bold')
            # Set a title of the current axes.
            plt.title(model_name+' with '+str(n)+' clients, '+r'$\mathbf{\rho}$='+str(rho), fontweight='bold')
            plt.savefig(model_name+"_n_"+str(n)+"_rho_"+str(rho)+".png", dpi=300)
#             plt.show()


# In[ ]:





# In[ ]:




