/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2023 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

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
#include "core/ActionShortcut.h"
#include "core/ActionRegister.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/Group.h"
#include "MultiColvarShortcuts.h"
#include <string>
#include <cmath>

//+PLUMEDOC MCOLVAR DISTANCES
/*
Calculate the distances between multiple piars of atoms

\par Examples

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace multicolvar {

class Distances : public ActionShortcut {
public:
  static void registerKeywords(Keywords& keys);
  explicit Distances(const ActionOptions&);
};

PLUMED_REGISTER_ACTION(Distances,"DISTANCES")

void Distances::registerKeywords(Keywords& keys) {
  ActionShortcut::registerKeywords( keys );
  keys.add("numbered","ATOMS","the pairs of atoms that you would like to calculate the angles for");
  keys.addFlag("NOPBC",false,"ignore the periodic boundary conditions when calculating distances");
  keys.addFlag("COMPONENTS",false,"calculate the x, y and z components of the distance separately and store them as label.x, label.y and label.z");
  keys.addFlag("SCALED_COMPONENTS",false,"calculate the a, b and c scaled components of the distance separately and store them as label.a, label.b and label.c");
  keys.reset_style("ATOMS","atoms"); MultiColvarShortcuts::shortcutKeywords( keys );
  keys.add("atoms","ORIGIN","calculate the distance of all the atoms specified using the ATOMS keyword from this point");
  keys.add("numbered","LOCATION","the location at which the CV is assumed to be in space");
  keys.reset_style("LOCATION","atoms");
  keys.addOutputComponent("x","COMPONENTS","the x-components of the distance vectors");
  keys.addOutputComponent("y","COMPONENTS","the y-components of the distance vectors");
  keys.addOutputComponent("z","COMPONENTS","the z-components of the distance vectors");
}

Distances::Distances(const ActionOptions& ao):
  Action(ao),
  ActionShortcut(ao)
{
  // Create distances
  std::string dline = getShortcutLabel() + ": DISTANCE_VECTOR";
  bool nopbc; parseFlag("NOPBC",nopbc); if( nopbc ) dline += " NOPBC";
  bool comp; parseFlag("COMPONENTS",comp); if( comp ) dline += " COMPONENTS";
  bool scomp; parseFlag("SCALED_COMPONENTS",scomp); if( scomp ) dline += " SCALED_COMPONENTS";
  // Parse origin
  std::string num, ostr; parse("ORIGIN",ostr);
  if( ostr.length()>0 ) {
    // Parse atoms
    std::vector<std::string> afstr, astr; parseVector("ATOMS",astr); Tools::interpretRanges(astr);
    for(unsigned i=0; i<astr.size(); ++i) {
      Group* mygr=plumed.getActionSet().selectWithLabel<Group*>(astr[i]);
      if( mygr ) {
        std::vector<std::string> grstr( mygr->getGroupAtoms() );
        for(unsigned j=0; j<grstr.size(); ++j) afstr.push_back(grstr[j]);
      } else {
        Group* mygr2=plumed.getActionSet().selectWithLabel<Group*>(astr[i] + "_grp");
        if( mygr2 ) {
          std::vector<std::string> grstr( mygr2->getGroupAtoms() );
          for(unsigned j=0; j<grstr.size(); ++j) afstr.push_back(grstr[j]);
        } else afstr.push_back(astr[i]);
      }
    }
    for(unsigned i=0; i<afstr.size(); ++i) { Tools::convert( i+1, num ); dline += " ATOMS" + num + "=" + ostr + "," + afstr[i]; }
  } else {
    std::string grpstr = getShortcutLabel() + "_grp: GROUP ATOMS=";
    for(unsigned i=1;; ++i) {
      std::string atstring; parseNumbered("ATOMS",i,atstring);
      if( atstring.length()==0 ) break;
      std::string locstr; parseNumbered("LOCATION",i,locstr);
      if( locstr.length()==0 ) {
        std::string num; Tools::convert( i, num );
        readInputLine( getShortcutLabel() + "_vatom" + num + ": CENTER ATOMS=" + atstring );
        if( i==1 ) grpstr += getShortcutLabel() + "_vatom" + num; else grpstr += "," + getShortcutLabel() + "_vatom" + num;
      } else {
        if( i==1 ) grpstr += locstr; else grpstr += "," + locstr;
      }
      std::string num; Tools::convert( i, num );
      dline += " ATOMS" + num + "=" + atstring;
    }
    readInputLine( grpstr );
  }
  log.printf("Action DISTANCE\n");
  log.printf("  with label %s \n", getShortcutLabel().c_str() );
  readInputLine( dline );
  // Add shortcuts to label
  MultiColvarShortcuts::expandFunctions( getShortcutLabel(), getShortcutLabel(), "", this );
}

}
}
