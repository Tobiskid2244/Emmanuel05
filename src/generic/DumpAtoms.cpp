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
#include "core/ActionAtomistic.h"
#include "core/ActionPilot.h"
#include "core/ActionRegister.h"
#include "tools/Pbc.h"
#include "tools/File.h"
#include "core/PlumedMain.h"
#include "core/Atoms.h"
#include "tools/Units.h"
#include <cstdio>
#include "core/SetupMolInfo.h"
#include "core/ActionSet.h"

using namespace std;

namespace PLMD
{
namespace generic{

//+PLUMEDOC ANALYSIS DUMPATOMS
/*
Dump selected atoms on a file.

This command can be used to output the positions of a particular set of atoms.
The atoms required are ouput in a xyz or gro formatted file.
The type of file is automatically detected from the file extension, but can be also
enforced with TYPE.
Importantly, if your
input file contains actions that edit the atoms position (e.g. \ref WHOLEMOLECULES)
and the DUMPATOMS command appears after this instruction, then the edited
atom positions are output.
You can control the buffering of output using the \ref FLUSH keyword on a separate line.

Units of the printed file can be controlled with the UNITS keyword. By default PLUMED units as
controlled in the \ref UNITS command are used, but one can override it e.g. with UNITS=A.
Notice that gro files can only contain coordinates in nm.

\par Examples

The following input instructs plumed to print out the positions of atoms
1-10 together with the position of the center of mass of atoms 11-20 every
10 steps to a file called file.xyz.
\verbatim
COM ATOMS=11-20 LABEL=c1
DUMPATOMS STRIDE=10 FILE=file.xyz ATOMS=1-10,c1
\endverbatim
(see also \ref COM)

The following input is very similar but dumps a .gro (gromacs) file,
which also contains atom and residue names.
\verbatim
# this is required to have proper atom names:
MOLINFO STRUCTURE=reference.pdb
# if omitted, atoms will have "X" name...

COM ATOMS=11-20 LABEL=c1
DUMPATOMS STRIDE=10 FILE=file.gro ATOMS=1-10,c1
# notice that last atom is a virtual one and will not have
# a correct name in the resulting gro file
\endverbatim
(see also \ref COM and \ref MOLINFO)


*/
//+ENDPLUMEDOC

class DumpAtoms:
  public ActionAtomistic,
  public ActionPilot
{
  OFile of;
  double lenunit;
  std::vector<std::string> names;
  std::vector<unsigned>    residueNumbers;
  std::vector<std::string> residueNames;
  std::string type;
public:
  DumpAtoms(const ActionOptions&);
  ~DumpAtoms();
  static void registerKeywords( Keywords& keys );
  void calculate(){}
  void apply(){}
  void update();
};

PLUMED_REGISTER_ACTION(DumpAtoms,"DUMPATOMS")

void DumpAtoms::registerKeywords( Keywords& keys ){
  Action::registerKeywords( keys );
  ActionPilot::registerKeywords( keys );
  ActionAtomistic::registerKeywords( keys );
  keys.add("compulsory","STRIDE","1","the frequency with which the atoms should be output");
  keys.add("atoms", "ATOMS", "the atom indices whose positions you would like to print out");
  keys.add("compulsory", "FILE", "file on which to output coordinates. .gro extension is automatically detected");
  keys.add("compulsory", "UNITS","PLUMED","the units in which to print out the coordinates. PLUMED means internal PLUMED units");
  keys.add("optional", "TYPE","file type, either xyz or gro, can override an automatically detected file extension");
}

DumpAtoms::DumpAtoms(const ActionOptions&ao):
  Action(ao),
  ActionAtomistic(ao),
  ActionPilot(ao)
{
  vector<AtomNumber> atoms;
  string file;
  parse("FILE",file);
  if(file.length()==0) error("name out output file was not specified");
    type=Tools::extension(file);
    log<<"  file name "<<file<<"\n";
  if(type=="gro" || type=="xyz"){
    log<<"  file extension indicates a "<<type<<" file\n";
  } else {
    log<<"  file extension not detected, assuming xyz\n";
    type="xyz";
  }
  string ntype;
  parse("TYPE",ntype);
  if(ntype.length()>0){
    if(ntype!="xyz" && ntype!="gro") error("TYPE should be either xyz or gro");
    log<<"  file type enforced to be "<<ntype<<"\n";
    type=ntype;
  }

  parseAtomList("ATOMS",atoms);

  std::string unitname; parse("UNITS",unitname);
  if(unitname!="PLUMED"){
    Units myunit; myunit.setLength(unitname);
    if(myunit.getLength()!=1.0 && type=="gro") error("gro files should be in nm");
    lenunit=plumed.getAtoms().getUnits().getLength()/myunit.getLength();
  } else if(type=="gro") lenunit=plumed.getAtoms().getUnits().getLength(); 
  else lenunit=1.0;

  checkRead();
  of.link(*this);
  of.open(file);
  log.printf("  printing the following atoms in %s :", unitname.c_str() );
  for(unsigned i=0;i<atoms.size();++i) log.printf(" %d",atoms[i].serial() );
  log.printf("\n");
  requestAtoms(atoms);
  std::vector<SetupMolInfo*> moldat=plumed.getActionSet().select<SetupMolInfo*>();
  if( moldat.size()==1 ){
    log<<"  MOLINFO DATA found, using proper atom names\n";
    names.resize(atoms.size());
    for(unsigned i=0;i<atoms.size();i++) names[i]=moldat[0]->getAtomName(atoms[i]);
    residueNumbers.resize(atoms.size());
    for(unsigned i=0;i<residueNumbers.size();++i) residueNumbers[i]=moldat[0]->getResidueNumber(atoms[i]);
    residueNames.resize(atoms.size());
    for(unsigned i=0;i<residueNames.size();++i) residueNames[i]=moldat[0]->getResidueName(atoms[i]);
  }
}

void DumpAtoms::update(){
  if(type=="xyz"){
    of.printf("%d\n",getNumberOfAtoms());
    const Tensor & t(getPbc().getBox());
    if(getPbc().isOrthorombic()){
      of.printf(" %f %f %f\n",lenunit*t(0,0),lenunit*t(1,1),lenunit*t(2,2));
    }else{
      of.printf(" %f %f %f %f %f %f %f %f %f\n",
                   lenunit*t(0,0),lenunit*t(0,1),lenunit*t(0,2),
                   lenunit*t(1,0),lenunit*t(1,1),lenunit*t(1,2),
                   lenunit*t(2,0),lenunit*t(2,1),lenunit*t(2,2)
             );
    }
    for(unsigned i=0;i<getNumberOfAtoms();++i){
      const char* defname="X";
      const char* name=defname;
      if(names.size()>0) if(names[i].length()>0) name=names[i].c_str();
      of.printf("%s %f %f %f\n",name,lenunit*getPosition(i)(0),lenunit*getPosition(i)(1),lenunit*getPosition(i)(2));
    }
  } else if(type=="gro"){
    const Tensor & t(getPbc().getBox());
    of.printf("Made with PLUMED t=%f\n",getTime()/plumed.getAtoms().getUnits().getTime());
    of.printf("%d\n",getNumberOfAtoms());
    for(unsigned i=0;i<getNumberOfAtoms();++i){
      const char* defname="X";
      const char* name=defname;
      unsigned residueNumber=0;
      if(names.size()>0) if(names[i].length()>0) name=names[i].c_str();
      if(residueNumbers.size()>0) residueNumber=residueNumbers[i];
      std::string resname="";
      if(residueNames.size()>0) resname=residueNames[i];
      of.printf("%5u%-5s%5s%5d%8.3f%8.3f%8.3f%8.4f%8.4f%8.4f\n",residueNumber,resname.c_str(),name,getAbsoluteIndex(i).serial(),
         lenunit*getPosition(i)(0),lenunit*getPosition(i)(1),lenunit*getPosition(i)(2),0.0,0.0,0.0);
    }
    of.printf("%12.7f %12.7f %12.7f %12.7f %12.7f %12.7f %12.7f %12.7f %12.7f\n",
           lenunit*t(0,0),lenunit*t(1,1),lenunit*t(2,2),
           lenunit*t(0,1),lenunit*t(0,2),lenunit*t(1,0),
           lenunit*t(1,2),lenunit*t(2,0),lenunit*t(2,1));
  } else plumed_merror("unknown file type "+type);
}

DumpAtoms::~DumpAtoms(){
}
  

}
}
