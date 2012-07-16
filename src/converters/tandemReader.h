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


#ifndef TANDEMREADER_H
#define TANDEMREADER_H

#include "Reader.h"
#include "parser.hxx"
#include "tandem2011.12.01.1.hxx"
#include "FragSpectrumScanDatabase.h"

using namespace std;
using namespace xercesc;

typedef map<std::string, double> spectraMapType;

class tandemReader: public Reader
{

public:
  
  tandemReader(ParseOptions po);
  
  virtual ~tandemReader();
  
  virtual void read(const std::string fn, bool isDecoy,boost::shared_ptr<FragSpectrumScanDatabase> database);
  
  virtual bool checkValidity(const std::string file);
  
  virtual bool checkIsMeta(std::string file);
 
  virtual void getMaxMinCharge(std::string fn);
  
  virtual void addFeatureDescriptions(bool doEnzyme,const std::string& aaAlphabet);
  
private:
  std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
  std::vector<std::string> split(const std::string &s, char delim);
  spectraMapType spectraMap;
  
  bool fixedXML;
  bool x_score;
  bool y_score;
  bool z_score;
  bool a_score;
  bool b_score;
  bool c_score;
  bool firstPSM;
  
  void createPSM(const tandem_ns::protein &protObj,bool isDecoy,boost::shared_ptr<FragSpectrumScanDatabase> database,spectraMapType &spectraMap,std::string fn);
};

#endif //TANDEMREADER_H
