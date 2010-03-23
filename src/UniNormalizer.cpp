/*******************************************************************************
    Copyright 2006-2009 Lukas Käll <lukas.kall@cbr.su.se>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

 *******************************************************************************/
#include <vector>
#include <iostream>
#include <math.h>
using namespace std;
#include "DataSet.h"
#include "Normalizer.h"
#include "UniNormalizer.h"
#include "SetHandler.h"

UniNormalizer::UniNormalizer() {
}

UniNormalizer::~UniNormalizer() {
}

void UniNormalizer::unnormalizeweight(const vector<double>& in,vector<double>& out) {
  double sum = 0;
  unsigned int i=0;
  for (; i<numFeatures; i++) {
    out[i]=in[i]/div[i];
    sum += sub[i]*in[i]/div[i];
  }
  out[i]=in[i]-sum;
}

void UniNormalizer::normalizeweight(const vector<double>& in, vector<double>& out) {
  double sum = 0;
  size_t i=0;
  for (; i<numFeatures; i++) {
    out[i]=in[i]*div[i];
    sum+=sub[i]*in[i];
  }
  out[i]=in[i]+sum;
}

void UniNormalizer::setSet(vector<double *> & featuresV,vector<double *> & rtFeaturesV, size_t nf, size_t nrf) {
  numFeatures = nf;
  numRetentionFeatures=nrf;
  sub.resize(nf+nrf,0.0);
  div.resize(nf+nrf,0.0);
  vector<double> mins(nf+nrf,1e+100), maxs(nf+nrf,-1e+100);

  double * features;
  size_t ix;
  vector<double *>::iterator it=featuresV.begin();
  for (; it!=featuresV.end(); ++it) {
    features = *it;
    for (ix=0; ix<numFeatures; ix++) {
      mins[ix]=min(features[ix],mins[ix]);
      maxs[ix]=max(features[ix],maxs[ix]);
    }
  }
  for (it=rtFeaturesV.begin(); it!=rtFeaturesV.end(); ++it) {
    features = *it;
    for (ix=numFeatures; ix<numFeatures+numRetentionFeatures; ++ix) {
      mins[ix]=min(features[ix-numFeatures],mins[ix]);
      maxs[ix]=max(features[ix-numFeatures],maxs[ix]);
    }
  }
  for (ix=0; ix<numFeatures+numRetentionFeatures; ++ix) {
    sub[ix]=mins[ix];
    div[ix]=maxs[ix]-mins[ix];
    if (div[ix]<=0)
      div[ix]=1.0;
  }
}


/*
void UniNormalizer::setPsmSet(vector<PSMDescription> & psms, size_t noFeat)
{
	double * retentionFeatures;
	size_t ix;
	vector<PSMDescription>::iterator it;
	numRetentionFeatures = noFeat;
	vector<double> mins(noFeat, 1e+100), maxs(noFeat, -1e+100);
	sub.resize(noFeat,0.0);
	div.resize(noFeat,0.0);

    for(it = psms.begin(); it != psms.end(); ++it)
    {
    	retentionFeatures = it->retentionFeatures;
    	for (ix = 0;ix < numRetentionFeatures; ++ix)
    	{
    		mins[ix] = min(retentionFeatures[ix], mins[ix]);
    	    maxs[ix] = max(retentionFeatures[ix], maxs[ix]);
    	}
    }

	for(ix = 0; ix < numRetentionFeatures; ++ix)
	{
		sub[ix] = mins[ix];
		div[ix] = maxs[ix] - mins[ix];
		if (div[ix] <= 0)
			div[ix] = 1.0;
	}
}
*/
