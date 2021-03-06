
/*******************************************************************************
 Copyright 2006-2012 Lukas Käll <lukas.kall@scilifelab.se>

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

#include <boost/algorithm/string.hpp> // for case insensitive string compare  TOFIX not very portable
#include <cstdlib>
#include <iostream>
#include <cstring> // needed for stricmp()
#include <string>
#include <assert.h>
#include <MyException.h>


class Enzyme {
  public:
    enum EnzymeType {
      NO_ENZYME, TRYPSIN, CHYMOTRYPSIN, THERMOLYSIN, PROTEINASEK, PEPSIN, ELASTASE, 
      LYSN, LYSC, ARGC, ASPN, GLUC
    };
    virtual ~Enzyme() {
      delete theEnzyme;
    }
    static Enzyme* getEnzyme();
    static void setEnzyme(EnzymeType enz);
    static void setEnzyme(std::string enzyme);
    static EnzymeType getEnzymeType() {
      return getEnzyme()->getET();
    }
    static size_t countEnzymatic(std::string& peptide);
    static bool isEnzymatic(const char& n, const char& c) {
      return getEnzyme()->isEnz(n, c);
    }
    static bool isEnzymatic(std::string peptide) {
      return (getEnzyme()->isEnz(peptide[0], peptide[2])
          && getEnzyme()->isEnz(peptide[peptide.length() - 3],
                                peptide[peptide.length() - 1]));
    }
    static std::string getStringEnzyme() {
      return getEnzyme()->toString();
    }
    Enzyme() {
      assert(theEnzyme == NULL);
      theEnzyme = this;
    }
    static std::string getString() {
      return "no_enzyme";
    }
  protected:
    static Enzyme* theEnzyme;
    virtual bool isEnz(const char& n, const char& c) {
      return true;
    }
    virtual std::string toString() {
      return getString();
    }
    virtual EnzymeType getET() {
      return NO_ENZYME;
    }
    
  private:
    
    inline int stricmp (const std::string &s1,const std::string &s2)
    {
      return stricmp (s1.c_str(), s2.c_str()); // C's stricmp
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
    static std::string getString() {
      return "trypsin";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'K' || n == 'R') && c != 'P') || n == '-' || c == '-');
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
    static std::string getString() {
      return "chymotrypsin";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'F' || n == 'H' || n == 'W' || n == 'Y' || n == 'L'
          || n == 'M') && c != 'P') || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return CHYMOTRYPSIN;
    }
};

class Thermolysin : public Enzyme {
  public:
    virtual ~Thermolysin() {
      ;
    }
    Thermolysin() {
      ;
    }
    static std::string getString() {
      return "thermolysin";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((c == 'A' || c == 'F' || c == 'I' || c == 'L' || c == 'M'
          || c == 'V' || (n == 'R' && c == 'G')) && n != 'D' && n != 'E') || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return THERMOLYSIN;
    }
};

class Proteinasek : public Enzyme {
  public:
    virtual ~Proteinasek() {
      ;
    }
    Proteinasek() {
      ;
    }
    static std::string getString() {
      return "proteinasek";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return ((n == 'A' || n == 'E' || n == 'F' || n == 'I' || n == 'L'
          || n == 'T' || n == 'V' || n == 'W' || n == 'Y' ) || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return PROTEINASEK;
    }
};

class Pepsin : public Enzyme {
  public:
    virtual ~Pepsin() {
      ;
    }
    Pepsin() {
      ;
    }
    static std::string getString() {
      return "pepsin";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((c == 'F' || c == 'L' || c == 'W' || c == 'Y' || n == 'F'
          || n == 'L' || n == 'W' || n == 'Y') && n != 'R') || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return PEPSIN;
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
    static std::string getString() {
      return "elastase";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'L' || n == 'V' || n == 'A' || n == 'G') && c != 'P')
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return ELASTASE;
    }
};

class LysN : public Enzyme {
  public:
    virtual ~LysN() {
      ;
    }
    LysN() {
      ;
    }
    static std::string getString() {
      return "lys-n";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return ((c == 'K')
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return LYSN;
    }
};





class LysC : public Enzyme {
  public:
    virtual ~LysC() {
      ;
    }
    LysC() {
      ;
    }
    static std::string getString() {
      return "lys-c";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'K') && c != 'P')
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return LYSC;
    }
};

class ArgC : public Enzyme {
  public:
    virtual ~ArgC() {
      ;
    }
    ArgC() {
      ;
    }
    static std::string getString() {
      return "arg-c";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'R') && c != 'P')
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return ARGC;
    }
};

class AspN : public Enzyme {
  public:
    virtual ~AspN() {
      ;
    }
    AspN() {
      ;
    }
    static std::string getString() {
      return "asp-n";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return ((c == 'D')
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return ASPN;
    }
};

class GluC : public Enzyme {
  public:
    virtual ~GluC() {
      ;
    }
    GluC() {
      ;
    }
    static std::string getString() {
      return "glu-c";
    }
  protected:
    virtual std::string toString() {
      return getString();
    }
    virtual bool isEnz(const char& n, const char& c) {
      return (((n == 'E') && (c != 'P'))
          || n == '-' || c == '-');
    }
    virtual EnzymeType getET() {
      return GLUC;
    }
};

#endif /* ENZYME_H_ */
