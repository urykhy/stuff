#!/usr/bin/env python

import sys
import xgboost as xgb
import pandas as pd
import numpy as np

X = pd.read_csv('wine.csv')

dtrain = xgb.DMatrix(X.drop('Type', axis=1), map(lambda x: 1 if x == "White" else 0, X["Type"].values))

print "training ..."

param = {'silent':1, 'objective':'binary:logistic','eval_metric': 'logloss'}
param['nthread'] = 4
num_round = 10
model = xgb.train( param.items(), dtrain, num_round, [(dtrain,'train')])

ypred = model.predict(xgb.DMatrix(X.drop('Type', axis=1)))
ypred = map(lambda x: int(round(x)), ypred)

count = 0
for ((index,x),y) in zip(X.iterrows(), ypred):
    xgood = 1 if x['Type'] == "White" else 0
    if y == xgood:
        count+=1
print "Good is",count*100.0/len(X),": ",count,"from",len(X)

# feature importance
from matplotlib import pylab as plt
import operator

model.dump_model('xgb.fmap');
importance = model.get_fscore()
importance = sorted(importance.items(), key=operator.itemgetter(1))
df = pd.DataFrame(importance, columns=['feature', 'fscore'])
df['fscore'] = df['fscore'] / df['fscore'].max() * 10

plt.figure()
df.plot()
df.plot(kind='barh', x='feature', y='fscore', legend=False, figsize=(25, 15))
plt.title('XGBoost Feature Importance')
plt.xlabel('relative importance')
plt.gcf().savefig('Feature_Importance_xgb.png')

