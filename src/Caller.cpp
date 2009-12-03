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
#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>
#include <cstdlib>
#include <vector>
#include <set>
#include <map>
#include <string>
using namespace std;
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include "Option.h"
#include "SanityCheck.h"
#include "SqtSanityCheck.h"
#include "DataSet.h"
#include "Normalizer.h"
#include "Scores.h"
#include "Normalizer.h"
#include "SetHandler.h"
#include "ssl.h"
#include "Caller.h"
#include "Globals.h"
#include "MSReader.h"
#include "Spectrum.h"
#include "MSToolkitTypes.h"
#include "MassHandler.h"
#include "Enzyme.h"

const unsigned int Caller::xval_fold = 3;

Caller::Caller() : pNorm(NULL), pCheck(NULL), svmInput(NULL),
  modifiedFN(""), modifiedDecoyFN(""), forwardFN(""), decoyFN (""), //shuffledThresholdFN(""), shuffledTestFN(""),
  decoyWC(""), resultFN(""), gistFN(""), tabFN(""), xmloutFN(""), weightFN(""),
  gistInput(false), tabInput(false), dtaSelect(false), docFeatures(false), reportPerformanceEachIteration(false), reportUniquePeptides(false),
  test_fdr(0.01), selectionfdr(0.01), selectedCpos(0), selectedCneg(0), threshTestRatio(0.3), trainRatio(0.6),
  niter(10)
{
}

Caller::~Caller()
{
    if (pNorm)
      delete pNorm;
    pNorm=NULL;
    if (pCheck)
      delete pCheck;
    pCheck=NULL;
    if (svmInput)
      delete svmInput;
    svmInput=NULL;
}

string Caller::extendedGreeter() {
  ostringstream oss;
  char * host = getenv("HOST");
  if (!host)
    host=(char *)"unknown_host";
  oss << greeter();
  oss << "Issued command:" << endl << call;
  oss << "Started " << ctime(&startTime);
  oss.seekp(-1, ios_base::cur);
  oss << " on " << host << endl;
  oss << "Hyperparameters fdr=" << selectionfdr;
  oss << ", Cpos=" << selectedCpos << ", Cneg=" << selectedCneg << ", maxNiter=" << niter << endl;
  return oss.str();
}

string Caller::greeter() {
  ostringstream oss;
  oss << "Percolator version " << VERSION << ", ";
  oss << "Build Date " << __DATE__ << " " << __TIME__ << endl;
  oss << "Copyright (c) 2006-9 University of Washington. All rights reserved." << endl;
  oss << "Written by Lukas Käll (lukall@u.washington.edu) in the" << endl;
  oss << "Department of Genome Sciences at the University of Washington." << endl;
  return oss.str();
}

bool Caller::parseOptions(int argc, char **argv){
  ostringstream callStream;
  callStream << argv[0];
  for (int i=1;i<argc;i++) callStream << " " << argv[i];
  callStream << endl;
  call = callStream.str();
  ostringstream intro,endnote;
  intro << greeter() << endl << "Usage:" << endl;
  intro << "   percolator [options] target.sqt decoy.sqt" << endl;
  intro << "or percolator [options] -P pattern target_and_decoy.sqt" << endl;
  intro << "or percolator [options] -g gist.data gist.label" << endl << endl;
  intro << "   where target.sqt is the target sqt-file," << endl;
  intro << "     and decoy.sqt is the decoy sqt-file," << endl;
  intro << "Small data sets may be merged by replace the sqt-files with meta" << endl;
  intro << "files. Meta files are text files containing the paths of sqt-files, one path" << endl;
  intro << "per line. For successful result, the different runs should be generated under" << endl;
  intro << "similair condition." << endl;
  // init
  CommandLineParser cmd(intro.str());
  cmd.defineOption("o","sqt-out",
    "Create an SQT file with the specified name from the given target SQT file, \
replacing the XCorr value the learned score and Sp with the negated q-value.","filename");
  cmd.defineOption("s","shuffled",
    "Same as -o, but for the decoy SQT file",
    "filename");
  cmd.defineOption("P","pattern",
    "Option for single SQT file mode defining the name pattern used for shuffled data base. \
Typically set to random_seq","pattern");
  cmd.defineOption("p","Cpos",
    "Cpos, penalty for mistakes made on positive examples. Set by cross validation if not specified.",
    "value");
  cmd.defineOption("n","Cneg",
    "Cneg, penalty for mistakes made on negative examples. Set by cross validation if not specified or -p not specified.",
    "value");
  cmd.defineOption("F","trainFDR",
    "False discovery rate threshold to define positive examples in training. Set by cross validation if 0. Default is 0.01.",
    "value");
  cmd.defineOption("t","testFDR",
    "False discovery rate threshold for evaluating best cross validation result and the reported end result. Default is 0.01.",
    "value");
  cmd.defineOption("i","maxiter",
    "Maximal number of iterations",
    "number");
  cmd.defineOption("m","matches",
    "Maximal number of matches to take in consideration per spectrum when using sqt-files",
    "number");
  cmd.defineOption("f","train-ratio",
    "Fraction of the negative data set to be used as train set when only providing one negative set, remaining examples will be used as test set. Set to 0.6 by default.",
    "value");
  cmd.defineOption("G","gist-out",
    "Output the computed features to the given file in tab-delimited format. A file with the features, named <trunc name>.data, and a file with the labels named <trunc name>.label will be created",
    "trunc name");
  cmd.defineOption("g","gist-in",
    "Input files are given as gist files. In this case first argument should be a file name \
of the data file, the second the label file. Labels are interpreted as 1 -- positive train \
and test set, -1 -- negative train set, -2 -- negative in test set.","",TRUE_IF_SET);
  cmd.defineOption("J","tab-out",
    "Output the computed features to the given file in tab-delimited format. A file with the features with the given file name will be created",
    "file name");
  cmd.defineOption("j","tab-in",
    "Input files are given as a tab delimited file. In this case the only argument should be a file name \
of the data file. The tab delimited fields should be id <tab> label <tab> feature1 \
<tab> ... <tab> featureN <tab> peptide <tab> proteinId1 <tab> .. <tab> proteinIdM \
Labels are interpreted as 1 -- positive set \
and test set, -1 -- negative set.\
When the --doc option the first and second feature (third and fourth column) should contain \
the retention time and difference between observed and calculated mass","",TRUE_IF_SET);
  cmd.defineOption("w","weights",
    "Output final weights to the given file",
    "filename");
  cmd.defineOption("W","init-weights",
    "Read initial weights from the given file",
    "filename");
  cmd.defineOption("V","default-direction",
    "The most informative feature given as feature number, can be negated to indicate that a lower value is better.",
    "featureNum");
  cmd.defineOption("v","verbose",
    "Set verbosity of output: 0=no processing info, 5=all, default is 2",
    "level");
  cmd.defineOption("u","unitnorm",
    "Use unit normalization [0-1] instead of standard deviation normalization","",TRUE_IF_SET);
  cmd.defineOption("a","aa-freq","Calculate amino acid frequency features","",TRUE_IF_SET);
  cmd.defineOption("b","PTM","Calculate feature for number of post-translational modifications","",TRUE_IF_SET);
  cmd.defineOption("d","DTASelect",
    "Add an extra hit to each spectra when writing sqt files","",TRUE_IF_SET);
  cmd.defineOption("R","test-each-iteration","Measure performance on test set each iteration","",TRUE_IF_SET);
  cmd.defineOption("Q","quadratic",
    "Calculate quadratic feature terms","",TRUE_IF_SET);
  cmd.defineOption("O","override",
    "Override error check and do not fall back on default score vector in case of suspect score vector",
    "",TRUE_IF_SET);
  cmd.defineOption("y","notryptic",
    "Turn off calculation of tryptic/chymo-tryptic features.","",TRUE_IF_SET);
  cmd.defineOption("c","chymo",
    "Replace tryptic features with chymo-tryptic features.","",TRUE_IF_SET);
  cmd.defineOption("e","elastase",
    "Replace tryptic features with elastase features.","",TRUE_IF_SET);
  cmd.defineOption("N","PNGaseF",
    "Calculate feature based on N-linked glycosylation pattern resulting from a PNGaseF treatment. (N[*].[ST])","",TRUE_IF_SET);
  cmd.defineOption("S","seed",
    "Setting seed of the random number generator. Default value is 0"
    ,"value");
  cmd.defineOption("2","ms2-file",
    "File containing spectra and retention time. The file could be in mzXML, MS2 or compressed MS2 file.",
    "filename");
  cmd.defineOption("M","isotope",
    "Mass difference calculated to closest isotope mass rather than to the average mass.","",TRUE_IF_SET);
  cmd.defineOption("K","klammer",
    "Retention time features calculated as in Klammer et al.","",TRUE_IF_SET);
  cmd.defineOption("D","doc",
    "Include description of correct features.","",MAYBE,"15");
  cmd.defineOption("r","results",
    "Output tab delimited results to a file instead of stdout",
    "filename");
  cmd.defineOption("B","decoy-results",
    "Output tab delimited results for decoys into a file",
    "filename");
  cmd.defineOption("X","xml-output",
    "Output results in xml-format into a file",
    "filename");
  cmd.defineOption("Z","decoy-xml-output",
    "Include decoys PSMs in the xml-output. Only available if -X is used."
    ,"",TRUE_IF_SET);
  cmd.defineOption("U","unique-peptides",
    "Remove all redundant peptides and only keep the highest scoring PSM. q-values and PEPs are only calculated on peptide level in such case"
    ,"",TRUE_IF_SET);

  // finally parse and handle return codes (display help etc...)
  cmd.parseArgs(argc, argv);


  // now query the parsing results
  if (cmd.optionSet("o"))
    modifiedFN = cmd.options["o"];
  if (cmd.optionSet("s"))
    modifiedDecoyFN = cmd.options["s"];
  if (cmd.optionSet("P"))
    decoyWC = cmd.options["P"];
  if (cmd.optionSet("p")) {
    selectedCpos = cmd.getDouble("p",0.0,1e127);
  }
  if (cmd.optionSet("n")) {
    selectedCneg = cmd.getDouble("n",0.0,1e127);
  }
  if (cmd.optionSet("G"))
    gistFN = cmd.options["G"];
  if (cmd.optionSet("g")) {
    gistInput=true;
    if (cmd.arguments.size()!=2) {
      cerr << "Provide exactly two arguments when using gist input" << endl;
      exit(-1);
    }
  }
  if (cmd.optionSet("J"))
    tabFN = cmd.options["J"];
  if (cmd.optionSet("j")) {
    tabInput=true;
    if (cmd.arguments.size()!=1) {
      cerr << "Provide exactly one arguments when using tab delimited input" << endl;
      exit(-1);
    }
  }
  if (cmd.optionSet("w"))
    weightFN = cmd.options["w"];
  if (cmd.optionSet("W"))
    SanityCheck::setInitWeightFN(cmd.options["W"]);
  if (cmd.optionSet("V"))
    SanityCheck::setInitDefaultDir(cmd.getInt("V",-100,100));
  if (cmd.optionSet("f")) {
    double frac = cmd.getDouble("f", 0.0 ,1.0);
    trainRatio=frac;
  }
  if (cmd.optionSet("r"))
    resultFN = cmd.options["r"];
  if (cmd.optionSet("u"))
    Normalizer::setType(Normalizer::UNI);
  if (cmd.optionSet("d"))
    dtaSelect=true;
  if (cmd.optionSet("Q"))
    DataSet::setQuadraticFeatures(true);
  if (cmd.optionSet("O"))
    SanityCheck::setOverrule(true);
  if (cmd.optionSet("y"))
    Enzyme::setEnzyme(Enzyme::NO_ENZYME);
  if (cmd.optionSet("R"))
    reportPerformanceEachIteration=true;
  if (cmd.optionSet("e"))
    Enzyme::setEnzyme(Enzyme::ELASTASE);
  if (cmd.optionSet("c"))
    Enzyme::setEnzyme(Enzyme::CHYMOTRYPSIN);
  if (cmd.optionSet("N"))
    DataSet::setPNGaseF(true);
  if (cmd.optionSet("a"))
    DataSet::setAAFreqencies(true);
  if (cmd.optionSet("b"))
    DataSet::setPTMfeature(true);
  if (cmd.optionSet("i")) {
    niter = cmd.getInt("i",0,100000000);
  }
  if (cmd.optionSet("m")) {
    int m = cmd.getInt("m",1,30000);
    DataSet::setHitsPerSpectrum(m);
  }
  if (cmd.optionSet("v")) {
    Globals::getInstance()->setVerbose(cmd.getInt("v",0,10));
  }
  if (cmd.optionSet("F")) {
    selectionfdr = cmd.getDouble("F",0.0,1.0);
  }
  if (cmd.optionSet("t")) {
    test_fdr = cmd.getDouble("t",0.0,1.0);
  }
  if (cmd.optionSet("S")) {
    Scores::setSeed(cmd.getInt("S",0,20000));
  }
  if (cmd.optionSet("2")) {
    spectrumFile = cmd.options["2"];
  }
  if (cmd.optionSet("B"))
    decoyOut = cmd.options["B"];
  if (cmd.optionSet("M"))
    MassHandler::setMonoisotopicMass(true);
  if (cmd.optionSet("K"))
    DescriptionOfCorrect::setKlammer(true);
  if (cmd.optionSet("D")) {
    docFeatures = true;
    DataSet::setCalcDoc(true);
    DescriptionOfCorrect::setDocType(cmd.getInt("D",0,15));
  }
  if (cmd.optionSet("X"))
    xmloutFN = cmd.options["X"];
  if (cmd.optionSet("Z")) {
	if (xmloutFN.empty()) {
		cerr << "The -Z switch was set without any xml-output file specified" << stderr;
	    exit(-1);
	}
    Scores::setOutXmlDecoys(true);
  }
  if (cmd.optionSet("U")) {
    if (!modifiedFN.empty() || !modifiedDecoyFN.empty()) {
		cerr << "The -U switch may not be used together with the -o and -s options" << stderr;
	    exit(-1);
    }
	reportUniquePeptides = true;
  }

  if (cmd.arguments.size()>2) {
      cerr << "Too many arguments given" << endl;
      cmd.help();
  }
  if (cmd.arguments.size()==0) {
      cerr << "No arguments given" << endl;
      cmd.help();
  }
  if (cmd.arguments.size()>0)
     forwardFN = cmd.arguments[0];
  if (cmd.arguments.size()>1)
     decoyFN = cmd.arguments[1];
  return true;
}

void Caller::readRetentionTime(string filename) {
  MSReader r;
  Spectrum s;

  r.setFilter(MS2);

  char* cstr = new char [filename.size()+1];
  strcpy (cstr, filename.c_str());
  r.readFile(cstr,s);

  while (s.getScanNumber()!=0){
    scan2rt[s.getScanNumber()] = (double) s.getRTime();
    r.readFile(NULL,s);
  }
  delete [] cstr;
}

void Caller::printWeights(ostream & weightStream, vector<double>& w) {
  weightStream << "# first line contains normalized weights, second line the raw weights" << endl;
  weightStream << DataSet::getFeatureNames().getFeatureNames() << "\tm0" << endl;
  weightStream.precision(3);
  weightStream << w[0];
  for(unsigned int ix=1;ix<FeatureNames::getNumFeatures()+1;ix++) {
    weightStream << "\t" << w[ix];
  }
  weightStream << endl;
  vector<double> ww(FeatureNames::getNumFeatures()+1);
  pNorm->unnormalizeweight(w,ww);
  weightStream << ww[0];
  for(unsigned int ix=1;ix<FeatureNames::getNumFeatures()+1;ix++) {
    weightStream << "\t" << ww[ix];
  }
  weightStream << endl;
}


void Caller::filelessSetup(const unsigned int numFeatures, const unsigned int numSpectra, char ** featureNames, double pi0) {
  pCheck = new SanityCheck();
  normal.filelessSetup(numFeatures, numSpectra,1);
  shuffled.filelessSetup(numFeatures, numSpectra,-1);
  for (unsigned int ix=0;ix<numFeatures;ix++){
    string fn = featureNames[ix];
    DataSet::getFeatureNames().insertFeature(fn);
  }
}

void Caller::readFiles(bool &doSingleFile) {
  if (gistInput) {
    pCheck = new SanityCheck();
    normal.readGist(forwardFN,decoyFN,1);
    shuffled.readGist(forwardFN,decoyFN,-1);
  } else if (tabInput) {
    pCheck = new SanityCheck();
    normal.readTab(forwardFN,1);
    shuffled.readTab(forwardFN,-1);
  } else if (!doSingleFile) {
    pCheck = new SqtSanityCheck();
    normal.readFile(forwardFN,1);
    shuffled.readFile(decoyFN,-1);
  } else {
    pCheck = new SqtSanityCheck();
    normal.readFile(forwardFN,decoyWC,false);
    shuffled.readFile(forwardFN,decoyWC,true);
  }
  if (spectrumFile.size()>0)
    readRetentionTime(spectrumFile);
}

int Caller::xv_step(vector<vector<double> >& w, bool updateDOC) {
  // Setup
  struct options *Options = new options;
  Options->lambda=1.0;
  Options->lambda_u=1.0;
  Options->epsilon=EPSILON;
  Options->cgitermax=CGITERMAX;
  Options->mfnitermax=MFNITERMAX;

  struct vector_double *Weights = new vector_double;
  Weights->d = FeatureNames::getNumFeatures()+1;
  Weights->vec = new double[Weights->d];

  vector<vector<double> > best_w(xval_fold);
  int estTP = 0;

  for (unsigned int set=0;set<xval_fold;++set) {
    int bestTP = 0;
    double best_cpos=1,best_cneg=1;

    if(VERB>2) cerr << "cross calidation - fold " << set+1 << " out of " << xval_fold << endl;
    vector<double> ww = w[set];
    xv_train[set].calcScores(ww,selectionfdr);
    if (docFeatures && updateDOC)
      xv_train[set].recalculateDescriptionOfGood(selectionfdr);
    xv_train[set].generateNegativeTrainingSet(*svmInput,1.0);
    xv_train[set].generatePositiveTrainingSet(*svmInput,selectionfdr,1.0);
    if (VERB>2) cerr << "Calling with " << svmInput->positives << " positives and " << svmInput->negatives << " negatives\n";

    struct vector_double *Outputs = new vector_double;
    Outputs->vec = new double[svmInput->positives+svmInput->negatives];
    Outputs->d = svmInput->positives+svmInput->negatives;

    vector<double>::iterator cpos,cfrac;
    for(cpos=xv_cposs.begin();cpos!=xv_cposs.end();cpos++) {
      for(cfrac=xv_cfracs.begin();cfrac!=xv_cfracs.end();cfrac++) {
        if(VERB>2) cerr << "-cross validation with cpos=" << *cpos <<
          ", cfrac=" << *cfrac << endl;
        int tp=0;

        for(int ix=0;ix<Weights->d;ix++) Weights->vec[ix]=0;
        for(int ix=0;ix<Outputs->d;ix++) Outputs->vec[ix]=0;
        svmInput->setCost(*cpos,(*cpos)*(*cfrac));

        L2_SVM_MFN(*svmInput,Options,Weights,Outputs);

        for(int i= FeatureNames::getNumFeatures()+1;i--;)
          ww[i]=Weights->vec[i];

        tp = xv_train[set].calcScores(ww,test_fdr);
        if(VERB>2) cerr << "- cross validation estimates " << tp << " target PSMs over " << test_fdr*100 << "% FDR level" << endl;
        if (tp>=bestTP) {
          if(VERB>2) cerr << "Better than previous result, store this" << endl;
          bestTP = tp;
          best_w[set] = ww;
          best_cpos=*cpos;
          best_cneg=(*cpos)*(*cfrac);
        }
      }
      if(VERB>2) cerr << "cross validation estimates " << bestTP/(xval_fold-1) << " target PSMs with q<" << test_fdr << " for hyperparameters Cpos=" << best_cpos
                  << ", Cneg=" << best_cneg << endl;
    }
    estTP += bestTP;
    delete [] Outputs->vec;
    delete Outputs;
  }
  w = best_w;

  delete [] Weights->vec;
  delete Weights;
  delete Options;

  return estTP/(xval_fold-1);
}

void Caller::train(vector<vector<double> >& w) {
  // iterate
  for(unsigned int i=0;i<niter;i++) {
    if(VERB>1) cerr << "Iteration " << i+1 << " : ";
    int tar = xv_step(w,true);
    if (VERB>1) {
        cerr << "After the iteration step, " << tar << " target PSMs with q<"
             << selectionfdr << " were estimated by cross validation" << endl;
    }

    if(VERB>2) {cerr<<"Obtained weights" << endl; for (size_t set=0;set<xval_fold;++set) printWeights(cerr,w[set]);}
  }
  if(VERB==2 ) {
    cerr << "Obtained weights (only showing weights of first cross validation set)" << endl; printWeights(cerr,w[0]);
  }
  int tar = 0;
  for (size_t set=0;set<xval_fold;++set) {
    if (docFeatures) {
       xv_test[set].getDOC().copyDOCparameters(xv_train[set].getDOC());
       xv_test[set].setDOCFeatures();
    }
    tar += xv_test[set].calcScores(w[set],test_fdr);
  }
  if(VERB>0) {
    cerr << "After all training done, " << tar << " target PSMs with q<"
         << test_fdr << " were found when measuring on the test set" << endl;
  }
}


void Caller::fillFeatureSets() {
  fullset.fillFeatures(normal,shuffled);
  if (VERB>1) {
    cerr << "Train/test set contains " << fullset.posSize() << " positives and " << fullset.negSize() << " negatives, size ratio="
         << fullset.factor << " and pi0=" << fullset.pi0 << endl;
  }
  //Normalize features
  set<DataSet *> all;
  all.insert(normal.getSubsets().begin(),normal.getSubsets().end());
  all.insert(shuffled.getSubsets().begin(),shuffled.getSubsets().end());
  if (docFeatures) {
    for (set<DataSet *>::iterator myset=all.begin();myset!=all.end();++myset)
      (*myset)->setRetentionTime(scan2rt);
  }
  if (gistFN.length()>0) {
    SetHandler::gistWrite(gistFN,normal,shuffled);
  }
  if (tabFN.length()>0) {
    SetHandler::writeTab(tabFN,normal,shuffled);
  }
  // Move the features into a format suitable for the normalizer
  vector<double *> featuresV, rtFeaturesV;
  double * features;
  PSMDescription* pPSM;
  size_t ix;
  set<DataSet *>::iterator it;
  for (it=all.begin();it!=all.end();++it) {
    int ixPos=-1;
    while((pPSM=(*it)->getNext(ixPos))!=NULL) {
      features = pPSM->features;
      featuresV.push_back(features);
      if (features = pPSM->retentionFeatures)
        rtFeaturesV.push_back(features);
    }
  }
  pNorm=Normalizer::getNormalizer();
  pNorm->setSet(featuresV,rtFeaturesV,FeatureNames::getNumFeatures(),docFeatures?RTModel::totalNumRTFeatures():0);
  pNorm->normalizeSet(featuresV,rtFeaturesV);
}

int Caller::preIterationSetup(vector<vector<double> >& w) {

  svmInput = new AlgIn(fullset.size(),FeatureNames::getNumFeatures()+1); // One input set, to be reused multiple times

  if (selectedCpos<=0 || selectedCneg <= 0) {
    xv_train.resize(xval_fold); xv_test.resize(xval_fold);
    fullset.createXvalSets(xv_train,xv_test,xval_fold);
    if (selectionfdr<=0.0) selectionfdr=test_fdr;
    if (selectedCpos > 0) {
      xv_cposs.push_back(selectedCpos);
    } else {
      xv_cposs.push_back(10);xv_cposs.push_back(1);xv_cposs.push_back(0.1);
      if(VERB>0) cerr << "selecting cpos by cross validation" << endl;
    }
    if (selectedCpos > 0 && selectedCneg > 0) {
      xv_cfracs.push_back(selectedCneg/selectedCpos);
    } else  {
      xv_cfracs.push_back(1.0*fullset.factor);xv_cfracs.push_back(3.0*fullset.factor);xv_cfracs.push_back(10.0*fullset.factor);
      if(VERB>0) cerr << "selecting cneg by cross validation" << endl;
    }
    return pCheck->getInitDirection(xv_test,xv_train,pNorm,w,test_fdr);
   } else {
    vector<Scores> myset(1,fullset);
    return pCheck->getInitDirection(myset,myset,pNorm,w,test_fdr);
  }
}

int Caller::run() {
  time(&startTime);
  startClock=clock();
  if(VERB>0)  cerr << extendedGreeter();
  //File reading
  bool doSingleFile = !decoyWC.empty();
  readFiles(doSingleFile);
  fillFeatureSets();
  vector<vector<double> > w(xval_fold,vector<double>(FeatureNames::getNumFeatures()+1)),ww;
  int firstNumberOfPositives = preIterationSetup(w);
  if(VERB>0) cerr << "Estimating " << firstNumberOfPositives << " over q=" << test_fdr << " in initial direction" << endl;


  // Set up a first guess of w
  time_t procStart;
  clock_t procStartClock=clock();
  time (&procStart);
  double diff = difftime (procStart,startTime);

  if (VERB>1) cerr << "Reading in data and feature calculation took " <<
    ((double)(procStartClock-startClock))/(double)CLOCKS_PER_SEC <<
    " cpu seconds or " << diff << " seconds wall time" << endl;

  if(VERB>0) { cerr << "---Training with Cpos";
  	           if (selectedCpos>0) cerr << "=" << selectedCpos;
  	           else cerr << " selected by cross validation";
  	           cerr << ", Cneg";
  	           if (selectedCneg>0) cerr << "=" << selectedCneg;
  	           else cerr << " selected by cross validation";
  	           cerr << ", fdr=" << selectionfdr << endl;
  }

  train(w);

  if (!pCheck->validateDirection(w))
    fullset.calcScores(w[0]);
  if(VERB>0) cerr << "Merging results from " << xv_test.size() << " datasets" << endl;
  fullset.merge(xv_test);
  if (reportUniquePeptides) {
	if(VERB>0) cerr << "Tossing out \"redundant\" PSMs keeping only the best scoring PSM for each unique peptide." << endl;
    fullset.weedOutRedundant();
  }

  fullset.estimatePi0();
  if(VERB>0) cerr << "Selecting pi_0=" << fullset.getPi0() << endl;
  if(VERB>0) cerr << "Calibrating statistics - calculating q values" << endl;
  int foundPSMs = fullset.calcQ(test_fdr);
  if(VERB>0 && docFeatures) {
	cerr << "For the cross validation sets the average deltaMass are ";
	for(size_t ix=0;ix<xv_test.size();ix++)
      cerr << xv_test[ix].getDOC().getAvgDeltaMass() << " ";
	cerr << "and average pI are ";
	for(size_t ix=0;ix<xv_test.size();ix++)
	    cerr << xv_test[ix].getDOC().getAvgPI() << " ";
	cerr << endl;
  }
  if(VERB>0) cerr << "New pi_0 estimate on merged list gives " << foundPSMs << (reportUniquePeptides?" peptides":" PSMs") << " over q=" << test_fdr << endl;
  if(VERB>0) cerr << "Calibrating statistics - calculating Posterior error probabilities (PEPs)" << endl;
  fullset.calcPep();

  time_t end;
  time (&end);
  diff = difftime (end,procStart);

  ostringstream timerValues;
  timerValues.precision(4);
  timerValues << "Processing took " << ((double)(clock()-procStartClock))/(double)CLOCKS_PER_SEC;
  timerValues << " cpu seconds or " << diff << " seconds wall time" << endl;
  if (VERB>1) cerr << timerValues.str();
  normal.modifyFile(modifiedFN,fullset,extendedGreeter()+timerValues.str(), dtaSelect);
  shuffled.modifyFile(modifiedDecoyFN,fullset,extendedGreeter()+timerValues.str(), dtaSelect);

  if (weightFN.size()>0) {
     ofstream weightStream(weightFN.data(),ios::out);
     for (unsigned int ix=0;ix<xval_fold;++ix)
       printWeights(weightStream,w[ix]);
     weightStream.close();
  }
  if (xmloutFN.size()>0) {
	ofstream xmlStream(xmloutFN.data(),ios::out);
	writeXML(xmlStream,fullset);
	xmlStream.close();
  }

  if (resultFN.empty()) {
	 normal.print(fullset);
  } else {
     ofstream targetStream(resultFN.data(),ios::out);
     normal.print(fullset,targetStream);
     targetStream.close();
  }
  if (!decoyOut.empty()) {
     ofstream decoyStream(decoyOut.data(),ios::out);
     shuffled.print(fullset,decoyStream);
     decoyStream.close();
  }
  if (docFeatures) {
  	ofstream outs("retention_times.txt",ios::out);
    for (unsigned int set=0;set<xval_fold;++set) {
      xv_test[set].printRetentionTime(outs,test_fdr);
    }
    outs.close();
  }

  return 0;
}

void Caller::writeXML(ostream & os,Scores & fullset) {
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
  os << "<percolator_output majorVersion=\"1\" minorVersion=\"1\" percolator_version=\"" <<
		  "Percolator version " << VERSION << "\" " <<
		  "xsi:schemaLocation=\"http://noble.gs.washington.edu/proj/percolator/model/percolator_out percolator_out.xsd\" " <<
		  "xmlns=\"http://noble.gs.washington.edu/proj/percolator/model/percolator_out\" " <<
		  "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">" << endl;
  os << "  <process_info>" << endl;
  os << "    <command_line>" << call << "</command_line>" << endl;
  os << "    <pi_0>" << fullset.getPi0() << "</pi_0>" << endl;
  if (docFeatures) {
    os << "    <average_delta_mass>" << fullset.getDOC().getAvgDeltaMass() << "</average_delta_mass>" << endl;
    os << "    <average_pi>" << fullset.getDOC().getAvgPI() << "</average_pi>" << endl;
  }
  os << "  </process_info>" << endl;
  for(vector<ScoreHolder>::iterator psm=fullset.begin();psm!=fullset.end();++psm) {
	os << *psm;
  }
  os << "</percolator_output>" << endl;


}

