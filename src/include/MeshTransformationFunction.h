//
//  MeshTransformationFunction.h
//  Camellia-debug
//
//  Created by Nathan Roberts on 1/15/13.
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#include "Function.h"
#include "Mesh.h"

#ifndef Camellia_debug_MeshTransformationFunction_h
#define Camellia_debug_MeshTransformationFunction_h

class CellTransformationFunction;

class MeshTransformationFunction : public Function {
  map< int, FunctionPtr > _cellTransforms; // cellID --> cell transformation function
  EOperatorExtended _op;
protected:
  MeshTransformationFunction(map< int, FunctionPtr > cellTransforms, EOperatorExtended op);
public:
  MeshTransformationFunction(MeshPtr mesh, set<int> cellIDsToTransform); // might be responsible for only a subset of the curved cells.
  
  void values(FieldContainer<double> &values, BasisCachePtr basisCache);
  
  FunctionPtr dx();
  FunctionPtr dy();
  FunctionPtr dz();
};

#endif
