#!/usr/bin/env python

import sys
import xgboost as xgb
import pandas as pd
import numpy as np

X = pd.read_csv('wine.csv')
split_at = int(len(X)*0.75)
Xtrain = X.head(split_at)
Xtest = X[split_at:]
print "loaded "+str(len(X))+" rows, train size "+str(len(Xtrain))+", test size "+str(len(Xtest))

dtrain = xgb.DMatrix(Xtrain.drop('Type', axis=1), map(lambda x: 1 if x == "White" else 0, Xtrain["Type"].values))
dtest  = xgb.DMatrix(Xtest.drop('Type', axis=1),  map(lambda x: 1 if x == "White" else 0, Xtest["Type"].values))
ytest  = map(lambda x: 1 if x == "White" else 0, Xtest["Type"].values)

print "training ..."

param = {'silent':1, 'objective':'binary:logistic','eval_metric': 'logloss'}
param['nthread'] = 4
num_round = 30
model = xgb.train( param.items(), dtrain, num_round, [(dtrain,'train'),(dtest,'test')])

pred = model.predict(dtest);
pred_ok = sum( round(pred[i]) == ytest[i] for i in range(len(ytest)) )
print 'OK is ',pred_ok,"(",pred_ok/float(len(ytest)),")"

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

