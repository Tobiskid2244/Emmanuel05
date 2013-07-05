/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "core/ActionPilot.h"
#include "core/ActionWithValue.h"
#include "core/ActionRegister.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/Atoms.h"
#include "tools/IFile.h"

namespace PLMD{
namespace generic{

//+PLUMEDOC GENERIC READ
/* 
Read quantities from a colvar file.

\par Examples

*/
//+ENDPLUMEDOC

class Read :
public ActionPilot,
public ActionWithValue
{
private:
  bool cloned_file;
  unsigned nlinesPerStep;
  std::string filename;
  IFile* ifile;
  std::vector<Value*> readvals;
public:
  static void registerKeywords( Keywords& keys );
  Read(const ActionOptions&);
  ~Read();
  void prepare();
  void apply(){}
  void calculate();
  void update();
  std::string getFilename() const;
  IFile* getFile();
};

PLUMED_REGISTER_ACTION(Read,"READ")

void Read::registerKeywords(Keywords& keys){
  Action::registerKeywords(keys);
  ActionPilot::registerKeywords(keys);
  ActionWithValue::registerKeywords(keys);
  keys.add("compulsory","STRIDE","1","the frequency with which the file should be read.");
  keys.add("compulsory","EVERY","1","only read every ith line of the colvar file. This should be used if the colvar was written more frequently than the trajectory.");
  keys.add("compulsory","VALUES","the values to read from the file");
  keys.add("compulsory","FILE","the name of the file from which to read these quantities");
  keys.remove("NUMERICAL_DERIVATIVES");
}

Read::Read(const ActionOptions&ao):
Action(ao),
ActionPilot(ao),
ActionWithValue(ao),
nlinesPerStep(1)
{
  // Read the file name from the input line
  parse("FILE",filename);
  // Open the file if it is not already opened
  cloned_file=false;
  std::vector<Read*> other_reads=plumed.getActionSet().select<Read*>();
  for(unsigned i=0;i<other_reads.size();++i){
      if( other_reads[i]->getFilename()==filename ){
          ifile=other_reads[i]->getFile();
          cloned_file=true;
      }
  }
  if( !cloned_file ){
      ifile=new IFile();
      if( !ifile->FileExist(filename) ) error("could not find file named " + filename);
      ifile->link(*this);
      ifile->open(filename);
      ifile->allowIgnoredFields();
  }
  parse("EVERY",nlinesPerStep);
  if(nlinesPerStep>1) log.printf("  only reading every %uth line of file %s\n",nlinesPerStep,filename.c_str() );
  else log.printf("  reading data from file %s\n",filename.c_str() );
  // Find out what we are reading
  std::vector<std::string> valread; parseVector("VALUES",valread);

  std::size_t dot=valread[0].find_first_of('.');
  if( valread[0].find(".")!=std::string::npos ){
      std::string label=valread[0].substr(0,dot);
      std::string name=valread[0].substr(dot+1);
      if( name=="*" ){
         if( valread.size()>1 ) error("all values must be from the same Action when using READ");
         std::vector<std::string> fieldnames;
         ifile->scanFieldList( fieldnames );
         for(unsigned i=0;i<fieldnames.size();++i){
             if( fieldnames[i].substr(0,dot)==label ){
                 readvals.push_back(new Value(this, fieldnames[i], false) );
                 addComponent( fieldnames[i].substr(dot+1) ); componentIsNotPeriodic( fieldnames[i].substr(dot+1) );
             }
         }
      } else {
         readvals.push_back(new Value(this, valread[0], false) );
         addComponent( name ); componentIsNotPeriodic( valread[0].substr(dot+1) );
         for(unsigned i=1;i<valread.size();++i) {
             if( valread[i].substr(0,dot)!=label ) error("all values must be from the same Action when using READ");;
             readvals.push_back(new Value(this, valread[i], false) );
             addComponent( valread[i].substr(dot+1) ); componentIsNotPeriodic( valread[i].substr(dot+1) );
         }
      }
  } else {
      if( valread.size()!=1 ) error("all values must be from the same Action when using READ");
      readvals.push_back(new Value(this, valread[0], false) );
      addValue(); setNotPeriodic();
      log.printf("  reading value %s and storing as %s\n",valread[0].c_str() ,getLabel().c_str() );   
  }
  checkRead();
}

Read::~Read(){
  if( !cloned_file ){ ifile->close(); delete ifile; }
  for(unsigned i=0;i<readvals.size();++i) delete readvals[i];
}

std::string Read::getFilename() const {
  return filename;
}

IFile* Read::getFile(){
  return ifile;
}

void Read::prepare(){
  if( !cloned_file ){
      double du_time; 
      if( !ifile->scanField("time",du_time) ){
           error("Reached end of file " + filename + " before end of trajectory");
      } else if( fabs( du_time-getTime() )>plumed.getAtoms().getTimeStep() ){
          std::string str_dutime,str_ptime; Tools::convert(du_time,str_dutime); Tools::convert(getTime(),str_ptime);
          error("mismatched times in colvar files : colvar time=" + str_dutime + " plumed time=" + str_ptime );
      }
  }  
}

void Read::calculate(){
  std::string smin, smax;
  for(unsigned i=0;i<readvals.size();++i){
      ifile->scanField( readvals[i] );
      getPntrToComponent(i)->set( readvals[i]->get() ); 
      if( readvals[i]->isPeriodic() ){
          readvals[i]->getDomain( smin, smax );
          getPntrToComponent(i)->setDomain( smin, smax );
      } 
  }
}

void Read::update(){
  if( !cloned_file ){
      for(unsigned i=0;i<nlinesPerStep;++i){
         ifile->scanField(); double du_time;
         if( plumed.getAtoms().getNatoms()==0 && !ifile->scanField("time",du_time) ) plumed.stop(); 
      }
  }
}

}
}
