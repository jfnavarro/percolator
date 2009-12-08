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

 *****************************************************************************/

#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <math.h>
#include "Globals.h"
#include "RTModel.h"
#include "DataSet.h"
#include "DescriptionOfCorrect.h"

// by default, 3-fold cross validation is used
static double DEFAULT_K = 3;

// default values for C, gamma and epsilon
// they are used when gType = NO_GRID, except for gamma which is initialized with 1/n
static double INITIAL_C = 5.0;
static double INITIAL_GAMMA  = 0.5;
static double INITIAL_EPSILON = 1e-2;

// for gType = FINE_GRID, a coarse grid is used combined with a fine grid
static double COARSE_GRID_GAMMA[] =  { pow(2, -15), pow(2, -13), pow(2, -11), pow(2, -9), pow(2, -7), pow(2, -5),
									   pow(2, -3), pow(2, -1), pow(2, 1), pow(2, 3), pow(2, 5)};
static double COARSE_GRID_C[] = { pow(2, -5), pow(2, -3), pow(2, -1), pow(2, 1), pow(2, 3), pow(2, 5), pow(2, 7),
								  pow(2, 9), pow(2, 11), pow(2, 13)};
static double COARSE_GRID_EPSILON[] = {INITIAL_EPSILON / 10, INITIAL_EPSILON, INITIAL_EPSILON * 10};
// no of points smaller than the parameter value found using the coarse grid (the fine grid will include NO_POINTS_FINE_GRID*2 for one parameter)
static int NO_POINTS_FINE_GRID = 7;
// step is give as the exponent of 2 (the actual step value is 2^(STEP_FINE_GRID))
static double STEP_FINE_GRID = 0.25;

// for gType = NORMAL_GRID
static double NORMAL_GRID_GAMMA[] =  { pow(2, -8), pow(2, -7), pow(2, -6), pow(2, -5), pow(2, -4), pow(2, -3),
									   pow(2, -2), pow(2, -1), pow(2, 0), pow(2, 1)};
static double NORMAL_GRID_C[] = { pow(2, -2), pow(2, -1), pow(2, 0), pow(2, 1), pow(2, 2), pow(2, 3), pow(2, 4),
								  pow(2, 5), pow(2, 6), pow(2, 7)};
static double NORMAL_GRID_EPSILON[] = {INITIAL_EPSILON/10, INITIAL_EPSILON, INITIAL_EPSILON*10};

// amino acids alphabet
string RTModel::aaAlphabet = "ACDEFGHIKLMNPQRSTVWY";
//characters indicating post translational modifications in the ms2 file
string RTModel::ptmAlphabet = "#*@";
bool RTModel::doKlammer = false;

// hydrophobicity indices
float RTModel::krokhin_index['Z'-'A'+1] =
         {1.1, 0.0, 0.45, 0.15, 0.95,  10.9, -0.35, -1.45, 8.0, 0.0, -2.05, 9.3, 6.2,
          -0.85,0.0,2.1,-0.4,-1.4,-0.15,0.65,0.0,5.0,12.25,0.0,4.85,0.0};
float RTModel::krokhin100_index['Z'-'A'+1] =
         {1,0,0.1,0.15,1,11.67,-0.35,-3.0,7.95,0,-3.4,9.4,6.25,
         -0.95,0,1.85,-0.6,-2.55,-0.15,0.65,0,4.7,13.35,0,5.35,0};
float RTModel::krokhinC2_index['Z'-'A'+1] =
         {0.5,0.0,0.2,0.4,0,9.5,0.15,-0.2,6.6,0.0,-1.5,7.4,5.7,-0.2,0.0,
          2.1,-0.2,-1.1,-0.1,0.6,0.0,3.4,11.8,0.0,4.5,0.0};
float RTModel::krokhinTFA_index['Z'-'A'+1] =
         {1.11,0.0,0.04,-0.22,1.08,11.34,-0.35,-3.04,7.86,0.0,-3.53,9.44,6.57,
          -1.44,0.0,1.62,-0.53,-2.58,-0.33,0.48,0.0,4.86,13.12,0.0,5.4,0.0};
// negated hessa scale
float RTModel::hessa_index['Z'-'A'+1] =
         {-0.11,-0.0,0.13,-3.49,-2.68,0.32,-0.74,-2.06,0.60,0.0,-2.71,0.55,0.10,-2.05,
          0.0,-2.23,-2.36,-2.58,-0.84,-0.52,0.0,0.31,-0.30,0.0,-0.68,0.0};
float RTModel::kytedoolittle_index['Z'-'A'+1] =
         {1.80,0.0,2.50,-3.50,-3.50,2.80,-0.40,-3.20,4.50,0.0,-3.90,3.80,1.90,-3.50,
          0.0,-1.60,-3.50,-4.50,-0.80,-0.70,0.0,4.20,-0.90,0.0,-1.30,0.0};
float RTModel::aa_weights['Z'-'A'+1] =
		{71.0788,0, 103.1448,115.0886,129.1155,147.1766,57.052,137.1412,113.1595,0,128.1742,113.1595,131.1986,114.1039,
		 0,97.1167,128.1308,156.1876,87.0782,101.1051,0,99.1326,186.2133,0,163.1760,0};
float RTModel::bulkiness['Z'-'A'+1] =
		{ 11.50, 0.0, 13.46, 11.68, 13.57, 19.80, 3.40, 13.69, 21.40, 0.0, 15.71, 21.40, 16.25,
		  12.82, 0.0, 17.43, 14.45, 14.28, 9.47, 15.77, 0.0, 21.57, 21.67, 0.0, 18.03, 0.0};
/*// conformational preferences of aa
float RTModel::alpha_helix['Z'-'A'+1] =
		{ 1.41, 0.0, 0.66, 0.99, 1.59, 1.16, 0.43, 1.05, 1.09, 0.0, 1.23, 1.34, 1.30, 0.76,
		  0.0, 0.34, 1.27, 1.21, 0.57, 0.76, 0.0, 0.90, 1.02, 0.0, 0.74, 0.0};
float RTModel::beta_sheet['Z'-'A'+1] =
		{ 0.72, 0.0, 1.40, 0.39, 0.52, 1.33, 0.58, 0.80,  1.67,  0.0, 0.69, 1.22, 1.14, 0.48,
		  0.0, 0.31, 0.98, 0.84, 0.96, 1.17, 0.0, 1.87, 1.35, 0.0, 1.45, 0.0 };*/

// groups of features that can be switched on or off
string RTModel::feature_groups[NO_FEATURE_GROUPS] =
		{ "krokhin_index", "krokhin100_index", "krokhinc2_index", "krokhintfa_index", "doolittle_index",
		  "hessa_index", "peptide_size", "no_ptms", "ptms", "bulkiness", "no_consec_krdenq", "aa_features" };
// number of features in each group
int RTModel::no_features_per_group[NO_FEATURE_GROUPS] = {16, 16, 16, 16, 16, 16, 1, 1, 3, 1, 1, aaAlphabet.size()};
// groups of features to be used
//(krokhin, krokhintfa, doolittle = 25), peptide_size(=64), ptms (=256),  bulkiness(=2^9 = 512), no_consec_krdenq (1024),
//aa(2048)
static int DEFAULT_FEATURE_GROUPS = 25 + 64 + 256 + 512 + 1024 + 2048;

RTModel::RTModel(): model(NULL), c(INITIAL_C), gamma(INITIAL_GAMMA), epsilon(INITIAL_EPSILON), stepFineGrid(STEP_FINE_GRID),
                    noPointsFineGrid(NO_POINTS_FINE_GRID), calibrationFile(""),	saveCalibration(false), k(DEFAULT_K),
                    gType(NORMAL_GRID), eType(K_FOLD_CV), selected_features(DEFAULT_FEATURE_GROUPS)
{
	grids.gridGamma.assign(NORMAL_GRID_GAMMA, NORMAL_GRID_GAMMA + sizeof(NORMAL_GRID_GAMMA) / sizeof (NORMAL_GRID_GAMMA[0]));
	grids.gridC.assign(NORMAL_GRID_C, NORMAL_GRID_C + sizeof(NORMAL_GRID_C) / sizeof (NORMAL_GRID_C[0]));
	grids.gridEpsilon.assign(NORMAL_GRID_EPSILON, NORMAL_GRID_EPSILON + sizeof(NORMAL_GRID_EPSILON) / sizeof (NORMAL_GRID_EPSILON[0]));
	noFeaturesToCalc = 0;
	// compute the total number of features depending on the selected feature groups
	for(int i = 0; i < NO_FEATURE_GROUPS; i++)
			if (selected_features & 1 << i)
				noFeaturesToCalc += no_features_per_group[i];
}

RTModel::~RTModel()
{
}

size_t RTModel::totalNumRTFeatures()
{
	return (doKlammer?64:minimumNumRTFeatures() + aaAlphabet.size());
}

void RTModel::setDoKlammer(const bool switchKlammer)
{
	doKlammer = switchKlammer;
}

void RTModel::setSelectFeatures(const int sf)
{
	selected_features = sf;
	noFeaturesToCalc = 0;
	for(int i = 0; i < NO_FEATURE_GROUPS; i++)
			if (selected_features & 1 << i)
				noFeaturesToCalc += no_features_per_group[i];
}

double RTModel::getNoPtms(string pep)
{
	double ptms = 0;
  	string::size_type posP = 0;

  	while (posP < ptmAlphabet.length())
  	{
   		string::size_type pos = 0;
    	while ( (pos = pep.find(ptmAlphabet[posP], pos)) != string::npos )
     	{
      		pep.replace( pos, 1, "");
      		++pos;
      		++ptms;
    	}
    	++posP;
  	}
  	return ptms;
}

// Functions to compute retention features
// adds 3 features, corresponding to the number of 3 types of ptms in the peptide
double* RTModel::fillPTMFeatures(const string& pep, double *feat)
{
	string::size_type pos = ptmAlphabet.size();

  	for ( ; pos--; )
  		feat[pos] = 0.0;

  	for (string::const_iterator it = pep.begin(); it != pep.end(); it++)
  	{
    	pos = ptmAlphabet.find(*it);
    	if (pos != string::npos)
    		feat[pos]++;
    }

  	return feat + ptmAlphabet.size();
}

// most and least hydrophobic patches
double* RTModel::amphipathicityHelix(const float *index, const string& peptide, double *features)
{
	double min = 0.0, max = 0.0, hWindow = 0.0;
	int n = peptide.length();
	double cos300, cos400;
	// value to be added to the peptides < 9
	double cst;

	cos300 = cos(300 * M_PI / 180);
	cos400 = cos(400 * M_PI / 180);

	// calculate the average of hydrophobicity of the index
	double avgHydrophobicityIndex = 0.0;
	for(int i = 0; i < (('Z' - 'A') + 1); ++i)
		avgHydrophobicityIndex += index[i];
	avgHydrophobicityIndex = avgHydrophobicityIndex / aaAlphabet.size();
	cst = avgHydrophobicityIndex * (1 + 2 * cos300 + 2 * cos400);

	// if the peptide is too short, use cst
	if (n < 9)
	{
		*(features++) = cst;
	  	*(features++) = cst;
	}
	else
	{
		// min (maximum) - initialized with the max(min) possible value
		for(int i = 4; i <= (peptide.length()-5); ++i)
		{

			hWindow = index[peptide[i] - 'A'] +
					  (cos300 * (index[peptide[i-3] - 'A'] + index[peptide[i+3] - 'A'])) +
					  (cos400 * (index[peptide[i-4] - 'A'] + index[peptide[i+4] - 'A']));
			if (i == 4)
			{
				min = hWindow;
				max = hWindow;
			}
			else
			{
				if (hWindow < min)
					min = hWindow;
				if (hWindow > max)
					max = hWindow;
			}

		}
		*(features++) = min;
		*(features++) = max;
    }

	return features;
}

// bulkiness
double RTModel::bulkinessSum(const string& peptide)
{
	double sum = 0.0;
	string::const_iterator token = peptide.begin();

  	for(;token != peptide.end(); ++token)
    	sum += bulkiness[*token - 'A'];

  	return sum;
}

// hydrophobic moment (angle = 100 for helices, = 180 for sheets)
double*  RTModel::hydrophobicMoment(const float *index, const string& peptide, const double angleDegrees, const int w, double * features)
{
	double sum1 = 0.0, sum2 = 0.0;
	int lengthPeptide = peptide.length();
	double minHMoment, maxHMoment, hMoment;
	// calculate the angle in radians
	double angle = angleDegrees * M_PI / 180;

	for(int i = 0; i < min(lengthPeptide, w); ++i)
	{
		sum1 += index[peptide[i] - 'A']*sin((i+1) * angle);
		sum2 += index[peptide[i] - 'A']*cos((i+1) * angle);
	}

	hMoment = sqrt((sum1 * sum1) + (sum2 * sum2));
	minHMoment = hMoment;
	maxHMoment = hMoment;

	if (lengthPeptide > w)
	{
		for(int i = 1; i <= lengthPeptide - w; ++i)
		{
			sum1 = 0.0;
			sum2 = 0.0;
			for(int j = 0; j < w; j++)
			{
				sum1 += index[peptide[i+j] - 'A']*sin((j+1) * angle);
				sum2 += index[peptide[i+j] - 'A']*cos((j+1) * angle);
			}
			hMoment = sqrt((sum1 * sum1) + (sum2 * sum2));

			if (hMoment > maxHMoment)
				maxHMoment = hMoment;
			if (hMoment < minHMoment)
				minHMoment = hMoment;
		}
	}

	*(features++) = minHMoment;
	*(features++) = maxHMoment;

	return features;
}

// calculate the number of consecutive occurences of (R,K,D,E,N,Q)
int RTModel::noConsecKRDENQ(const string& peptide)
{
	double noOccurences = 0.0;
	string AA = "RKDENQ";
	size_t isAA1, isAA2;

  	for(unsigned int ix = 0; ix < (peptide.size() - 1); ++ix)
  	{
  		// are the current aa and his neighbour one of RKDENQ?
  		isAA1 = AA.find(peptide[ix]);
  		isAA2 = AA.find(peptide[ix + 1]);
  		if ((isAA1 != string::npos) && (isAA2 != string::npos))
  		{
  			noOccurences++;
    	}
  	}

  	return noOccurences;
}

// function used by percolator
void RTModel::fillFeaturesAllIndex(const string& pep, double *features)
{
	// calculate the number of posttranslational modifications
	unsigned int ptms = 0;
  	string peptide = pep;
  	string::size_type posP = 0;

  	while (posP < ptmAlphabet.length())
  	{
   		string::size_type pos = 0;
    	while ( (pos = peptide.find(ptmAlphabet[posP], pos)) != string::npos )
     	{
      		peptide.replace( pos, 1, "");
      		++pos;
      		++ptms;
    	}
    	++posP;
  	}

	if(doKlammer)
	{
		// Klammer et al. features
		features = fillAAFeatures(peptide, features);
		 features = fillAAFeatures(peptide.substr(0,1), features);
		 features = fillAAFeatures(peptide.substr(peptide.size()-2,1), features);
		 char Ct = peptide[peptide.size()-1];
		 *(features++) = ((Ct=='K' || Ct== 'R')?1.0:0.0);
		 *(features++) = peptide.size();
		 *(features++) = indexSum(aa_weights,peptide)+1.0079+17.0073; //MV
    }
  	else
  	{
  		// fill in the features characteristic to each of the three hydrophobicity indices
  		features = fillFeaturesIndex(peptide, krokhin_index, features);
  		features = fillFeaturesIndex(peptide, krokhin100_index, features);
  		features = fillFeaturesIndex(peptide, krokhinTFA_index, features);
  		*(features++) = peptide.size();
  		*(features++) = (double) ptms;
  		// fill all the aa features
  		features = fillAAFeatures(peptide, features);
  	}
}

// calculate the sum of hydrophobicities of all aa in the peptide
double RTModel::indexSum(const float* index, const string& peptide)
{
	double sum = 0.0;
	string::const_iterator token = peptide.begin();

  	for(;token != peptide.end(); ++token)
    	sum += index[*token - 'A'];

  	return sum;
}

// calculate the average of hydrophobicities of all aa in the peptide
double RTModel::indexAvg(const float* index, const string& peptide)
{
	double sum = 0.0;
  	string::const_iterator token = peptide.begin();

  	for(;token != peptide.end(); ++token)
    	sum += index[*token - 'A'];

  	return sum / (double) peptide.size();
}


// calculate the sum of hydrophobicities of neighbours of R(Argenine) and K (Lysine)
double RTModel::indexNearestNeigbourPos(const float* index, const string& peptide)
{
	double sum = 0.0;

  	for(unsigned int ix = 0; ix < peptide.size(); ++ix)
  	{
  		if (peptide[ix]=='R' || peptide[ix]=='K')
  		{
      		if (ix > 0)
        		sum += max(0.0f, index[peptide[ix - 1] - 'A']);
      		if (ix < peptide.size() - 1)
        		sum += max(0.0f, index[peptide[ix + 1] - 'A']);
    	}
  	}

	return sum;
}

// calculate the sum of hydrophobicities of neighbours of D(Aspartate) and E(Glutamate)
double RTModel::indexNearestNeigbourNeg(const float* index, const string& peptide)
{
	double sum = 0.0;

  	for(unsigned int ix = 0; ix < peptide.size(); ++ix)
  	{
  		if (peptide[ix] == 'D' || peptide[ix] == 'E')
  		{
      		if (ix > 0)
        		sum += max(0.0f, index[peptide[ix - 1] - 'A']);
      		if (ix < peptide.size() - 1)
        		sum += max(0.0f, index[peptide[ix + 1] - 'A']);
    	}
  	}

  	return sum;
}

// hydrophobicity of the n-terminus
inline double RTModel::indexN(const float *index, const string& peptide)
{
	return index[peptide[0] - 'A'];
}

// hydrophobicity of the c-terminus
inline double RTModel::indexC(const float *index, const string& peptide)
{
	return index[peptide[peptide.size() - 1] - 'A'];
}

// product between hydrophobicity of n- and c- terminus
inline double RTModel::indexNC(const float *index, const string& peptide)
{
	double n = max(0.0, indexN(index, peptide));
	double c = max(0.0, indexC(index, peptide));

	return n * c;
}

// the most and least hydrophobic windows
double* RTModel::indexPartialSum(const float* index, const string& peptide, const size_t win, double *features)
{
	double sum = 0.0;
  	size_t window = min(win,peptide.size()-1);
  	string::const_iterator lead = peptide.begin(), lag = peptide.begin();

  	for( ; lead != (peptide.begin() + window); ++lead)
    	sum += index[*lead-'A'];

  	double minS = sum, maxS = sum;

  	for( ; lead != peptide.end(); ++lead, ++lag)
  	{
    	sum += index[*lead - 'A'];
    	sum -= index[*lag - 'A'];
    	minS = min(sum, minS);
    	maxS = max(sum, maxS);
  	}

  	*(features++) = maxS;
  	*(features++) = minS;

  return features;
}

// adds 20 features, each including the number of an aa in the peptide
double* RTModel::fillAAFeatures(const string& pep, double *feat)
{
	// Overall amino acid composition features
  	string::size_type pos = aaAlphabet.size();

  	for ( ; pos--; )
  		feat[pos] = 0.0;

  	for (string::const_iterator it = pep.begin(); it != pep.end(); it++)
  	{
    	pos = aaAlphabet.find(*it);
    	if (pos != string::npos)
    		feat[pos]++;
  	}

  	return feat + aaAlphabet.size();
}

// features for a hydrophobicity index
double* RTModel::fillFeaturesIndex(const string& peptide, const float *index, double *features)
{
	*(features++) = indexSum(index, peptide);
	*(features++) = indexAvg(index, peptide);
	*(features++) = indexN(index, peptide);
	*(features++) = indexC(index, peptide);
	*(features++) = indexNearestNeigbourPos(index, peptide);
	*(features++) = indexNearestNeigbourNeg(index, peptide);
	features = indexPartialSum(index, peptide, 5, features);
  	features = indexPartialSum(index, peptide, 2, features);
  	features = amphipathicityHelix(index, peptide, features);
  	features = hydrophobicMoment(index, peptide, 100, 11, features);
  	features = hydrophobicMoment(index, peptide, 180, 11, features);

  	return features;
}

// calculate the retention features
void RTModel::calcRetentionFeatures(PSMDescription &psm)
{
	string peptide = psm.getPeptide();
	string::size_type pos1 = peptide.find('.');
	string::size_type pos2 = peptide.find('.',++pos1);
	string pep = peptide.substr(pos1,pos2-pos1);

	double *features = psm.getRetentionFeatures();

	// if there is memory allocated
	if (psm.getRetentionFeatures())
	{
		if (selected_features & 1 << 0)
			features = fillFeaturesIndex(pep, krokhin_index, features);

		if (selected_features & 1 << 1)
			features = fillFeaturesIndex(pep, krokhin100_index, features);

		if (selected_features & 1 << 2)
			features = fillFeaturesIndex(pep, krokhinC2_index, features);

		if (selected_features & 1 << 3)
			features = fillFeaturesIndex(pep, krokhinTFA_index, features);

		if (selected_features & 1 << 4)
			features = fillFeaturesIndex(pep, kytedoolittle_index, features);

		if (selected_features & 1 << 5)
			features = fillFeaturesIndex(pep, hessa_index, features);

		if (selected_features & 1 << 6)
			*(features++) = pep.size();

		if (selected_features & 1 << 7)
		  	*(features++) = getNoPtms(pep);

		// number of each type of ptm
		if (selected_features & 1 << 8)
			features = fillPTMFeatures(pep, features);

		// bulkiness
		if (selected_features & 1 << 9)
			*(features++) = bulkinessSum(pep);

		// no of consecutive KRDENQ
		if (selected_features & 1 << 10)
			*(features++) = noConsecKRDENQ(pep);

		// amino acids
		if (selected_features & 1 << 11)
			features = fillAAFeatures(pep, features);
	}
}

// calculate the retention features for a vector of PSMs
void RTModel::calcRetentionFeatures(vector<PSMDescription> &psms)
{
	vector<PSMDescription>::iterator it;

	if (VERB > 2)
		cerr << "\nComputing retention features..." << endl;

	for(it = psms.begin(); it != psms.end(); ++it)
		calcRetentionFeatures(*it);

	if (VERB > 2)
		cerr << "Done. " << endl << endl;
}

// save an svm model in the global variable model
void RTModel::copyModel(svm_model* from)
{
	// destroy an existant model
	if (model != NULL)
    	svm_destroy_model(model);

	model = (svm_model *) malloc(sizeof(svm_model));
	model->param = from->param;
	model->nr_class = from->nr_class;
	model->l = from->l;			// total #SV

  	// _DENSE_REP
  	model->SV = (svm_node *)malloc(sizeof(svm_node)*model->l);
  	for(int i = 0;i < model->l; ++i)
  	{
    	model->SV[i].dim = from->SV[i].dim;
    	model->SV[i].values = (double *)malloc(sizeof(double)*model->SV[i].dim);
    	memcpy(model->SV[i].values,from->SV[i].values,sizeof(double)*model->SV[i].dim);
  	}

  	model->sv_coef = (double **) malloc(sizeof(double *)*(model->nr_class-1));
  	for(int i=0;i<model->nr_class-1;++i)
  	{
    	model->sv_coef[i] = (double *) malloc(sizeof(double)*(model->l));
    	memcpy(model->sv_coef[i],from->sv_coef[i],sizeof(double)*(model->l));
  	}
  	int n = model->nr_class * (model->nr_class-1)/2;
  	if (from->rho)
  	{
    	model->rho = (double *) malloc(sizeof(double)*n);
    	memcpy(model->rho,from->rho,sizeof(double)*n);
  	}
  	else
  	{
    	model->rho=NULL;
  	}
  	if (from->probA)
  	{
    	model->probA = (double *) malloc(sizeof(double)*n);
    	memcpy(model->probA,from->probA,sizeof(double)*n);
  	}
  	else
  	{
  		model->probA=NULL;
  	}
  	if (from->probB)
  	{
    	model->probB = (double *) malloc(sizeof(double)*n);
    	memcpy(model->probB,from->probB,sizeof(double)*n);
    }
    else
    {
    	model->probB = NULL;
    }

	assert(from->label == NULL);
	assert(from->nSV == NULL);

  	// for classification only
  	model->label=NULL;		// label of each class (label[k])
  	model->nSV=NULL;		// number of SVs for each class (nSV[k])
  	// nSV[0] + nSV[1] + ... + nSV[k-1] = l
  	// XXX
  	model->free_sv = 1;         // 1 if svm_model is created by svm_load_mode                        // 0 if svm_model is created by svm_train
}

// select the first n features from; return the corresponding code
int RTModel::getSelect(int sel_features, int max, size_t *finalNumFeatures)
{
	int noFeat = 0;
	int retValue = 0;

	for(int i = 0; i < NO_FEATURE_GROUPS; i++)
	// if this feature was selected and if adding it does not exceed the allowed no of features, then add it
		if ((sel_features & 1 << i) && ((noFeat + no_features_per_group[i]) <= max))
		{
			retValue += (int)pow(2, i);
			noFeat += no_features_per_group[i];
		}

	(*finalNumFeatures) = noFeat;
	return retValue;
}

// train the SVM
void RTModel::trainRetention(vector<PSMDescription>& trainset, const double C, const double gamma, const double epsilon, int noPsms)
{
   /*int minFeat = minimumNumRTFeatures();
   //if we have in total less than 500 PSMs, we use only a subset of features
   if ((noPsms>500) || (noFeaturesToCalc <= minFeat))
   {
     numRTFeat = noFeaturesToCalc;
   }
   else
   {
    selected_features = getSelect(selected_features, minFeat, &numRTFeat);
    cout << "Not enough data. Only a subset of features is used to train  the model" << endl;
   }*/

  // initialize the parameters of the SVM
  svm_parameter param;
  param.svm_type = EPSILON_SVR;
  param.kernel_type = RBF;
  //param.gamma = 1/(double)noPsms*gamma;
  param.gamma = gamma;
  param.coef0 = 0;
  param.nu = 0.5;
  param.cache_size = 300;
  param.C = C;
  param.eps = epsilon; //1e-3;
  param.p = 0.1;
  param.shrinking = 1;
  param.probability = 0;
  param.nr_weight = 0;
  param.weight_label = NULL;
  param.weight = NULL;

  // initialize a SVM problem
  svm_problem data;
  data.l=trainset.size();
  data.x=new svm_node[data.l];
  data.y=new double[data.l];

  for(size_t ix1=0;ix1<trainset.size();ix1++)
  {
	  data.x[ix1].values=trainset[ix1].retentionFeatures;
	  data.x[ix1].dim=noFeaturesToCalc;
	  data.y[ix1]=trainset[ix1].retentionTime;
  }

  // build a model by training the SVM on the given training set
  if (svm_check_parameter(&data, &param) != NULL)
  {
	  if (VERB >= 1)
		  cerr << "ERROR: Incorrect parameters for the SVR. Execution aborted. " << endl;
	  exit(-1);
  }

  svm_model* m = svm_train(&data, &param);
  // save the model in the current object
  copyModel(m);

  delete[] data.x;
  delete[] data.y;
  svm_destroy_model(m);
}

// old function to train a SVR (used by percolator)
void RTModel::trainRetention(vector<PSMDescription>& psms)
{
  // Train retention time regressor
  size_t test_frac = 4u;
  if (psms.size()>test_frac*10u) {
    // If we got enough data, calibrate gamma and C by leaving out a testset
	vector<PSMDescription> train,test;
	for(size_t ix=0; ix<psms.size(); ++ix) {
	  if (ix%test_frac==0) {
        test.push_back(psms[ix]);
      } else {
        train.push_back(psms[ix]);
      }
    }
	double sizeFactor=((double)train.size())/((double)psms.size());
    double bestRms = 1e100;
    double gammaV[3] = {gamma/2,gamma,gamma*2};
    double cV[3] = {c/2./sizeFactor,c/sizeFactor,c*2./sizeFactor};
    double epsilonV[3] = {epsilon/2,epsilon,epsilon*2};
    for (double* gammaNow=&gammaV[0];gammaNow!=&gammaV[3];gammaNow++){
        for (double* cNow=&cV[0];cNow!=&cV[3];cNow++){
            for (double* epsilonNow=&epsilonV[0];epsilonNow!=&epsilonV[3];epsilonNow++){
        	  trainRetention(train,*cNow,(*gammaNow)/((double)psms.size()),*epsilonNow,train.size());
        	  double rms=testRetention(test);
        	  if (rms<bestRms) {
        		  c=*cNow;gamma=*gammaNow;epsilon=*epsilonNow;
        		  bestRms=rms;
        	  }
            }
        }
    }
    // Compensate for the difference in size of the training sets
    c=sizeFactor*c;
    // cerr << "CV selected gamma=" << gamma << " and C=" << c << endl;
  }
  trainRetention(psms,c,gamma/((double)psms.size()),epsilon,psms.size());
}

// perform k-validation and return as estimate of the prediction error CV = 1/k (sum(PE(k))), where PE(k)=(sum(yi - yi_pred)^2)/size
double RTModel::computeKfoldCV(const vector<PSMDescription> & psms, const double gamma, const double epsilon, const double c)
{
	vector<PSMDescription> train, test;
	int noPsms;
	// sum of prediction errors
	double sumPEs, PEk;

	noPsms = psms.size();
	sumPEs = 0;

	if (VERB > 2)
		cerr << k << " fold cross validation..." << endl;

	for(int i = 0; i < k; ++i)
	{
		train.clear();
		test.clear();
		// get training and testing sets
		for(int j = 0; j < noPsms; ++j)
		{
			if ((j % k) == i)
				test.push_back(psms[j]);
			else
				train.push_back(psms[j]);

		}

		trainRetention(train, c, gamma, epsilon, noPsms);
		PEk = testRetention(test);
		sumPEs += PEk;
	}

	if (VERB > 2)
		cerr << "Done." << endl;

	return (sumPEs / (double)k);
}

// simple evaluation; just divide the data in 4 parts, train on three of them and test on the 4th; return the ms of diff
double RTModel::computeSimpleEvaluation(const vector<PSMDescription> & psms, const double gamma, const double epsilon, const double c)
{
	vector<PSMDescription> train, test;
	int noPsms;
	double ms;
	// how many parts will the data be split in
	size_t test_frac;

	test_frac = 4u;
	noPsms = psms.size();

	// give a warning if there is little data
	if ((VERB >= 2) && (noPsms < test_frac * 10u))
		cerr << "Warning: very little data to calibrate parameters (just " << noPsms << "), parameter values may be unreliable" << endl;

	if (VERB > 2)
		cerr << "Simple evaluation..." << endl;

	// build train and test set
	for(size_t i = 0; i < noPsms; ++i)
	{
		if (i % test_frac == 0)
			test.push_back(psms[i]);
		else
			train.push_back(psms[i]);
	 }

	// train the model and test it
	trainRetention(train, c, gamma, epsilon, noPsms);
	ms = testRetention(test);

	if (VERB > 2)
		cerr << "Done." << endl;

	return ms;
}

// train the Support Vector Regressor
void RTModel::trainSVM(vector<PSMDescription> & psms)
{
  int noPsms = psms.size();

  if (VERB >= 2)
	  cerr << "Building the model..." << endl;

  // if no parameter calibration is to be performed, then just use the defaults to train the model (except for gamma = 1/n)
  if (gType == NO_GRID)
  {
	  gamma = 1.0 / (double)noPsms;
	  trainRetention(psms, c, gamma, epsilon, noPsms);

	  if (VERB >= 2)
		  cerr << "Done. " << endl;

	  return;
  }

  // calibrate parameters using the grid (if gType = FINE_GRID, this will be followed by a fine grid search)
  ofstream calFile;
  double gamma, epsilon, c;
  double bestError = 1e100, error;
  vector<double>::iterator it1, it2, it3;
  int totalIterations = grids.gridGamma.size() * grids.gridC.size() * grids.gridEpsilon.size();
  int step = 0;

  if (saveCalibration)
  {
	  calFile.open(calibrationFile.c_str(), ios::out);
	  calFile << "gamma\tC\tepsilon\terror\n";
  }

  if (VERB > 2)
  {
	  cerr << "Calibrating (gamma, epsilon, c)..."<< endl;
	  cerr << "------------------------------" << endl;
  }

  // grid search to calibrate parameters
  for(it1 = grids.gridGamma.begin(); it1 != grids.gridGamma.end(); ++it1)
  {
	  for(it2 = grids.gridC.begin(); it2 != grids.gridC.end(); ++it2)
	  {
		  for(it3 = grids.gridEpsilon.begin(); it3 != grids.gridEpsilon.end(); ++it3)
		  {
			  ++step;
			  if (VERB > 2)
			  {
				  cerr << "Step " << step << " / " << totalIterations << endl;
				  cerr << "Evaluate = (gamma, C, epsilon) = (" << (*it1) << ", " << (*it2) << ", " << (*it3) << ")" << endl;
			  }

			  if (eType == SIMPLE_EVAL)
				  error = computeSimpleEvaluation(psms, (*it1), (*it3), (*it2));
			  else
				  error = computeKfoldCV(psms, (*it1), (*it3), (*it2));

			  if (VERB > 2)
				  cerr << "Error = " << error << endl;

			  // save info to the calibration file
			  if (saveCalibration)
			  {
			  	  calFile << (*it1) << "\t" << (*it2) << "\t" << (*it3) << "\t" << error << "\n";
			  }

			  if (error < bestError)
			  {
				  c = (*it2);
				  gamma = (*it1);
				  epsilon = (*it3);
				  bestError = error;
			  }

			  if (VERB > 2)
			 	cerr << endl;
	      }
       }
  }

  if (VERB >= 2)
 	cerr << "Done." << endl;

  // if fine grid is to be perform, then build the fine grid according to parameters and calibrate gamma and
  if (gType == FINE_GRID)
  {
	  vector<double> fGridC, fGridGamma;
	  double offset;

	  if (saveCalibration)
	  {
	  	  calFile << "--------------------------------\n";
	  }
	  if (VERB > 2)
		  cerr << endl << "Calibration via fine grid starting from (gamma, epsilon, c) = (" << gamma << ", " << epsilon << ", "
		   << c << ") with Error = " << bestError << endl;

  	  // define the fine grid
	  for(int i = -noPointsFineGrid; i <= noPointsFineGrid; ++i)
	  {
		  offset = pow(2, stepFineGrid * i);
		  fGridC.push_back(c * offset);
		  fGridGamma.push_back(gamma * offset);
	  }

	  totalIterations = fGridGamma.size() * fGridC.size();
	  step = 0;

	  // fine grid search to calibrate parameters
	  for(it1 = fGridGamma.begin(); it1 != fGridGamma.end(); ++it1)
	  {
		  for(it2 = fGridC.begin(); it2 != fGridC.end(); ++it2)
		  {
			  ++step;
			  if (VERB > 2)
			  {
				  cerr << endl << "Step " << step << " / " << totalIterations << endl;
				  cerr << "Evaluate (gamma, c, epsilon) = " << (*it1) << ", " << (*it2) << ", " << epsilon << ")" << endl;
			  }

			  if (eType == SIMPLE_EVAL)
				  error = computeSimpleEvaluation(psms, (*it1), epsilon, (*it2));
			  else
			 	  error = computeKfoldCV(psms, (*it1), epsilon, (*it2));

			  if (VERB > 2)
				  cerr << "Error = " << error << endl;

			  // save info to the calibration file
			  if (saveCalibration)
			  {
			  	  calFile << (*it1) << "\t" << (*it2) << "\t" << (*it3) << "\t" << error << "\n";
			  }

			  if (error < bestError)
			  {
				  c = (*it2);
				  gamma = (*it1);
				  bestError = error;
			  }
		  }
	  }
  }

  if (VERB > 2)
  {
	  cerr << "------------------------------" << endl;
	  cerr << "Final parameters are (gamma, c, epsilon) = (" << gamma << ", " << c << ", " << epsilon << ") with error = " << bestError << endl;
  }

  // in the final step train the model on all available data using the best parameters found so far
  if (VERB > 3)
  {
	  cerr << endl << "Performing final training..." << endl;
  }

  trainRetention(psms, c, gamma, epsilon, noPsms);

  if (VERB > 3)
  {
	  cerr << "Done." << endl;
  }
  if (saveCalibration)
  {
 	  calFile.close();
 	  if (VERB > 3)
 		  cerr << "Calibration details were saved to " << calibrationFile << endl;
  }
}

// test the svm on the given test set
double RTModel::testRetention(vector<PSMDescription>& testset)
{
  double rms=0.0;
  double estimatedRT;

  for(size_t ix1=0;ix1<testset.size();ix1++)
  {
	  estimatedRT = estimateRT(testset[ix1].retentionFeatures);
	  double diff = estimatedRT - testset[ix1].retentionTime;
	  rms += diff*diff;
  }
  return rms / testset.size();
}

// estimate the retention time using the svm model
double RTModel::estimateRT(double * features)
{
	double predicted_value;
	svm_node node;

	node.values = features;
	node.dim = noFeaturesToCalc;
	predicted_value = svm_predict(model, &node);

	return predicted_value;
}
// set the type of grid
void RTModel::setGridType(const GridType & g)
{
	gType = g;

	if (g == NO_GRID)
	{
		// just set the parameters to their default values
		gamma = INITIAL_GAMMA;
		c = INITIAL_C;
		epsilon = INITIAL_EPSILON;
		// remove any information in the grids variable
		grids.gridC.clear();
		grids.gridEpsilon.clear();
		grids.gridGamma.clear();
	}
	else if (g == NORMAL_GRID)
	{
		grids.gridGamma.assign(NORMAL_GRID_GAMMA, NORMAL_GRID_GAMMA + sizeof(NORMAL_GRID_GAMMA) / sizeof (NORMAL_GRID_GAMMA[0]));
		grids.gridC.assign(NORMAL_GRID_C, NORMAL_GRID_C + sizeof(NORMAL_GRID_C) / sizeof (NORMAL_GRID_C[0]));
		grids.gridEpsilon.assign(NORMAL_GRID_EPSILON, NORMAL_GRID_EPSILON + sizeof(NORMAL_GRID_EPSILON) / sizeof (NORMAL_GRID_EPSILON[0]));
	}
	else if (g == FINE_GRID)
	{
		grids.gridGamma.assign(COARSE_GRID_GAMMA, COARSE_GRID_GAMMA + sizeof(COARSE_GRID_GAMMA) / sizeof (COARSE_GRID_GAMMA[0]));
		grids.gridC.assign(COARSE_GRID_C, COARSE_GRID_C + sizeof(COARSE_GRID_C) / sizeof (COARSE_GRID_C[0]));
		grids.gridEpsilon.assign(COARSE_GRID_EPSILON, COARSE_GRID_EPSILON + sizeof(COARSE_GRID_EPSILON) / sizeof (COARSE_GRID_EPSILON[0]));
	}
	else
	{
		if (VERB >= 2)
			cerr << g << "is unknown. Execution aborted." << endl;
		exit(-1);
	}
}

string RTModel::getGridType()
{
	if (gType == NO_GRID)
		return "No calibration of parameters";
	else
		if (gType == NORMAL_GRID)
			return "Calibration via grid search";
		else
			return "Calibration via coarse grid followed by fine grid search";
}

string RTModel::getEvaluationType()
{
	if (gType != NO_GRID)
	{
		if (eType == SIMPLE_EVAL)
			return "Simple evaluation (train on 3/4 of data, test on the 1/4 left)";
		else
		{
			stringstream s;
			s << k << "-folds cross validation";
			return s.str();
		}
	}
	else
		return "Not applicable if no calibration of parameters is carried out";
}

// save the current model to a file
void RTModel::saveSVRModel(const string modelFile, Normalizer *theNormalizer)
{
	assert (model != NULL);

	svm_save_model2(modelFile.c_str(), model, PSMDescription::normSub, PSMDescription::normDiv, theNormalizer->getSub(),
			        theNormalizer->getDiv(), theNormalizer->getNumRetFeatures(), selected_features);
}

// load a model from a file
void RTModel::loadSVRModel(const string modelFile, Normalizer *theNormalizer)
{
	if (model != NULL)
		svm_destroy_model(model);

	theNormalizer->setNumFeatures(0);
	model = svm_load_model2(modelFile.c_str(), &PSMDescription::normSub, &PSMDescription::normDiv, theNormalizer->getSub(),
							theNormalizer->getDiv(), theNormalizer->getNumRetFeatures(), &selected_features);
	noFeaturesToCalc = *(theNormalizer->getNumRetFeatures());
}

// calculate the difference in hydrophobicity between 2 peptides
double RTModel::calcDiffHydrophobicities(const string & parent, const string & child)
{
	double sumParent, sumChild;

	sumParent = indexSum(krokhin_index, parent);
	sumChild = indexSum(krokhin_index, child);

	return abs(sumParent - sumChild);
}

// print the table of features
void RTModel::printFeaturesInUse(ostringstream & oss)
{
	oss << " *" << noFeaturesToCalc << " features should be used to generate the model." << endl;
	oss << " *Feature groups used: ";
	for(int i = 0; i < NO_FEATURE_GROUPS; i++)
		if (selected_features & 1 << i)
			oss << feature_groups[i] << "\t";
	oss << endl;
}
