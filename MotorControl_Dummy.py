#!/usr/bin/env python
# coding: utf-8

# In[107]:


import time
import numpy as np
import math

class motor():
    def __init__(self, name, pin_list):
        self.name = name
        self.chA = pin_list[0]
        self.chB = pin_list[1]
        self.dir = pin_list[2]
        self.pwm = pin_list[3]
        self.wheel_radius=0
        self.fwd_flag=True
        self.pwm_val=0
        self.dir_val = 0
        
    def stop(self):
        self.pwm_val=0
        self.dir_val=0
    
    @property
    def circumference(self):
        return 2*math.pi*self.wheel_radius
            
    
    def initiate_encoder(self):
        print("Encoder A is connected to: ",self.chA)
        time.sleep(1)
        print("Encoder B is connected to: ",self.chB)
        time.sleep(1)
        print("starting encoder pins ...")
        time.sleep(2)
        print("all done")
        
    def move_forward(self,ext_pwm_val):
        # First check and stop any reverse movement
        if(self.fwd_flag==False):
            print(f"flag set @ {self.fwd_flag}")
            print("stopping motor")
            self.fwd_flag=True
            self.pwm_val=0
            self.dir_val=0
            time.sleep(0.1)

        # Set dir and pwm_value with external input >>
        self.dir_val = 1
        self.pwm_val=ext_pwm_val
        #time.sleep(0.5)
        
        print(f"direction on {self.dir} set to {self.dir_val}")
        time.sleep(1)
        print(f"pwm on {self.pwm} set to {self.pwm_val}")
        time.sleep(1)
        print(f"running at {self.pwm_val} speed")
        return(self.fwd_flag,self.dir_val,self.pwm_val)


    def move_reverse(self,ext_pwm_val):
        if(self.fwd_flag==True):
            print(f"flag set @ {self.fwd_flag}")
            print("stopping motor")
            self.fwd_flag=False
            self.pwm_val=0
            self.dir_val=0
            time.sleep(0.1)
            
        #set dir and pwm_value with external input >>        
        self.dir_val=-1
        self.pwm_val = ext_pwm_val
        #time.sleep(0.5)
        print(f"direction on {self.dir} set to {self.dir_val}")
        time.sleep(1)
        print(f"pwm on {self.pwm} set to {self.pwm_val}")
        time.sleep(1)
        print(f"running reverse at {self.pwm_val} speed")
        return(self.fwd_flag,self.dir_val,self.pwm_val)


# In[108]:


l=[22,23,17,18]
m1 = motor("left",l)


# In[109]:


m1.initiate_encoder()


# In[110]:


m1.move_forward(0.3)


# In[111]:


m1.move_forward(0.2)


# In[57]:


m1.move_forward(0.5)


# In[112]:


m1.move_reverse(0.3)


# In[ ]:




