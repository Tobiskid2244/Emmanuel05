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
#include "Colvar.h"
#include "ActionRegister.h"
#include "core/PlumedMain.h"
#include "core/Atoms.h"

#include <string>
#include <cmath>

namespace PLMD{
namespace colvar{

//+PLUMEDOC COLVAR ENERGY
/*
Calculate the total energy of the simulation box.

Total energy can be biased with umbrella sampling \cite bart-karp98jpcb or with well tempered metadynamics \cite Bonomi:2009p17935.

\par Examples
The following input instructs plumed to print the energy of the system
\verbatim
ENERGY LABEL=ene
PRINT ARG=ene
\endverbatim
(See also \ref PRINT).

*/
//+ENDPLUMEDOC


class Energy : public Colvar {

public:
  Energy(const ActionOptions&);
// active methods:
  void prepare();
  virtual void calculate();
  static void registerKeywords( Keywords& keys );
};


using namespace std;


PLUMED_REGISTER_ACTION(Energy,"ENERGY")

Energy::Energy(const ActionOptions&ao):
PLUMED_COLVAR_INIT(ao)
{
//  if(checkNumericalDerivatives())
//    error("Cannot use NUMERICAL_DERIVATIVES with ENERGY");
  isEnergy=true;
  addValueWithDerivatives(); setNotPeriodic();
  getPntrToValue()->resizeDerivatives(1);
  log<<"  Bibliography ";
  log<<plumed.cite("Bartels and Karplus, J. Phys. Chem. B 102, 865 (1998)");
  log<<plumed.cite("Bonomi and Parrinello, J. Comp. Chem. 30, 1615 (2009)");
  log<<"\n";
}

void Energy::registerKeywords( Keywords& keys ){
  Action::registerKeywords( keys );
  ActionAtomistic::registerKeywords( keys );
  ActionWithValue::registerKeywords( keys );
  keys.remove("NUMERICAL_DERIVATIVES"); 
}

void Energy::prepare(){
  plumed.getAtoms().setCollectEnergy(true);
}

// calculator
void Energy::calculate(){
  setValue( getEnergy() );
  getPntrToComponent(0)->addDerivative(0,1.0);
}

}
}



