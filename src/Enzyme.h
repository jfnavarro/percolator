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

#ifndef ENZYME_H_
#define ENZYME_H_

#include <string>
#include <assert.h>
using namespace std;

class Enzyme {
public:
  enum EnzymeType {NO_ENZYME,TRYPSIN,CHYMOTRYPSIN,ELASTASE};
  virtual ~Enzyme() {
    ;
  }
  static Enzyme *getEnzyme();
  static void setEnzyme(EnzymeType enz);
  static EnzymeType getEnzymeType() {
    return getEnzyme()->getET();
  }
  static size_t countEnzymatic(string& peptide);
  static bool isEnzymatic(const char& n,const char& c) {
    return getEnzyme()->isEnz(n,c);
  }
  static bool isEnzymatic(string peptide) {
    return (getEnzyme()->isEnz(peptide[0], peptide[2]) &&
            getEnzyme()->isEnz(peptide[peptide.length() - 3], peptide[peptide.length() - 1]));
  }
  static string getStringEnzyme() {
    return getEnzyme()->toString();
  }
  Enzyme() {
    assert(theEnzyme==NULL);
    theEnzyme=this;
  }
protected:
  static Enzyme * theEnzyme;
  virtual bool isEnz(const char& n,const char& c) {
    return true;
  }
  virtual string toString() {
    return "no_enzyme";
  }
  virtual EnzymeType getET() {
    return NO_ENZYME;
  }
};

class Trypsin : public Enzyme {
public:
  virtual ~Trypsin() {
    ;
  }
  Trypsin() {
    ;
  }
protected:
  virtual string toString() {
    return "trypsin";
  }
  virtual bool isEnz(const char& n,const char& c) {
    return (((n=='K' || n=='R') && c != 'P') || n=='-' || c=='-');
  }
  virtual EnzymeType getET() {
    return TRYPSIN;
  }
};

class Chymotrypsin : public Enzyme {
public:
  virtual ~Chymotrypsin() {
    ;
  }
  Chymotrypsin() {
    ;
  }
protected:
  virtual string toString() {
    return "chymotrypsin";
  }
  virtual bool isEnz(const char& n,const char& c) {
    return (((n=='F' || n=='H' || n=='W' || n=='Y' || n=='L' || n=='M') && c!= 'P') || n=='-' || c=='-');
  }
  virtual EnzymeType getET() {
    return CHYMOTRYPSIN;
  }
};

class Elastase : public Enzyme {
public:
  virtual ~Elastase() {
    ;
  }
  Elastase() {
    ;
  }
protected:
  virtual string toString() {
    return "elastase";
  }
  virtual bool isEnz(const char& n,const char& c) {
    return (((n=='L' || n=='V' || n=='A' || n=='G' ) && c!= 'P') || n=='-' || c=='-');
  }
  virtual EnzymeType getET() {
    return ELASTASE;
  }
};

#endif /* ENZYME_H_ */
