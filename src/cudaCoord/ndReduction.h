/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2023 The plumed team
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
#ifndef __PLUMED_cuda_ndReduction_h
#define __PLUMED_cuda_ndReduction_h
#include "plumed/tools/Vector.h"
#include "plumed/tools/Tensor.h"
#include "cudaHelpers.cuh"
#include <vector>
namespace PLMD{
namespace CUDAHELPERS {
  /// @brief reduce the component of a nat x 3 x nthreads vector
  /// @return a std::vector that contains the the reduced nat Vectors
  /// We are using nat as the number of vector because usually this is used to
  /// reduce the per atom derivatives
  /** @brief reduce the component of a 3 x nat x N vector
   *  @param cudaNVectorAddress the pointer to the memory in cuda
   *  @param N the number of Tensors to reduce
   *  @param nat the number 
   *  @param maxNumThreads limits the number of threads per block to be used 
   *  @return the reduced Vector
   * 
   * reduceTensor expects that cudaNVectorAddress is initializated with
   * cudaMalloc(&cudaVectorAddress,  N * nat * 3 * sizeof(double)).
   * The componensts of cudaVectorAddress in memory shoud be organized like
   *  [x0_0, y0_0, z0_0, ..., x0_(nat-1), y0_(nat-1), z0_(nat-1),
   *   x1_0, y1_0, z1_0, ..., x1_(nat-1), y1_(nat-1), z1_(nat-1),
   *      ...
   *  x(N-1)_0, y(N-1)_0, z(N-1)_0, ..., x(N-1)_(nat-1), y(N-1)_(nat-1), z(N-1)_(nat-1)(N-1)
   * 
   * The algorithm will decide the best number of threads to be used in 
   * an extra nthreads * 3 * sizeof(double) will be allocated on the GPU,
   * where nthreads is the total number of threads that will be used
   * 
   * @note cudaNVectorAddress is threated as not owned: the user will need to call cudaFree on it!!!
   */
  std::vector<Vector> reduceNVectors(double* cudaNVectorAddress, unsigned N, unsigned nat, unsigned maxNumThreads=512);

//THIS DOES NOT KEEP THE DATA SAFE
std::vector<Vector> reduceNVectors(memoryHolder<double>& cudaNVectorAddress,
 memoryHolder<double>& memoryHelper, 
unsigned N, unsigned nat, unsigned maxNumThreads);

  /** @brief reduce the component of a 3 x N vector
   *  @param cudaVectorAddress the pointer to the memory in cuda
   *  @param N the number of Vectors to reduce
   *  @param maxNumThreads limits the number of threads per block to be used 
   *  @return the reduced Vector
   * 
   * reduceTensor expects that cudaVectorAddress is initializated with
   * cudaMalloc(&cudaVectorAddress,  N * 3 * sizeof(double)).
   * The componensts of cudaVectorAddress in memory shoud be organized like
   *  [x0, y0, z0, x1, y1, z1 ... x(N-1), y(N-1), z(N-1)]
   * 
   * The algorithm will decide the best number of threads to be used in 
   * an extra nthreads * 3 * sizeof(double) will be allocated on the GPU,
   * where nthreads is the total number of threads that will be used
   * 
   * @note cudaVectorAddress is threated as not owned: the user will need to call cudaFree on it!!!
   */
  Vector reduceVector(double* cudaVectorAddress, unsigned N, unsigned maxNumThreads=512);

  /** @brief reduces a Tensor from 3x3 x N to 3x3
   *  @param cudaTensorAddress the pointer to the memory in cuda
   *  @param N the number of Tensors to reduce
   *  @param maxNumThreads limits the number of threads per block to be used 
   *  @return the reduced Tensor
   * 
   * reduceTensor expects that cudaTensorAddress is initializated with
   * cudaMalloc(&cudaTensorAddress,  N * 9 * sizeof(double)).
   * The componensts of cudaVectorAddress in memory shoud be stred in organized in N sequential blocks 
   * whose compontents shall be [(0,0) (0,1) (0,2) (1,0) (1,1) (1,2) (2,0) (2,1) (2,2)]
   * 
   * The algorithm will decide the best number of threads to be used in 
   * an extra nthreads * 9 * sizeof(double) will be allocated on the GPU,
   * where nthreads is the total number of threads that will be used
   * 
   * @note cudaTensorAddress is threated as not owned: the user will need to call cudaFree on it!!!
   */ 
  Tensor reduceTensor(double* cudaTensorAddress, unsigned N, unsigned maxNumThreads=512);

//THIS DOES NOT KEEP THE DATA SAFE
Tensor reduceTensor(memoryHolder<double>& cudaTensorAddress,
 memoryHolder<double>& memoryHelper, 
unsigned N, unsigned maxNumThreads);

  /** @brief reduce the component of a N vector to a scalar
   *  @param cudaScalarAddress the pointer to the memory in cuda
   *  @param N the number of scalar to reduce
   *  @param maxNumThreads limits the number of threads per block to be used 
   *  @return the reduced scalar
   * 
   * reduceTensor expects that cudaVectorAddress is initializated with
   * cudaMalloc(&cudaVectorAddress,  N * sizeof(double)).
   * The componensts of cudaScalarAddress in memory shoud be organized like
   *  [x0, x1, x2 ...,  x(N-1)]
   * 
   * The algorithm will decide the best number of threads to be used in 
   * an extra nthreads * sizeof(double) will be allocated on the GPU,
   * where nthreads is the total number of threads that will be used
   * 
   * @note cudaScalarAddress is threated as not owned: the user will need to call cudaFree on it!!!
   */ 
  double reduceScalar(double* cudaScalarAddress, unsigned N, unsigned maxNumThreads=512);
  double reduceScalar(memoryHolder<double>& cudaScalarAddress,
 memoryHolder<double>& memoryHelper, unsigned N, unsigned maxNumThreads=512);

 struct DVS{
  std::vector<Vector> deriv;
  Tensor virial;
  double scalar;
  DVS(unsigned nat);
};

/** @brief reduces the coordination, the derivatives and the virial from a coordination calculation
  *  @param derivativeIn the 3 * N array to be reduced with the compontent sof the derivatives
  *  @param scalarIn the N array to be reduced with the values of the coordination
  *  @param memoryHelperD preallocated GPU memory for speed up (for the derivatives)
  *  @param memoryHelperV preallocated GPU memory for speed up (for the virial)
  *  @param memoryHelperS preallocated GPU memory for speed up (for the scalar/coordination)
  *  @param streamDerivatives preinitializated stream for concurrency (for the derivatives)
  *  @param streamVirial preinitializated stream for concurrency (for the virial)
  *  @param streamScalar preinitializated stream for concurrency (for the scalar/coordination)
  *  @param cudaScalarAddress the pointer to the memory in cuda
  *  @param N the number of scalar to reduce
  *  @param nat the number of atoms
  *  @param maxNumThreads limits the number of threads per block to be used 
  *  @return the derivative, the virial and the coordination
  * 
  * reduceDVS 
  * The memory helpers will be resized if needed (the memory occupied may be increased but not reduced)
  * 
  * The algorithm will decide the best number of threads to be used in 
  * an extra nthreads * sizeof(double) will be allocated on the GPU,
  * where nthreads is the total number of threads that will be used
  * 
  * @note cudaScalarAddress is threated as not owned: the user will need to call cudaFree on it!!!
  */ 
DVS reduceDVS(
  memoryHolder<double>& derivativeIn,
  memoryHolder<double>& virialIn,
  memoryHolder<double>& scalarIn,
  memoryHolder<unsigned>& pairListIn,
  memoryHolder<double>& memoryHelperV,
  memoryHolder<double>& memoryHelperS,
  cudaStream_t streamVirial,
  cudaStream_t streamScalar,
  unsigned N, unsigned nat,
  unsigned maxNumThreads=512);

} //namespace CUDAHELPERS
} //namespace PLMD
#endif //__PLUMED_cuda_ndReduction_h