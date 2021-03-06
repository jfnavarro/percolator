# Mattia Tomasoni - Percolator Project
# Script that tests the correctness of percolator
# Parameters: none

import os
import sys
import tempfile

pathToBinaries = "@pathToBinaries@"
pathToData = "@pathToData@"
tmpPath = tempfile.gettempdir()
success = True

print("PERCOLATOR CORRECTNESS")

# validating output against schema
def validate(what,file):
  success = True
  print("(*) validating "+what+" output...")
  processFile = os.popen("xmllint --noout --schema " + pathToData + "/../src/xml/percolator_out.xsd " + file)
  exitStatus = processFile.close()
  if exitStatus is not None:
    print("...TEST FAILED: percolator (on "+what+" probabilities) produced an invalid output")
    print("check "+file+" for details")
    success = False
  return success

def canPercRunThis(testName,flags,testFile,checkValid=True,readData=True):
  tmpName=os.path.join(tmpPath,"PERCOLATOR_"+testName)
  readPath=""
  if readData:
    readPath = os.path.join(pathToData, testFile)
  processFile = os.popen("(" + os.path.join(pathToBinaries, "percolator ") + readPath 
   + " -X " + tmpName + ".pout.xml " + flags + 
  " 2>&1) > "+ tmpName +".txt")
  exitStatus = processFile.close()
  if exitStatus is not None:
    print("...TEST FAILED: percolator ("+testName+") terminated with " + str(exitStatus) + " exit status")
    print("check "+ tmpName +".txt for details") 
    success = False
  if checkValid:
    success = validate(testName,tmpName +".pout.xml")

# running percolator to calculate psm probabilities
print("(*) running percolator to calculate psm probabilities...")
canPercRunThis("psms","-U","percolator/pin/pin.xml")

# running percolator to calculate peptide probabilities
print("(*) running percolator to calculate peptide probabilities...")
canPercRunThis("peptides","","percolator/pin/pin.xml")

# running percolator to calculate protein probabilities
print("(*) running percolator to calculate protein probabilities...")
canPercRunThis("proteins","-A","percolator/pin/pin.xml")

# running percolator to calculate psm probabilities given input file generated from sqt2pin with ptms
print("(*) running percolator on input with ptms...")
canPercRunThis("psm_ptms","-U","percolator/pin_ptms/pin_ptms.xml")

# running percolator with option to generate tab-delimited input
print("(*) running percolator to generate tab-delimited input...")
tabData=os.path.join(pathToData, "percolator/tab/percolatorTab ")
flg = "-U -J " + tabData
canPercRunThis("tab_generate",flg,"percolator/pin/pin.xml",False)

# running percolator with option to process tab-delimited input
print("(*) running percolator on tab-delimited input...")
flg = "-U -j " + tabData
canPercRunThis("tab",flg,"",False,False)

# running percolator on pin.xml with -D 4 option (description of correct features)
print("(*) running percolator with description of correct features option...")
canPercRunThis("D4on","-D 4 -U","percolator/pin/pin.xml")

# if no errors were encountered, succeed
if success == True:
 print("...TEST SUCCEEDED")
 exit(0)
else:
 print("...TEST FAILED")
 exit(1)
