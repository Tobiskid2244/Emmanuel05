/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2015-2019 The plumed team
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
#ifndef __PLUMED_gridtools_ActionWithInputGrid_h
#define __PLUMED_gridtools_ActionWithInputGrid_h

#include "core/ActionWithValue.h"
#include "core/ActionWithArguments.h"
#include "GridCoordinatesObject.h"

namespace PLMD {
namespace gridtools {

class ActionWithInputGrid :
  public ActionWithValue,
  public ActionWithArguments {
private:
  void doTheCalculation();
protected:
  bool firststep;
  bool set_zero_outside_range;
  GridCoordinatesObject gridobject;
  void setupGridObject();
  double getFunctionValue( const unsigned& ipoint ) const ;
  double getFunctionValue( const std::vector<unsigned>& ip ) const ;
  double getFunctionValueAndDerivatives( const std::vector<double>& x, std::vector<double>& der ) const ;
public:
  static void registerKeywords( Keywords& keys );
  explicit ActionWithInputGrid(const ActionOptions&ao);
  virtual unsigned getNumberOfDerivatives() const ;
  virtual void finishOutputSetup() = 0;
  virtual void jobsAfterLoop() {}
  void calculate();
  virtual void apply() {};
  void update();
  void runFinalJobs();
};

inline
double ActionWithInputGrid::getFunctionValue( const unsigned& ipoint ) const {
  return getPntrToArgument(0)->get( ipoint );
}

inline
double ActionWithInputGrid::getFunctionValue( const std::vector<unsigned>& ip ) const {
  return getPntrToArgument(0)->get( gridobject.getIndex(ip) );
}

inline
unsigned ActionWithInputGrid::getNumberOfDerivatives() const {
  return getPntrToArgument(0)->getShape().size();
}

}
}
#endif

