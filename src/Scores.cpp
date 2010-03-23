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
#include <assert.h>
#include <iostream>
#include <fstream>
#include <utility>
#include <algorithm>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <math.h>
using namespace std;
#include "DataSet.h"
#include "Normalizer.h"
#include "SetHandler.h"
#include "Scores.h"
#include "Globals.h"
#include "PosteriorEstimator.h"
#include "ssl.h"

inline bool operator>(const ScoreHolder &one, const ScoreHolder &other) {
  return (one.score>other.score);
}

inline bool operator<(const ScoreHolder &one, const ScoreHolder &other) {
  return (one.score<other.score);
}

inline string getRidOfUnprintablesAndUnicode(string inpString)  {
  string outputs = "";
  for ( int jj = 0; jj < inpString.size(); jj++ )  {
    char ch = inpString[ jj ];
    if ( ( ( int ) ch ) >= 32 && ( ( int ) ch ) <= 128 )  {
      outputs += ch;
    }
  }
  return outputs;
}

ostream& operator<<(ostream& os, const ScoreHolder& sh) {
  if (sh.label!=1 && !Scores::isOutXmlDecoys())
    return os;
  os << "  <psm psm_id=\"" << sh.pPSM->id << "\"";
  if (sh.label!=1)
    os << " decoy=\"true\"";
  os << ">" << endl;
  os << "    <svm_score>"<< sh.score << "</svm_score>" << endl;
  os << "    <q_value>"<< sh.pPSM->q << "</q_value>" << endl;
  os << "    <pep>"<< sh.pPSM->pep << "</pep>" << endl;
  if (DataSet::getCalcDoc())
    os << "    <retentionTime observed=\""<< PSMDescription::unnormalize(sh.pPSM->retentionTime) <<
       "\" predicted=\"" << PSMDescription::unnormalize(sh.pPSM->predictedTime) << "\"/>" << endl;
  string peptide = sh.pPSM->getPeptide();
  string::size_type pos1 = peptide.find('.');
  string n = peptide.substr(0,pos1);
  string::size_type pos2 = peptide.find('.',++pos1);
  string c = peptide.substr(pos2+1,peptide.size());
  string centpep = peptide.substr(pos1,pos2-pos1);
  os << "    <peptide n=\""<< n << "\" c=\"" << c <<"\" seq=\"" << centpep << "\"/>" << endl;
  for(set<string>::const_iterator pid = sh.pPSM->proteinIds.begin(); pid != sh.pPSM->proteinIds.end(); ++pid) {
    os << "    <protein_id>" << getRidOfUnprintablesAndUnicode(*pid) << "</protein_id>" << endl;
  }
  os << "  </psm>" << endl;
  return os;
}


Scores::Scores() {
  pi0=1.0;
  targetDecoySizeRatio=1;
  neg=0;
  pos=0;
  posNow=0;
}

Scores::~Scores() {
}

bool Scores::outxmlDecoys = false;
uint32_t Scores::seed = 1;

void Scores::merge(vector<Scores>& sv, bool reportUniquePeptides) {
  scores.clear();
  for(vector<Scores>::iterator a = sv.begin(); a!=sv.end(); a++) {
    sort(a->begin(),a->end(), greater<ScoreHolder>());
    a->estimatePi0();
    a->calcQ();
    a->calcPep();
    a->normalizeScores();
    copy(a->begin(),a->end(),back_inserter(scores));
  }
  if (reportUniquePeptides)
    weedOutRedundant();
  neg=count_if(scores.begin(),scores.end(),mem_fun_ref(&ScoreHolder::isDecoy));
  pos=count_if(scores.begin(),scores.end(),mem_fun_ref(&ScoreHolder::isTarget));
  targetDecoySizeRatio=pos/max(1.0,(double)neg);
  sort(scores.begin(),scores.end(), greater<ScoreHolder>() );
  estimatePi0();
}

void Scores::printRetentionTime(ostream& outs, double fdr) {
  vector<ScoreHolder>::iterator it;
  for(it=scores.begin(); it!=scores.end() && it->pPSM->q <= fdr; ++it) {
    if (it->label!=-1)
      outs << PSMDescription::unnormalize(it->pPSM->retentionTime) << "\t"
           << PSMDescription::unnormalize(doc.estimateRT(it->pPSM->retentionFeatures)) << "\t"
           << it->pPSM->peptide << endl;
  }
}

double Scores::calcScore(const double * feat) const {
  register int ix=FeatureNames::getNumFeatures();
  register double score = w_vec[ix];
  for(; ix--;) {
    score += feat[ix]*w_vec[ix];
  }
  return score;
}

ScoreHolder* Scores::getScoreHolder(const double *d) {
  if (scoreMap.size()==0) {
    vector<ScoreHolder>::iterator it;
    for(it=scores.begin(); it!=scores.end(); it++) {
      scoreMap[it->pPSM->features] = &(*it);
    }
  }
  std::map<const double *,ScoreHolder *>::iterator res = scoreMap.find(d);

  if (res != scoreMap.end())
    return res->second;
  return NULL;
}


void Scores::fillFeatures(SetHandler& norm,SetHandler& shuff) {
  scores.clear();
  PSMDescription * pPSM;
  SetHandler::Iterator shuffIter(&shuff), normIter(&norm);
  while((pPSM=normIter.getNext())!=NULL) {
    scores.push_back(ScoreHolder(.0,1,pPSM));
  }
  while((pPSM=shuffIter.getNext())!=NULL) {
    scores.push_back(ScoreHolder(.0,-1,pPSM));
  }
  pos = norm.getSize();
  neg = shuff.getSize();
  targetDecoySizeRatio=norm.getSize()/(double)shuff.getSize();
}

// Park–Miller random number generator
// from wikipedia
uint32_t Scores::lcg_rand() {
  seed = ((uint64_t)seed * 279470273) % 4294967291;
  return seed;
}

void Scores::createXvalSets(vector<Scores>& train,vector<Scores>& test, const unsigned int xval_fold) {
  train.resize(xval_fold);
  test.resize(xval_fold);
  vector<size_t> remain(xval_fold);
  size_t fold = xval_fold, ix = scores.size();
  while (fold--) {
    remain[fold] = ix / (fold + 1);
    ix -= remain[fold];
  }

  for(unsigned int j=0; j<scores.size(); j++) {
    ix = lcg_rand()%(scores.size()-j);
    fold = 0;
    while(ix>remain[fold])
      ix-= remain[fold++];
    for(unsigned int i=0; i<xval_fold; i++) {
      if(i==fold) {
        test[i].scores.push_back(scores[j]);
      } else {
        train[i].scores.push_back(scores[j]);
      }
    }
    --remain[fold];
  }
  vector<ScoreHolder>::const_iterator it;
  for(unsigned int i=0; i<xval_fold; i++) {
    train[i].pos=0;
    train[i].neg=0;
    for(it=train[i].begin(); it!=train[i].end(); it++) {
      if (it->label==1) train[i].pos++;
      else train[i].neg++;
    }
    train[i].targetDecoySizeRatio=train[i].pos/(double)train[i].neg;
    test[i].pos=0;
    test[i].neg=0;
    for(it=test[i].begin(); it!=test[i].end(); it++) {
      if (it->label==1) test[i].pos++;
      else test[i].neg++;
    }
    test[i].targetDecoySizeRatio=test[i].pos/(double)test[i].neg;
  }
}

void Scores::normalizeScores() {
  vector<ScoreHolder>::iterator it = scores.begin();
  double breakQ=0.0;
  for(it=scores.begin(); it!=scores.end(); ++it) {
    // Score identifications based on their -log(PEP) or if PEP >=1 -log(1+q-q_at_pep1)
    if (it->pPSM->pep<1.0) {
      it->score = -log(it->pPSM->pep);
      breakQ = it->pPSM->q;
    } else {
      it->score = -log(1.0+(it->pPSM->q-breakQ));
    }
  }
}

int Scores::calcScores(vector<double>& w,double fdr) {
  w_vec=w;
  const double * features;
  unsigned int ix;
  vector<ScoreHolder>::iterator it = scores.begin();
  while(it!=scores.end()) {
    features = it->pPSM->features;
    it->score = calcScore(features);
    it++;
  }
  sort(scores.begin(),scores.end(),greater<ScoreHolder>());
  if (VERB>3) {
    cerr << "10 best scores and labels" << endl;
    for (ix=0; ix < 10; ix++) {
      cerr << scores[ix].score << " " << scores[ix].label << endl;
    }
    cerr << "10 worst scores and labels" << endl;
    for (ix=scores.size()-10; ix < scores.size(); ix++) {
      cerr << scores[ix].score << " " << scores[ix].label << endl;
    }
  }
  return calcQ(fdr);
}

int Scores::calcQ(double fdr) {
  vector<ScoreHolder>::iterator it;
  int positives=0,nulls=0;
  double efp=0.0,q;
  for(it=scores.begin(); it!=scores.end(); it++) {
    if (it->label!=-1)
      positives++;
    if (it->label==-1) {
      nulls++;
      efp=pi0*nulls*targetDecoySizeRatio;
    }
    if (positives)
      q=efp/(double)positives;
    else
      q=pi0;
    if (q>pi0)
      q=pi0;
    it->pPSM->q=q;
    if (fdr>=q)
      posNow = positives;
  }
  for (int ix=scores.size(); --ix;) {
    if (scores[ix-1].pPSM->q > scores[ix].pPSM->q)
      scores[ix-1].pPSM->q = scores[ix].pPSM->q;
  }
  return posNow;
}

void Scores::generateNegativeTrainingSet(AlgIn& data,const double cneg) {
  unsigned int ix1=0,ix2=0;
  for(ix1=0; ix1<size(); ix1++) {
    if (scores[ix1].label==-1) {
      data.vals[ix2]=scores[ix1].pPSM->features;
      data.Y[ix2]=-1;
      data.C[ix2++]=cneg;
    }
  }
  data.negatives=ix2;
}


void Scores::generatePositiveTrainingSet(AlgIn& data,const double fdr,const double cpos) {
  unsigned int ix1=0,ix2=data.negatives,p=0;
  for(ix1=0; ix1<size(); ix1++) {
    if (scores[ix1].label==1) {
      if (fdr<scores[ix1].pPSM->q) {
        posNow=p;
        break;
      }
      data.vals[ix2]=scores[ix1].pPSM->features;
      data.Y[ix2]=1;
      data.C[ix2++]=cpos;
      ++p;
    }
  }
  data.positives=p;
  data.m=ix2;
}

void Scores::weedOutRedundant() {
  // Routine that sees to that only unique peptides are kept
  vector<ScoreHolder>::iterator it = scores.begin();
  set<string> uniquePeptides;
  pair<set<string>::iterator,bool> ret;
  for (; it!=scores.end();) {
    ret=uniquePeptides.insert(it->pPSM->peptide);
    if(!ret.second) {
      it = scores.erase(it);
    } else {
      ++it;
    }
  }
  sort(scores.begin(),scores.end(), greater<ScoreHolder>() ); // Is this really needed?
}

void Scores::recalculateDescriptionOfGood(const double fdr) {
  doc.clear();
  unsigned int ix1=0;
  for(ix1=0; ix1<size(); ix1++) {
    if (scores[ix1].label==1) {
//      if (fdr>scores[ix1].pPSM->q) {
      if (0.0>=scores[ix1].pPSM->q) {
        doc.registerCorrect(*scores[ix1].pPSM);
      }
    }
  }
  doc.trainCorrect();
  setDOCFeatures();
}

void Scores::setDOCFeatures() {
  for(size_t ix1=0; ix1<size(); ++ix1) {
    doc.setFeatures(*scores[ix1].pPSM);
  }
}


int Scores::getInitDirection(const double fdr, vector<double>& direction, bool findDirection) {
  int bestPositives = -1;
  int bestFeature =-1;
  bool lowBest = false;

  if (findDirection) {
    for (unsigned int featNo=0; featNo<FeatureNames::getNumFeatures(); featNo++) {
      vector<ScoreHolder>::iterator it = scores.begin();
      while(it!=scores.end()) {
        it->score = it->pPSM->features[featNo];
        it++;
      }
      sort(scores.begin(),scores.end());
      for (int i=0; i<2; i++) {
        int positives=0,nulls=0;
        double efp=0.0,q;
        for(it=scores.begin(); it!=scores.end(); it++) {
          if (it->label!=-1)
            positives++;
          if (it->label==-1) {
            nulls++;
            efp=pi0*nulls*targetDecoySizeRatio;
          }
          if (positives)
            q=efp/(double)positives;
          else
            q=pi0;
          if (fdr<=q) {
            if (positives>bestPositives && scores.begin()->score!=it->score) {
              bestPositives=positives;
              bestFeature = featNo;
              lowBest = (i==0);
            }
            if (i==0) {
              reverse(scores.begin(),scores.end());
            }
            break;
          }
        }
      }
    }
    for (int ix=FeatureNames::getNumFeatures(); ix--;) {
      direction[ix]=0;
    }
    direction[bestFeature]=(lowBest?-1:1);
    if (VERB>1) {
      cerr << "Selected feature number " << bestFeature +1 << " as initial search direction, could separate " <<
           bestPositives << " positives in that direction" << endl;
    }
  } else {
    bestPositives = calcScores(direction,fdr);
  }
  return bestPositives;
}

double Scores::estimatePi0() {
  vector<pair<double,bool> > combined;
  vector<double> pvals;
  transform(scores.begin(),scores.end(),back_inserter(combined),  mem_fun_ref(&ScoreHolder::toPair));

  // Estimate pi0
  PosteriorEstimator::getPValues(combined,pvals);
  pi0 = PosteriorEstimator::estimatePi0(pvals);
  return pi0;

}

void Scores::calcPep() {

  vector<pair<double,bool> > combined;
  transform(scores.begin(),scores.end(),back_inserter(combined),  mem_fun_ref(&ScoreHolder::toPair));
  vector<double> peps;

  // Logistic regression on the data
  PosteriorEstimator::estimatePEP(combined,pi0,peps,true);

  for(size_t ix=0; ix<scores.size(); ix++) {
    (scores[ix]).pPSM->pep = peps[ix];
  }
}
