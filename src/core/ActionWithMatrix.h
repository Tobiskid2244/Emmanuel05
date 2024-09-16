/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2023 The plumed team
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
#ifndef __PLUMED_core_ActionWithMatrix_h
#define __PLUMED_core_ActionWithMatrix_h

#include "ActionWithVector.h"

namespace PLMD {

class ActionWithMatrix : public ActionWithVector {
private:
  ActionWithMatrix* next_action_in_chain;
  ActionWithMatrix* matrix_to_do_before;
  ActionWithMatrix* matrix_to_do_after;
/// Update all the neighbour lists in the chain
  void updateAllNeighbourLists();
/// This clears all bookeeping arrays before the ith task
  void clearBookeepingBeforeTask( const unsigned& task_index ) const ;
/// This is used to clear up the matrix elements
  void clearMatrixElements( MultiValue& myvals ) const ;
/// This is used to find the total amount of space we need for storing matrix elements
  void setupMatrixStore();
/// This does the calculation of a particular matrix element
  void runTask( const std::string& controller, const unsigned& current, const unsigned colno, MultiValue& myvals ) const ;
protected:
/// This turns off derivative clearing for contact matrix if we are not storing derivatives
  bool clearOnEachCycle;
/// Does the matrix chain continue on from this action
  bool matrixChainContinues() const ;
/// This returns the jelem th element of argument ic
  double getArgumentElement( const unsigned& ic, const unsigned& jelem, const MultiValue& myvals ) const ;
/// This returns an element of a matrix that is passed an argument
  double getElementOfMatrixArgument( const unsigned& imat, const unsigned& irow, const unsigned& jcol, const MultiValue& myvals ) const ;
/// Add derivatives given the derivative wrt to the input vector element as input
  void addDerivativeOnVectorArgument( const bool& inchain, const unsigned& ival, const unsigned& jarg, const unsigned& jelem, const double& der, MultiValue& myvals ) const ;
/// Add derivatives given the derative wrt to the input matrix element as input
  void addDerivativeOnMatrixArgument( const bool& inchain, const unsigned& ival, const unsigned& jarg, const unsigned& irow, const unsigned& jcol, const double& der, MultiValue& myvals ) const ;
public:
  static void registerKeywords( Keywords& keys );
  explicit ActionWithMatrix(const ActionOptions&);
  virtual ~ActionWithMatrix();
///
  virtual bool isAdjacencyMatrix() const { return false; }
///
  void getAllActionLabelsInMatrixChain( std::vector<std::string>& mylabels ) const override ;
/// Get the first matrix in this chain
  const ActionWithMatrix* getFirstMatrixInChain() const ;
///
  void finishChainBuild( ActionWithVector* act );
/// This should return the number of columns to help with sparse storage of matrices
  virtual unsigned getNumberOfColumns() const = 0;
/// This requires some thought
  void setupStreamedComponents( const std::string& headstr, unsigned& nquants, unsigned& nmat, unsigned& maxcol ) override;
//// This does some setup before we run over the row of the matrix
  virtual void setupForTask( const unsigned& task_index, std::vector<unsigned>& indices, MultiValue& myvals ) const = 0;
/// Run over one row of the matrix
  virtual void performTask( const unsigned& task_index, MultiValue& myvals ) const ;
/// This is the virtual that will do the calculation of the task for a particular matrix element
  virtual void performTask( const std::string& controller, const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const = 0;
/// This is the jobs that need to be done when we have run all the jobs in a row of the matrix
  virtual void runEndOfRowJobs( const unsigned& ival, const std::vector<unsigned> & indices, MultiValue& myvals ) const = 0;
/// This is overwritten in Adjacency matrix where we have a neighbour list
  virtual void updateNeighbourList() {}
/// Run the calculation
  virtual void calculate() override;
/// Check if there are forces we need to account for on this task
  bool checkForTaskForce( const unsigned& itask, const Value* myval ) const override ;
/// This gathers the force on a particular value
  virtual void gatherForcesOnStoredValue( const unsigned& ival, const unsigned& itask, const MultiValue& myvals, std::vector<double>& forces ) const;
};

inline
bool ActionWithMatrix::matrixChainContinues() const {
  return matrix_to_do_after!=NULL;
}

inline
double ActionWithMatrix::getArgumentElement( const unsigned& ic, const unsigned& jelem, const MultiValue& myvals ) const {
  return getPntrToArgument(ic)->get( jelem );
}

inline
double ActionWithMatrix::getElementOfMatrixArgument( const unsigned& imat, const unsigned& irow, const unsigned& jcol, const MultiValue& myvals ) const {
  plumed_dbg_assert( imat<getNumberOfArguments() && getPntrToArgument(imat)->getRank()==2 && !getPntrToArgument(imat)->hasDerivatives() );
  return getArgumentElement( imat, irow*getPntrToArgument(imat)->getShape()[1] + jcol, myvals );
}

inline
void ActionWithMatrix::addDerivativeOnVectorArgument( const bool& inchain, const unsigned& ival, const unsigned& jarg, const unsigned& jelem, const double& der, MultiValue& myvals ) const {
  plumed_dbg_massert( jarg<getNumberOfArguments() && getPntrToArgument(jarg)->getRank()<2, "failing in action " + getName() + " with label " + getLabel() );
  unsigned vstart=arg_deriv_starts[jarg]; myvals.addDerivative( ival, vstart + jelem, der ); myvals.updateIndex( ival, vstart + jelem );
}

inline
void ActionWithMatrix::addDerivativeOnMatrixArgument( const bool& inchain, const unsigned& ival, const unsigned& jarg, const unsigned& irow, const unsigned& jcol, const double& der, MultiValue& myvals ) const {
  plumed_dbg_assert( jarg<getNumberOfArguments() && getPntrToArgument(jarg)->getRank()==2 && !getPntrToArgument(jarg)->hasDerivatives() );
  unsigned dloc = arg_deriv_starts[jarg] + irow*getPntrToArgument(jarg)->getNumberOfColumns() + jcol;
  myvals.addDerivative( ival, dloc, der ); myvals.updateIndex( ival, dloc );
}

}
#endif
