/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2016-2020 The plumed team
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
#include "AverageBase.h"
#include "PlumedMain.h"
#include "ActionSet.h"
#include "ActionRegister.h"

//+PLUMEDOC GRIDCALC AVERAGE
/*
Calculate the ensemble average of a collective variable

The ensemble average for a non-periodic, collective variable, \f$s\f$ is given by the following expression:

\f[
\langle s \rangle = \frac{ \sum_{t'=0}^t w(t') s(t') }{ \sum_{t'=0}^t w(t') }
\f]

Here the sum runs over a the trajectory and \f$s(t')\f$ is used to denote the value of the collective variable
at time \f$t'\f$.  The final quantity evalulated is a weighted
average as the weights, \f$w(t')\f$, allow us to negate the effect any bias might have on the region of phase space
sampled by the system.  This is discussed in the section of the manual on \ref Analysis.

When the variable is periodic (e.g. \ref TORSION) and has a value, \f$s\f$, in \f$a \le s \le b\f$ the ensemble average is evaluated using:

\f[
\langle s \rangle = a + \frac{b - a}{2\pi} \arctan \left[ \frac{ \sum_{t'=0}^t w(t') \sin\left( \frac{2\pi [s(t')-a]}{b - a} \right) }{ \sum_{t'=0}^t w(t') \cos\left( \frac{2\pi [s(t')-a]}{b - a} \right) } \right]
\f]

\par Examples

The following example calculates the ensemble average for the distance between atoms 1 and 2
and output this to a file called COLVAR.  In this example it is assumed that no bias is acting
on the system and that the weights, \f$w(t')\f$ in the formulas above can thus all be set equal
to one.

\plumedfile
d1: DISTANCE ATOMS=1,2
d1a: AVERAGE ARG=d1
PRINT ARG=d1a FILE=colvar STRIDE=100
\endplumedfile

The following example calculates the ensemble average for the torsional angle involving atoms 1, 2, 3 and 4.
At variance with the previous example this quantity is periodic so the second formula in the above introduction
is used to calculate the average.  Furthermore, by using the CLEAR keyword we have specified that block averages
are to be calculated.  Consequently, after 100 steps all the information aquired thus far in the simulation is
forgotten and the process of averaging is begun again.  The quantities output in the colvar file are thus the
block averages taken over the first 100 frames of the trajectory, the block average over the second 100 frames
of trajectory and so on.

\plumedfile
t1: TORSION ATOMS=1,2,3,4
t1a: AVERAGE ARG=t1 CLEAR=100
PRINT ARG=t1a FILE=colvar STRIDE=100
\endplumedfile

This third example incorporates a bias.  Notice that the effect the bias has on the ensemble average is removed by taking
advantage of the \ref REWEIGHT_BIAS method.  The final ensemble averages output to the file are thus block ensemble averages for the
unbiased canononical ensemble at a temperature of 300 K.

\plumedfile
t1: TORSION ATOMS=1,2,3,4
RESTRAINT ARG=t1 AT=pi KAPPA=100.
ww: REWEIGHT_BIAS TEMP=300
t1a: AVERAGE ARG=t1 LOGWEIGHTS=ww CLEAR=100
PRINT ARG=t1a FILE=colvar STRIDE=100
\endplumedfile

*/
//+ENDPLUMEDOC

namespace PLMD {

class Average : public AverageBase {
private:
  enum {t,f,ndata} normalization;
  double lbound, pfactor;
  std::vector<Vector> displacements;
public:
  static void registerKeywords( Keywords& keys );
  explicit Average( const ActionOptions& );
  bool allowComponentsAndValue() const { return true; }
  void setReferenceConfig();
  void accumulate( const std::vector<std::vector<Vector> >& dir  );

  void calculate();
};

PLUMED_REGISTER_ACTION(Average,"AVERAGE")

void Average::registerKeywords( Keywords& keys ) {
  AverageBase::registerKeywords( keys );
  keys.add("optional","ARG","the quantity that we are calculating an ensemble average for");
  keys.add("compulsory","NORMALIZATION","true","This controls how the data is normalized it can be set equal to true, false or ndata.  The differences between "
           "these options are explained in the manual page for \\ref HISTOGRAM");
  keys.addOutputComponent("sin","default","this value is only added when the input argument is periodic.  These tempory values are required as with periodic arguments we need to use Berry phase averages.");
  keys.addOutputComponent("cos","default","this value is only added when the input argument is periodic.  These tempory values are required as With periodic arguments we need to use Berry phase averages.");
}

Average::Average( const ActionOptions& ao):
  Action(ao),
  AverageBase(ao),
  lbound(0.0),pfactor(0.0)
{
  // Now read in the instructions for the normalization
  if( n_real_args>1 || n_real_args<0 ) error("cannot average more than one quantity at a time");
  if( getNumberOfArguments()>n_real_args ) {
      if( getNumberOfArguments()>n_real_args+1 ) error("too many arguments in input to average");
      if( getPntrToArgument(n_real_args)->getRank()>0 ) error("rank of input log weight is wrong"); 
  }
  std::string normstr; parse("NORMALIZATION",normstr);
  if( normstr=="true" ) { normalization=t; clearnorm=true; }
  else if( normstr=="false" ) { normalization=f; clearnorm=false; }
  else if( normstr=="ndata" ) { normalization=ndata; clearnorm=true; }
  else error("invalid instruction for NORMALIZATION flag should be true, false, or ndata");

  // Create a value
  if( n_real_args>0 ) {
     if( getNumberOfAtoms()>0 ) error("cannot average arguments and positions at same time");

     std::vector<unsigned> shape( getPntrToArgument(0)->getShape() ); 
     if( getPntrToArgument(0)->hasDerivatives() ) addValueWithDerivatives( shape );
     else addValue( shape );

     if( getPntrToArgument(0)->isPeriodic() ) {
       std::string min, max;
       getPntrToArgument(0)->getDomain( min, max ); setPeriodic( min, max );
       Tools::convert( min, lbound ); double ubound; Tools::convert( max, ubound );
       pfactor = ( ubound - lbound ) / (2*pi);
       addComponent( "sin", shape ); componentIsNotPeriodic( "sin" );
       addComponent( "cos", shape ); componentIsNotPeriodic( "cos" );
       if( normalization!=f ) { getPntrToOutput(1)->setNorm(0.0); getPntrToOutput(2)->setNorm(0.0); }
     } else {
       setNotPeriodic();
       if( normalization!=f ) getPntrToOutput(0)->setNorm(0.0);
     }
  } else if( getNumberOfAtoms()>0 ) {
      displacements.resize( mygroup.size() ); for(unsigned i=0;i<displacements.size();++i) displacements[i].zero();
      std::vector<unsigned> shape(1); shape[0]=3*getNumberOfAtoms(); addValue( shape ); setNotPeriodic();
  } else error("found nothing to average in input");
}

void Average::calculate() {
  if( n_real_args==0 || !firststep ) return;

  if( getPntrToOutput(0)->hasDerivatives() && getPntrToOutput(0)->getNumberOfValues( getLabel() )!=getPntrToArgument(0)->getNumberOfValues( getLabel() ) ) {
      getPntrToOutput(0)->setShape( getPntrToArgument(0)->getShape() );
  }
}

void Average::accumulate( const std::vector<std::vector<Vector> >& dir  ) {
  double lweight=0; if( getNumberOfArguments()>n_real_args ) lweight=getPntrToArgument(n_real_args)->get();
  double cweight = exp( lweight ); Value* val=getPntrToOutput(0);

  if( getNumberOfAtoms()>0 ) { 
      plumed_assert( dir.size()==1 );
      for(unsigned i=0;i<displacements.size();++i) {
          displacements[i] += cweight*dir[0][i];
          atoms.setVatomMass( mygroup[i], align[i] ); atoms.setVatomCharge( mygroup[i], displace[i] );
          Vector pos = getReferencePosition(i) + displacements[i] / val->getNorm();
          atoms.setVatomPosition( mygroup[i], pos );
          for(unsigned k=0;k<3;++k) val->set( 3*i + k, val->getNorm()*pos[k] );
      } 
  } 

  if( getPntrToOutput(0)->isPeriodic() ) {
      Value* valsin=getPntrToOutput(1); Value* valcos=getPntrToOutput(2);
      if( normalization==t ) { valsin->setNorm( valsin->getNorm() + cweight ); valcos->setNorm( valcos->getNorm() + cweight ); }
      else if( normalization==ndata ) { valsin->setNorm( valsin->getNorm() + 1.0 ); valcos->setNorm( valcos->getNorm() + 1.0 ); }
  } else {
      if( normalization==t ) val->setNorm( val->getNorm() + cweight );
      else if( normalization==ndata ) val->setNorm( val->getNorm() + 1.0 );
  }

  if( getNumberOfAtoms()==0 ) {
      Value* arg0=getPntrToArgument(0); unsigned nvals=arg0->getNumberOfValues( getLabel() );
      if( arg0->getRank()>0 && arg0->hasDerivatives() ) {
          // And accumulate the grid
          unsigned nder=val->getNumberOfDerivatives();
          for(unsigned i=0; i<nvals; ++i) {
              val->add( i*(1+nder), cweight*arg0->getRequiredValue(getLabel(), i) );
              for(unsigned j=0; j<nder; ++j) val->add( i*(1+nder)+1+j, cweight*arg0->getGridDerivative( i, j ) );
          }
      } else if( arg0->isPeriodic() ){
          Value* valsin=getPntrToOutput(1); Value* valcos=getPntrToOutput(2); 
          for(unsigned i=0;i<nvals;++i) {
              double tval = ( arg0->get(i) - lbound ) / pfactor;
              valsin->add( i, cweight*sin(tval) ); valcos->add( i, cweight*cos(tval) );
              val->set( i, lbound + pfactor*atan2( valsin->get(i), valcos->get(i)) );
          }
      } else {
          for(unsigned i=0;i<nvals;++i) val->add( i, cweight*arg0->get(i) );
      } 
  }
}

void Average::setReferenceConfig() {
  AverageBase::setReferenceConfig();
  for(unsigned i=0;i<displacements.size();++i) displacements[i].zero();
}

}
