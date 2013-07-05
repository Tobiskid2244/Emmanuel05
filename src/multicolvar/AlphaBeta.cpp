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
#include "MultiColvar.h"
#include "tools/Torsion.h"
#include "core/ActionRegister.h"

#include <string>
#include <cmath>

using namespace std;

namespace PLMD{
namespace multicolvar{

//+PLUMEDOC COLVAR ALPHABETA 
/*
Measures a distance including pbc between the instantaneous values of a set of torsional angles and set of reference values.

This colvar calculates the following quantity.

\f[
s = \frac{1}{2} \sum_i \left[ 1 + \cos( \phi_i - \phi_i^{\textrm{Ref}} ) \right]   
\f]

where the \f$\phi_i\f$ values are the instantaneous values for the \ref TORSION angles of interest.
The \f$\phi_i^{\textrm{Ref}}\f$ values are the user-specified reference values for the torsional angles.

\par Examples

The following provides an example of the input for an alpha beta similarity.

\verbatim
ALPHABETA ...
ATOMS1=168,170,172,188 REFERENCE1=3.14 
ATOMS2=170,172,188,190 REFERENCE2=3.14 
ATOMS3=188,190,192,230 REFERENCE3=3.14
LABEL=ab
... ALPHABETA
PRINT ARG=ab FILE=colvar STRIDE=10
\endverbatim

Because all the reference values are the same we can calculate the same quantity using

\verbatim
ALPHABETA ...
ATOMS1=168,170,172,188 REFERENCE=3.14 
ATOMS2=170,172,188,190 
ATOMS3=188,190,192,230 
LABEL=ab
... ALPHABETA
PRINT ARG=ab FILE=colvar STRIDE=10
\endverbatim

*/
//+ENDPLUMEDOC

class AlphaBeta : public MultiColvar {
private:
  std::vector<double> target;
public:
  static void registerKeywords( Keywords& keys );
  AlphaBeta(const ActionOptions&);
  virtual double compute( const unsigned& j );
  bool isPeriodic(){ return false; }
  Vector getCentralAtom();  
};

PLUMED_REGISTER_ACTION(AlphaBeta,"ALPHABETA")

void AlphaBeta::registerKeywords( Keywords& keys ){
  MultiColvar::registerKeywords( keys );
  keys.use("ATOMS");
  keys.add("numbered","REFERENCE","the reference values for each of the torsional angles.  If you use a single REFERENCE value the " 
                                  "same reference value is used for all torsions");
  keys.reset_style("REFERENCE","compulsory");
}

AlphaBeta::AlphaBeta(const ActionOptions&ao):
PLUMED_MULTICOLVAR_INIT(ao)
{
  // Read in the atoms
  int natoms=4; readAtoms( natoms );
  // Resize target
  target.resize( taskList.fullSize() );

  // Read in reference values
  unsigned ntarget=0;
  for(unsigned i=0;i<target.size();++i){
     if( !parseNumbered( "REFERENCE", i+1, target[i] ) ) break;
     ntarget++; 
  }
  if( ntarget==0 ){
      parse("REFERENCE",target[0]);
      for(unsigned i=1;i<target.size();++i) target[i]=target[0];
  } else if( ntarget!=target.size() ){
      error("found wrong number of REFERENCE values");
  }

  // And setup the ActionWithVessel
  readVesselKeywords();
  if( getNumberOfVessels()==0 ){
     std::string fake_input;
     addVessel( "SUM", fake_input, -1 );  // -1 here means that this value will be named getLabel()
     readVesselKeywords();  // This makes sure resizing is done
  }

  // And check everything has been read in correctly
  checkRead();
}

double AlphaBeta::compute( const unsigned& j ){
  Vector d0,d1,d2;
  d0=getSeparation(getPosition(1),getPosition(0));
  d1=getSeparation(getPosition(2),getPosition(1));
  d2=getSeparation(getPosition(3),getPosition(2));

  Vector dd0,dd1,dd2;
  PLMD::Torsion t;
  double value  = t.compute(d0,d1,d2,dd0,dd1,dd2);
  double svalue = -0.5*sin(value-target[current]);
  double cvalue = 1.+cos(value-target[current]);

  dd0 *= svalue;
  dd1 *= svalue;
  dd2 *= svalue;
  value = 0.5*cvalue;

  addAtomsDerivatives(0,dd0);
  addAtomsDerivatives(1,dd1-dd0);
  addAtomsDerivatives(2,dd2-dd1);
  addAtomsDerivatives(3,-dd2);

  addBoxDerivatives  (-(extProduct(d0,dd0)+extProduct(d1,dd1)+extProduct(d2,dd2)));

  return value;
}

Vector AlphaBeta::getCentralAtom(){
   addCentralAtomDerivatives( 1, 0.5*Tensor::identity() );
   addCentralAtomDerivatives( 2, 0.5*Tensor::identity() );
   return 0.5*( getPosition(1) + getPosition(2) );
}

}
}
