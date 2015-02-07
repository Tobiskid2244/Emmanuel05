/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2015 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.

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
#include "ActionWithVirtualAtom.h"
#include "Atoms.h"

using namespace std;

namespace PLMD{

void ActionWithVirtualAtom::registerKeywords(Keywords& keys){
  Action::registerKeywords(keys);
  ActionAtomistic::registerKeywords(keys);
  keys.add("atoms","ATOMS","the list of atoms which are involved the virtual atom's definition");
}

ActionWithVirtualAtom::ActionWithVirtualAtom(const ActionOptions&ao):
  Action(ao),
  ActionAtomistic(ao)
{
  index=atoms.addVirtualAtom(this);
  log.printf("  serial associated to this virtual atom is %u\n",index.serial());
}

ActionWithVirtualAtom::~ActionWithVirtualAtom(){
  atoms.removeVirtualAtom(this);
}

void ActionWithVirtualAtom::apply(){
  const Vector & f(atoms.forces[index.index()]);
  for(unsigned i=0;i<getNumberOfAtoms();i++) modifyForces()[i]=matmul(derivatives[i],f);
}

void ActionWithVirtualAtom::requestAtoms(const std::vector<AtomNumber> & a){
  ActionAtomistic::requestAtoms(a);
  derivatives.resize(a.size());
}

void ActionWithVirtualAtom::setGradients(){
  gradients.clear();
  for(unsigned i=0;i<getNumberOfAtoms();i++){
    AtomNumber an=getAbsoluteIndex(i);
    // this case if the atom is a virtual one 	 
    if(atoms.isVirtualAtom(an)){
      const ActionWithVirtualAtom* a=atoms.getVirtualAtomsAction(an);
      for(std::map<AtomNumber,Tensor>::const_iterator p=a->gradients.begin();p!=a->gradients.end();++p){
        gradients[(*p).first]+=matmul(derivatives[i],(*p).second);
      }
    // this case if the atom is a normal one 	 
    } else {
      gradients[an]+=derivatives[i];
    }
  }
}


void ActionWithVirtualAtom::setGradientsIfNeeded(){
  if(isOptionOn("GRADIENTS")) { 
	setGradients() ;	
  }
}

}
