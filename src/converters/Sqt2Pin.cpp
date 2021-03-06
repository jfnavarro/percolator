#include "Sqt2Pin.h"


Sqt2Pin::Sqt2Pin() {
 
}

Sqt2Pin::~Sqt2Pin() {
  if(reader)
    delete reader;
  reader = 0;
}


int Sqt2Pin::run() {
  
  // Content of sqt files is merged: preparing to write it to xml file
  
  ofstream xmlOutputStream;
  xmlOutputStream.open(xmlOutputFN.c_str());
  if(!xmlOutputStream && xmlOutputFN != ""){
    cerr << "Error: invalid path to output file: " << xmlOutputFN << endl;
    cerr << "Please invoke sqt2pin with a valid -o option" << endl;
    return 0;
  }
  
  //initialize reader
  parseOptions.targetFN = targetFN;
  parseOptions.decoyFN = decoyFN;
  parseOptions.call = call;
  parseOptions.spectrumFN = spectrumFile;
  parseOptions.xmlOutputFN = xmlOutputFN;
  reader = new SqtReader(&parseOptions);
  
  reader->init();
  reader->print(xmlOutputStream);
  
  if (VERB>2)
    cerr << "\nAll the input files have been successfully processed"<< endl;

  return true;
}
int main(int argc, char** argv) {
  
  Sqt2Pin* pSqt2Pin = new Sqt2Pin();
  int retVal = -1;
  
  try
  {
    if (pSqt2Pin->parseOpt(argc, argv, Sqt2Pin::Usage())) {
      retVal = pSqt2Pin->run();
    }
  }
  catch (const std::exception& e) 
  {
    std::cerr << e.what() << endl;
    retVal = -1;
  }
  catch(...)
  {
    std::cerr << "Unknown exception, contact the developer.." << std::endl;
  }  
    
  delete pSqt2Pin;
  Globals::clean();
  return retVal;
}
