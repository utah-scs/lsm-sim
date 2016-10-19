#!/usr/bin/python

import struct
import os
import numpy as np
import plistlib
import string
import plistlib
import argparse
from sklearn import linear_model
from sklearn import svm
from numpy import genfromtxt
import numpy.lib.recfunctions as nlr
from scipy.optimize import curve_fit
import pickle
from sklearn.externals import joblib

clf = svm.LinearSVC(random_state=10)

def FitFunction(App_number):
    y=np.genfromtxt('my_file' + str(App_number)  + '.csv',dtype={'names': ('kId','First_Hit_Time_period','Avg_Time_Between_Hits','Time_Between_Last_Hits','Max_Time_between_Hits','Amount_Of_Hits_Till_Arrival','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(6)).astype(np.float)
    x=np.genfromtxt('my_file' + str(App_number) + '.csv',dtype={'names': ('kId','a','Avg_Time_Between_Hits','Time_Between_Last_Hits','Max_Time_between_Hits','Amount_Of_Hits_Till_Arrival','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(1)).astype(np.float)
    t=np.genfromtxt('my_file' + str(App_number) + '.csv',dtype={'names': ('kId','First_Hit_Time_period','a','Max_Time_between_Hits','Amount_Of_Hits_Till_Arrival','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(2)).astype(np.float)
    s=np.genfromtxt('my_file' + str(App_number) + '.csv',dtype={'names': ('kId','First_Hit_Time_period','Avg_Time_Between_Hits','Time_Between_Last_Hits','a','Amount_Of_Hits_Till_Arrival','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(3)).astype(np.float)
    r=np.genfromtxt('my_file' + str(App_number) + '.csv',dtype={'names': ('kId','First_Hit_Time_period','Avg_Time_Between_Hits','Time_Between_Last_Hits','a','Amount_Of_Hits_Till_Arrival','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(4)).astype(np.float)
    z=np.genfromtxt('my_file' + str(App_number) + '.csv',dtype={'names': ('kId','First_Hit_Time_period','Avg_Time_Between_Hits','Time_Between_Last_Hits','Max_Time_between_Hits','a','target'),'formats': ('i10','f8','f8','f8','f8','f8','f8')},delimiter=',',skiprows=1,usecols=(5)).astype(np.float)
    X=np.hstack((x.reshape(len(x), 1),t.reshape(len(t), 1),s.reshape(len(s), 1),r.reshape(len(r), 1),z.reshape(len(z), 1)))
    
    clf.fit(X.reshape(len(X),5),y)
    print clf.coef_, clf.intercept_
    with open('fit_app_' + str(App_number) + '.pkl', 'wb') as f:
        pickle.dump(clf, f)
    return clf

def LoadSavedFunction(App_number):
    print "Load Function for app:" + str(App_number)
    with open("fit_functions/fit_app" + str(App_number) + "_" + str(SVM_TH)  + ".pkl", 'rb') as f:
        clf = pickle.load(f)
    print clf.coef_, clf.intercept_

def PredictFunction(a, b, c, d, e,App_number,SVM_TH):
    perclf = svm.LinearSVC(random_state=10)

    with open("fit_functions/fit_app" + str(App_number) + "_" + str(SVM_TH)  + ".pkl", 'rb') as f:
        perclf = pickle.load(f)
    #print perclf.coef_, perclf.intercept_
    tmp = perclf.predict([[a,b,c,d,e]])
    #print(tmp)
    return(tmp[0])

#FitFunction(3)
#print("app1")
#LoadSavedFunction(1)
#print("app3") 
#LoadSavedFunction(3)
#print("app18")
#LoadSavedFunction(18)
#print("app19")
#LoadSavedFunction(19)
#print("app20")
#LoadSavedFunction(20)
#PredictFunction(1,1,1,1,1,3)
