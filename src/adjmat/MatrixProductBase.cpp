/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2015-2017 The plumed team
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
#include "MatrixProductBase.h"
#include "AdjacencyMatrixBase.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"

namespace PLMD {
namespace adjmat {

void MatrixProductBase::registerKeywords( Keywords& keys ) {
  Action::registerKeywords( keys ); ActionAtomistic::registerKeywords( keys );
  ActionWithArguments::registerKeywords( keys ); ActionWithValue::registerKeywords( keys );
  keys.remove("NUMERICAL_DERIVATIVES"); keys.use("ARG");
}

MatrixProductBase::MatrixProductBase(const ActionOptions& ao):
  Action(ao),
  ActionAtomistic(ao),
  ActionWithArguments(ao),
  ActionWithValue(ao),
  skip_ieqj(false),
  isAdjacencyMatrix(false)
{
  if( getNumberOfArguments()>0 ) {
      if( getNumberOfArguments()!=2 ) error("should only have two arguments");
      for(unsigned i=0; i<2; ++i) {
          if( getPntrToArgument(i)->getRank()==0 || getPntrToArgument(i)->hasDerivatives() ) error("arguments should be matrices or vectors");
      }
      std::vector<unsigned> shape( getMatrixShapeForFinalTasks() ); 
      // Rerequest arguments 
      std::vector<Value*> args( getArguments() ); requestArguments( args, false ); 
      // Create a list of tasks for this action - n.b. each task calculates one row of the matrix
      for(unsigned j=0; j<shape[0]; ++j ) addTaskToList(j);
      // And create the matrix to hold the dot products
      addValue( shape ); 

      // Now do some stuff for time series
      bool timeseries=getPntrToArgument(0)->isTimeSeries();
      if( timeseries ) {
          for(unsigned i=1;i<getNumberOfArguments();++i) {
              if( !getPntrToArgument(i)->isTimeSeries() ) error("all arguments should either be time series or not time series");
          }
          getPntrToOutput(0)->makeTimeSeries();
      } else {
          for(unsigned i=1;i<getNumberOfArguments();++i) {
              if( getPntrToArgument(i)->isTimeSeries() ) error("all arguments should either be time series or not time series");
          }
      }
  }
}

unsigned MatrixProductBase::getNumberOfDerivatives() const {
  unsigned numargs = 0; if( getNumberOfArguments()>0 ) numargs = getPntrToArgument(0)->getSize() + getPntrToArgument(1)->getSize(); 
  if( getNumberOfAtoms()>0 ) return 3*getNumberOfAtoms() + 9 + numargs;
  return numargs;
}

bool MatrixProductBase::mustBeTreatedAsDistinctArguments() const {
  const AdjacencyMatrixBase* ab=dynamic_cast<const AdjacencyMatrixBase*>(this);
  if( ab ) return ActionWithArguments::mustBeTreatedAsDistinctArguments();
  return true;
}

void MatrixProductBase::getTasksForParent( const std::string& parent, std::vector<std::string>& actionsThatSelectTasks, std::vector<unsigned>& tflags ) {
  if( tflags.size()!=getFullNumberOfTasks() ) return;
  // Check if parent has already been added
  bool found=false;
  for(unsigned i=0;i<actionsThatSelectTasks.size();++i) {
      if( actionsThatSelectTasks[i]==parent ) { found=true; break; }
  }
  if( found ) return;
  // Get the flags for this chain
  std::vector<unsigned> lflags( tflags.size(), 0 ); std::vector<unsigned> pTaskList, pIndexList;
  unsigned n_active = setTaskFlags( lflags, pTaskList, pIndexList );
  // Check if anything has been deactivated downstream
  if( n_active==tflags.size() ) return;
  // And retrieve non zero elements of contact matrix we are multiplying this by
  AdjacencyMatrixBase* ab = dynamic_cast<AdjacencyMatrixBase*>( getActionThatCalculates() );
  if( ab ) {
      // If tasks are deactivated in this child we can deactivate things in parent
      actionsThatSelectTasks.push_back( parent );
      // Get the atoms so that we can setup the neightbor lists
      ab->retrieveAtoms();
      // Now prepare the input matrix for the task loop
      ab->prepareForTasks( n_active, pTaskList );
      // Get the neighbours of each of the active atoms
      std::vector<unsigned> indices( getFullNumberOfTasks() );
      for(unsigned i=0;i<n_active;++i) {
          unsigned nneigh = ab->retrieveNeighbours( pTaskList[i], indices );
          for(unsigned j=0;j<nneigh;++j) tflags[indices[j]] = 1;
      }
      unsigned nacc = 0;
      for(unsigned i=0; i<tflags.size(); ++i) {
        if( tflags[i]>0 ) nacc++;
      }
  }
}

void MatrixProductBase::lockRequests() {
  ActionWithArguments::lockRequests();
  ActionAtomistic::lockRequests();
}

void MatrixProductBase::unlockRequests() {
  ActionWithArguments::unlockRequests();
  ActionAtomistic::unlockRequests();
}

void MatrixProductBase::calculateNumericalDerivatives( ActionWithValue* a ) { plumed_error(); }

void MatrixProductBase::calculate() {
  if( actionInChain() || skipCalculate() ) return;
  runAllTasks();
}

void MatrixProductBase::update() {
  if( skipUpdate() ) return;
  plumed_dbg_assert( !actionInChain() );
  if( getFullNumberOfTasks()>0 ) runAllTasks();
}

void MatrixProductBase::runFinalJobs() {
  if( skipUpdate() ) return;
  plumed_dbg_assert( !actionInChain() );
  resizeForFinalTasks();
  runAllTasks();
}

unsigned MatrixProductBase::getNumberOfFinalTasks() {
  return getMatrixShapeForFinalTasks()[0];
}

std::vector<unsigned> MatrixProductBase::getMatrixShapeForFinalTasks() {
  std::vector<unsigned> shape(2);
  if( getPntrToArgument(0)->getRank()==1 && getPntrToArgument(1)->getRank()==1 ) {
      shape[0]=getPntrToArgument(1)->getShape()[0]; shape[1]=getPntrToArgument(0)->getShape()[0];
  } else if( getPntrToArgument(0)->getRank()==2 && getPntrToArgument(1)->getRank()==2 ) {
      if( getPntrToArgument(0)->getShape()[1]!=getPntrToArgument(1)->getShape()[0] ) error("number of columns in first matrix is not equal to number of columns in second");
      shape[0]=getPntrToArgument(0)->getShape()[0]; shape[1]=getPntrToArgument(1)->getShape()[1];

      // Check if we are multiplying a matrix by its transpose (if we are doing this we know the diagonal elements are all 1 or something similarly boring)
      if( (getPntrToArgument(0)->getPntrToAction())->getName()=="TRANSPOSE" ) {
           ActionWithArguments* aa = dynamic_cast<ActionWithArguments*>( getPntrToArgument(0)->getPntrToAction() );
           if( (aa->getPntrToArgument(0))->getName()==getPntrToArgument(1)->getName() && (getPntrToArgument(1)->getPntrToAction())->getName().find("STACK")!=std::string::npos ) skip_ieqj=true;
      } else if( (getPntrToArgument(1)->getPntrToAction())->getName()=="TRANSPOSE" ) {
           ActionWithArguments* aa = dynamic_cast<ActionWithArguments*>( getPntrToArgument(1)->getPntrToAction() );
           if( (aa->getPntrToArgument(0))->getName()==getPntrToArgument(0)->getName() && (getPntrToArgument(0)->getPntrToAction())->getName().find("STACK")!=std::string::npos ) skip_ieqj=true;
      }
  } else error("cannot do product of matrix and vector");
  return shape;
}

void MatrixProductBase::updateCentralMatrixIndex( const unsigned& ind, const std::vector<unsigned>& indices, MultiValue& myvals ) const {
  for(unsigned k=0; k<getNumberOfComponents(); ++k ) {
      unsigned nmat = getPntrToOutput(k)->getPositionInMatrixStash(), nmat_ind = myvals.getNumberOfMatrixIndices( nmat );
      std::vector<unsigned>& matrix_indices( myvals.getMatrixIndices( nmat ) ); unsigned invals=getFullNumberOfTasks(); 

      unsigned numargs = 0;
      if( getNumberOfArguments()>0 ) {
          unsigned nargs = 1; if( getPntrToArgument(0)->getRank()==2 ) nargs = getPntrToArgument(0)->getShape()[1];
          for(unsigned i=0; i<nargs; ++i) { matrix_indices[nmat_ind] = nargs*ind + i; nmat_ind++; }
          numargs = getPntrToArgument(0)->getSize() + getPntrToArgument(1)->getSize();
      }
      if( getNumberOfAtoms()>0 ) {
        matrix_indices[nmat_ind+0]=numargs + 3*ind+0;
        matrix_indices[nmat_ind+1]=numargs + 3*ind+1;
        matrix_indices[nmat_ind+2]=numargs + 3*ind+2;
        nmat_ind+=3; 
        for(unsigned i=myvals.getSplitIndex(); i<myvals.getNumberOfIndices(); ++i) { 
            matrix_indices[nmat_ind+0]=numargs + 3*indices[i]+0;
            matrix_indices[nmat_ind+1]=numargs + 3*indices[i]+1;
            matrix_indices[nmat_ind+2]=numargs + 3*indices[i]+2;
            nmat_ind+=3;
        }
        unsigned virbase = numargs + 3*getNumberOfAtoms();
        for(unsigned i=0; i<9; ++i) matrix_indices[nmat_ind+i]=virbase+i;
        nmat_ind+=9; 
      }
      myvals.setNumberOfMatrixIndices( nmat, nmat_ind );
  }
}

unsigned MatrixProductBase::getNumberOfColumns() const { 
  plumed_massert( !actionInChain(), "I am not sure how to do this so I am not allowing it GAT");  
  return getPntrToOutput(0)->getShape()[1];
}

void MatrixProductBase::setupForTask( const unsigned& current, MultiValue& myvals, std::vector<unsigned> & indices, std::vector<Vector>& atoms ) const {
  unsigned size_v = getPntrToOutput(0)->getShape()[1]; 
  if( skip_ieqj ) {
      if( indices.size()!=size_v ) indices.resize( size_v ); 
  } else if( indices.size()!=size_v+1 ) indices.resize( size_v+1 );
  unsigned k=1; indices[0]=current; unsigned start_n = getFullNumberOfTasks();
  for(unsigned i=0; i<size_v; ++i) {
      if( skip_ieqj && myvals.getTaskIndex()==i ) continue;
      indices[k]=start_n + i; k++;
  }
  myvals.setSplitIndex( indices.size() ); myvals.setNumberOfIndices( indices.size() );
}

void MatrixProductBase::performTask( const unsigned& current, MultiValue& myvals ) const {
  std::vector<unsigned> & indices( myvals.getIndices() );
  if( !isAdjacencyMatrix && actionInChain() ) {
    // If this is not an adjacency matrix then have done the relevant calculations during the first pass through the loop 
    if( !doNotCalculateDerivatives() && myvals.inVectorCall() ) updateCentralMatrixIndex( myvals.getTaskIndex(), indices, myvals );
    return ;
  }
  std::vector<Vector> & atoms( myvals.getFirstAtomVector() );
  setupForTask( current, myvals, indices, atoms );

  // Now loop over all atoms in coordination sphere
  unsigned ntwo_atoms = myvals.getSplitIndex();
  for(unsigned i=1; i<ntwo_atoms; ++i) {
    // This does everything in the stream that is done with single matrix elements
    runTask( getLabel(), myvals.getTaskIndex(), current, indices[i], myvals );
    // Now clear only elements that are not accumulated over whole row
    clearMatrixElements( myvals );
  }
  // Update the matrix index for the central atom
  if( !doNotCalculateDerivatives() ) updateCentralMatrixIndex( myvals.getTaskIndex(), indices, myvals );
}

void MatrixProductBase::updateAtomicIndices( const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const {
  unsigned narg_derivatives = 0; if( getNumberOfArguments()>0 ) narg_derivatives = getPntrToArgument(0)->getSize() + getPntrToArgument(1)->getSize();
  unsigned w_ind = getPntrToOutput(0)->getPositionInStream();
  // Update dynamic list indices for central atom
  myvals.updateIndex( w_ind, narg_derivatives + 3*index1+0 ); myvals.updateIndex( w_ind, narg_derivatives + 3*index1+1 ); myvals.updateIndex( w_ind, narg_derivatives + 3*index1+2 );
  // Update dynamic list indices for atom forming this bond
  myvals.updateIndex( w_ind, narg_derivatives + 3*index2+0 ); myvals.updateIndex( w_ind, narg_derivatives + 3*index2+1 ); myvals.updateIndex( w_ind, narg_derivatives + 3*index2+2 );
  // Now look after all the atoms in the third block
  std::vector<unsigned> & indices( myvals.getIndices() );
  for(unsigned i=myvals.getSplitIndex(); i<myvals.getNumberOfIndices(); ++i) {
    myvals.updateIndex( w_ind, narg_derivatives + 3*indices[i]+0 ); myvals.updateIndex( w_ind, narg_derivatives + 3*indices[i]+1 ); myvals.updateIndex( w_ind, narg_derivatives + 3*indices[i]+2 );
  }
  // Update dynamic list indices for virial
  unsigned base = narg_derivatives + 3*getNumberOfAtoms(); for(unsigned j=0; j<9; ++j) myvals.updateIndex( w_ind, base+j );
  // Matrix indices
  if( !myvals.inMatrixRerun() ) {
      unsigned nmat = getPntrToOutput(0)->getPositionInMatrixStash(), nmat_ind = myvals.getNumberOfMatrixIndices( nmat );
      std::vector<unsigned>& matrix_indices( myvals.getMatrixIndices( nmat ) );
      matrix_indices[nmat_ind+0]=narg_derivatives + 3*index2+0; matrix_indices[nmat_ind+1]=narg_derivatives + 3*index2+1; matrix_indices[nmat_ind+2]=narg_derivatives + 3*index2+2;
      nmat_ind+=3; myvals.setNumberOfMatrixIndices( nmat, nmat_ind );
  }
}

bool MatrixProductBase::performTask( const std::string& controller, const unsigned& index1, const unsigned& index2, MultiValue& myvals ) const {
  // This makes sure other AdjacencyMatrixBase actions in the stream don't get their matrix elements calculated here
  if( isAdjacencyMatrix && controller!=getLabel() ) return false; 
  // Now do the calculation
  unsigned sss=0, nargs=0, ind2 = index2; if( index2>=getFullNumberOfTasks() ) ind2 = index2 - getFullNumberOfTasks();
  if( getNumberOfArguments()>0 ) {
      sss=1; if( getPntrToArgument(1)->getRank()==2 ) sss=getPntrToArgument(1)->getShape()[1];
      nargs=1; if( getPntrToArgument(0)->getRank()==2 ) nargs = getPntrToArgument(0)->getShape()[1];
  }
  std::vector<double> args1(nargs), args2(nargs), der1(nargs), der2(nargs);
  for(unsigned i=0; i<nargs; ++i) {
    args1[i] = getPntrToArgument(0)->get( index1*nargs + i );
    args2[i] = getPntrToArgument(1)->get( i*sss + ind2 ); 
  }
  double val = computeVectorProduct( index1, index2, args1, args2, der1, der2, myvals );
  if( abs(val)<epsilon ) {
      if( !doNotCalculateDerivatives() ) {
          if( getNumberOfAtoms()>0 ) updateAtomicIndices( index1, index2, myvals );
          clearMatrixElements( myvals );
      }
      return false;       
  }
  unsigned ostrn = getPntrToOutput(0)->getPositionInStream();
  myvals.setValue( ostrn, val );
  // Return after calculation of value if we do not need derivatives
  if( doNotCalculateDerivatives() ) return true;

  unsigned nmat = getPntrToOutput(0)->getPositionInMatrixStash();
  std::vector<unsigned>& matrix_indices( myvals.getMatrixIndices( nmat ) );
  plumed_dbg_assert( matrix_indices.size()>=getNumberOfDerivatives() );
  unsigned nmat_ind = myvals.getNumberOfMatrixIndices( nmat );
  unsigned jind_start = 0; 
  if( getNumberOfArguments()>0 ) jind_start = getPntrToArgument(0)->getSize(); 
  for(unsigned i=0; i<nargs; ++i) {
    plumed_dbg_assert( nargs*index1 + i<myvals.getNumberOfDerivatives() );
    myvals.addDerivative( ostrn, nargs*index1 + i, der1[i] );
    myvals.updateIndex( ostrn, nargs*index1 + i );
    plumed_dbg_assert( jind_start + i*sss + ind2<myvals.getNumberOfDerivatives() );
    myvals.addDerivative( ostrn, jind_start + i*sss + ind2, der2[i] );
    myvals.updateIndex( ostrn, jind_start + i*sss + ind2 );
    if( !myvals.inMatrixRerun() ) { matrix_indices[nmat_ind] = jind_start + i*sss + ind2; nmat_ind++; }
  }
  myvals.setNumberOfMatrixIndices( nmat, nmat_ind );
  if( getNumberOfAtoms()>0 ) updateAtomicIndices( index1, index2, myvals );
  return true;
}

void MatrixProductBase::apply() {
  if( doNotCalculateDerivatives() ) return;
  std::fill(forcesToApply.begin(),forcesToApply.end(),0); unsigned mm=0;
  if( getForcesFromValues( forcesToApply ) ) {
    setForcesOnAtoms( forcesToApply, mm );
    setForcesOnArguments( 0, forcesToApply, mm );
  }
}

}
}
