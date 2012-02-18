//
//  MeshRefinementTests.cpp
//  Camellia
//
//  Created by Nathan Roberts on 2/17/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "DofOrdering.h"
#include "MeshRefinementTests.h"
#include "BilinearFormUtility.h"

typedef Teuchos::RCP<DofOrdering> DofOrderingPtr;

void MeshRefinementTests::patchParentSideIndices(map<int,int> &parentSideIndices, Teuchos::RCP<Mesh> mesh, ElementPtr elem) {
  // returns map from child side index --> child index in the matching parent side (0 or 1)
  parentSideIndices.clear();
  int numSides = elem->numSides();
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    if (elem->getNeighborCellID(sideIndex) == -1) {
      int ancestralNeighborSideIndex;
      int ancestralNeighborCellID = mesh->ancestralNeighborForSide(elem, sideIndex, ancestralNeighborSideIndex)->cellID();
      if (ancestralNeighborCellID != -1) { // NOT boundary side
        int parentSideIndex = elem->parentSideForSideIndex(sideIndex);
        parentSideIndices[sideIndex] = elem->indexInParentSide(parentSideIndex);
      }
    }
  }
}

void MeshRefinementTests::preStiffnessExpectedUniform(FieldContainer<double> &preStiff, 
                                                      double h, ElementTypePtr elemType,
                                                      FieldContainer<double> &sideParities) {
  double h_ratio = h / 2.0; // because the master line has length 2...
  
  DofOrderingPtr trialOrder = elemType->trialOrderPtr;
  DofOrderingPtr testOrder = elemType->testOrderPtr;
  preStiff.resize(1,testOrder->totalDofs(),trialOrder->totalDofs());
  int testID  =  testOrder->getVarIDs()[0]; // just one
  int trialID = trialOrder->getVarIDs()[0]; // just one (the flux)
  
  int numPoints = 4;
  FieldContainer<double> refPoints2D(numPoints,2); // quad nodes (for bilinear basis)
  refPoints2D(0,0) = -1.0;
  refPoints2D(0,1) = -1.0;
  refPoints2D(1,0) =  1.0;
  refPoints2D(1,1) = -1.0;
  refPoints2D(2,0) =  1.0;
  refPoints2D(2,1) =  1.0;
  refPoints2D(3,0) = -1.0;
  refPoints2D(3,1) =  1.0;
  
  int phi_ordinals[numPoints]; // phi_i = 1 at node x_i, and 0 at other nodes
  
  double tol = 1e-15;
  BasisPtr testBasis = testOrder->getBasis(testID);
  FieldContainer<double> testValues(testBasis->getCardinality(),refPoints2D.dimension(0));
  testBasis->getValues(testValues, refPoints2D, Intrepid::OPERATOR_VALUE);
  for (int testOrdinal=0; testOrdinal<testBasis->getCardinality(); testOrdinal++) {
    for (int pointIndex=0; pointIndex<numPoints; pointIndex++) {
      if (abs(testValues(testOrdinal,pointIndex)-1.0) < tol) {
        phi_ordinals[pointIndex] = testOrdinal;
      }
    }
  }
  
  BasisPtr trialBasis = trialOrder->getBasis(trialID,0); // uniform: all sides should be the same
  
  numPoints = 2;
  FieldContainer<double> refPoints1D(numPoints,1); // line nodes (for linear basis)
  refPoints2D(0,0) = -1.0;
  refPoints2D(1,0) = 1.0;
  int v_ordinals[numPoints];
  
  FieldContainer<double> trialValues(trialBasis->getCardinality(),refPoints1D.dimension(0));
  trialBasis->getValues(trialValues, refPoints1D, Intrepid::OPERATOR_VALUE);
  for (int trialOrdinal=0; trialOrdinal<trialBasis->getCardinality(); trialOrdinal++) {
    for (int pointIndex=0; pointIndex<numPoints; pointIndex++) {
      if (abs(trialValues(trialOrdinal,pointIndex)-1.0) < tol) {
        v_ordinals[pointIndex] = trialOrdinal;
      }
    }
  }
  
  map<int,pair<int,int> > phiNodesForSide; // sideIndex --> pair( phiNodeIndex for v0, phiNodeIndex for v1)
  phiNodesForSide[0] = make_pair(0,1);
  phiNodesForSide[1] = make_pair(1,2);
  phiNodesForSide[2] = make_pair(2,3);
  phiNodesForSide[3] = make_pair(3,0);
  
  preStiff.initialize(0.0);
  int numSides = 4;
  int phiNodes[2];
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    phiNodes[0] = phiNodesForSide[sideIndex].first;
    phiNodes[1] = phiNodesForSide[sideIndex].second;
    for (int nodeIndex=0; nodeIndex<2; nodeIndex++) { // loop over the line's two nodes
      int testOrdinal = phi_ordinals[ phiNodes[nodeIndex] ]; // ordinal of the test that "agrees"
      int testDofIndex = testOrder->getDofIndex(testID,testOrdinal);
      int trialDofIndex = trialOrder->getDofIndex(trialID,nodeIndex,sideIndex);
      preStiff(0,testDofIndex,trialDofIndex) = 2.0/3.0 * h_ratio * sideParities(0,sideIndex); // "2/3" computed manually
      testOrdinal = phi_ordinals[ phiNodes[1-nodeIndex] ]; // ordinal of the test that "disagrees"
      testDofIndex = testOrder->getDofIndex(testID,testOrdinal);
      preStiff(0,testDofIndex,trialDofIndex) = 1.0/3.0 * h_ratio * sideParities(0,sideIndex); // "1/3" computed manually
    }
  }
}

void MeshRefinementTests::preStiffnessExpectedPatch(FieldContainer<double> &preStiff, double h, 
                                                    const map<int,int> &sidesWithBiggerNeighborToIndexInParentSide,
                                                    ElementTypePtr elemType,
                                                    FieldContainer<double> &sideParities) {
  double h_ratio = h / 2.0; // because the master line has length 2...
  
  DofOrderingPtr trialOrder = elemType->trialOrderPtr;
  DofOrderingPtr testOrder = elemType->testOrderPtr;
  preStiff.resize(1,testOrder->totalDofs(),trialOrder->totalDofs());
  int testID  =  testOrder->getVarIDs()[0]; // just one
  int trialID = trialOrder->getVarIDs()[0]; // just one (the flux)
  
  int numPoints = 4;
  FieldContainer<double> refPoints2D(numPoints,2); // quad nodes (for bilinear basis)
  refPoints2D(0,0) = -1.0;
  refPoints2D(0,1) = -1.0;
  refPoints2D(1,0) =  1.0;
  refPoints2D(1,1) = -1.0;
  refPoints2D(2,0) =  1.0;
  refPoints2D(2,1) =  1.0;
  refPoints2D(3,0) = -1.0;
  refPoints2D(3,1) =  1.0;
  
  int phi_ordinals[numPoints]; // phi_i = 1 at node x_i, and 0 at other nodes
  
  double tol = 1e-15;
  BasisPtr testBasis = testOrder->getBasis(testID);
  FieldContainer<double> testValues(testBasis->getCardinality(),refPoints2D.dimension(0));
  testBasis->getValues(testValues, refPoints2D, Intrepid::OPERATOR_VALUE);
  for (int testOrdinal=0; testOrdinal<testBasis->getCardinality(); testOrdinal++) {
    for (int pointIndex=0; pointIndex<numPoints; pointIndex++) {
      if (abs(testValues(testOrdinal,pointIndex)-1.0) < tol) {
        phi_ordinals[pointIndex] = testOrdinal;
      }
    }
  }
  
  BasisPtr trialBasis = trialOrder->getBasis(trialID,0); // uniform: all sides should be the same
  
  numPoints = 2;
  FieldContainer<double> refPoints1D(numPoints,1); // line nodes (for linear basis)
  refPoints2D(0,0) = -1.0;
  refPoints2D(1,0) = 1.0;
  int v_ordinals[numPoints];
  
  FieldContainer<double> trialValues(trialBasis->getCardinality(),refPoints1D.dimension(0));
  trialBasis->getValues(trialValues, refPoints1D, Intrepid::OPERATOR_VALUE);
  for (int trialOrdinal=0; trialOrdinal<trialBasis->getCardinality(); trialOrdinal++) {
    for (int pointIndex=0; pointIndex<numPoints; pointIndex++) {
      if (abs(trialValues(trialOrdinal,pointIndex)-1.0) < tol) {
        v_ordinals[pointIndex] = trialOrdinal;
      }
    }
  }
  
  map<int,pair<int,int> > phiNodesForSide; // sideIndex --> pair( phiNodeIndex for v0, phiNodeIndex for v1)
  phiNodesForSide[0] = make_pair(0,1);
  phiNodesForSide[1] = make_pair(1,2);
  phiNodesForSide[2] = make_pair(2,3);
  phiNodesForSide[3] = make_pair(3,0);
  
  preStiff.initialize(0.0);
  int numSides = 4;
  int phiNodes[2];
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    phiNodes[0] = phiNodesForSide[sideIndex].first;
    phiNodes[1] = phiNodesForSide[sideIndex].second;
    
    double agreeValue[2], disagreeValue[2];
    
    agreeValue[0]    = 2.0 / 3.0;  // for non-patch bases, if nodes for phi and v0 line up
    disagreeValue[0] = 1.0 / 3.0;  // for non-patch bases, if nodes for phi and v0 don't line up
    agreeValue[1]    = 2.0 / 3.0;  // for non-patch bases, if nodes for phi and v1 line up
    disagreeValue[1] = 1.0 / 3.0;  // for non-patch bases, if nodes for phi and v1 don't line up
    
    map<int,int>::const_iterator patchFoundIt = sidesWithBiggerNeighborToIndexInParentSide.find(sideIndex);
    bool hasPatchBasis = (patchFoundIt != sidesWithBiggerNeighborToIndexInParentSide.end());
    if ( hasPatchBasis ) {
      int indexInParentSide = patchFoundIt->second;
      if (indexInParentSide == 0) {
        agreeValue[0]    = 5.0 / 6.0;
        disagreeValue[0] = 2.0 / 3.0;
        agreeValue[1]    = 1.0 / 3.0;
        disagreeValue[1] = 1.0 / 6.0;
      } else if (indexInParentSide == 1) {
        agreeValue[0]    = 1.0 / 3.0;
        disagreeValue[0] = 1.0 / 6.0;
        agreeValue[1]    = 5.0 / 6.0;
        disagreeValue[1] = 2.0 / 3.0;
      } else {
        TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unsupported indexInParentSide.");
      }
    }
    
    for (int nodeIndex=0; nodeIndex<2; nodeIndex++) { // loop over the line's two nodes
      int testOrdinal = phi_ordinals[ phiNodes[nodeIndex] ]; // ordinal of the test that "agrees"
      int testDofIndex = testOrder->getDofIndex(testID,testOrdinal);
      int trialDofIndex = trialOrder->getDofIndex(trialID,nodeIndex,sideIndex);
      preStiff(0,testDofIndex,trialDofIndex) = agreeValue[nodeIndex] * h_ratio * sideParities(0,sideIndex);
      testOrdinal = phi_ordinals[ phiNodes[1-nodeIndex] ]; // ordinal of the test that "disagrees"
      testDofIndex = testOrder->getDofIndex(testID,testOrdinal);
      preStiff(0,testDofIndex,trialDofIndex) = disagreeValue[nodeIndex] * h_ratio * sideParities(0,sideIndex);
    }
  }
}

void MeshRefinementTests::preStiffnessExpectedMulti(FieldContainer<double> &preStiff, double h,
                                                    const set<int> &brokenSides, ElementTypePtr elemType,
                                                    FieldContainer<double> &sideParities) {
  
}

void MeshRefinementTests::setup() {
  // throughout setup, we have hard-coded cellIDs.
  // this will break if the way mesh or the refinements order their elements changes.
  // not hard, though tedious, to test for the points we know should be in the elements.
  // (see the diagram in the header file for the mesh layouts, which will make clear which points
  //  should be in which elements.)
  // (It should be the case that all such hard-coding is isolated to this method.)
  
  // MultiBasis meshes:
  FieldContainer<double> quadPoints(4,2);
  
  // setup so that the big h is 1.0, small h is 0.5:
  quadPoints(0,0) = 0.0; // x1
  quadPoints(0,1) = 0.0; // y1
  quadPoints(1,0) = 2.0;
  quadPoints(1,1) = 0.0;
  quadPoints(2,0) = 2.0;
  quadPoints(2,1) = 1.0;
  quadPoints(3,0) = 0.0;
  quadPoints(3,1) = 1.0;  
  
  int H1Order = 2;  // means linear for fluxes
  int delta_p = -1; // means tests will likewise be (bi-)linear
  int horizontalCells = 2; int verticalCells = 1;
  
  _h = 1.0;
  _h_small = 0.5;
  
  _fluxBilinearForm = Teuchos::rcp( new TestBilinearFormFlux() );
  
  _multiA = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _multiA->setUsePatchBasis(false);
  vector<int> cellsToRefine;
  cellsToRefine.push_back(0); // hard-coding cellID: would be better to test for the point, but I'm being lazy
  _multiA->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  
  // get elements
  _A1multi = _multiA->getElement(1);
  _A3multi = _multiA->getElement(3);
  _A4multi = _multiA->getElement(4);
  _A5multi = _multiA->getElement(5);
  
  _multiB = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _multiB->setUsePatchBasis(false);
  
  _B1multi = _multiB->getElement(1);
  
  cellsToRefine.clear();
  
  _multiC = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _multiC->setUsePatchBasis(false);
  
  cellsToRefine.push_back(0); // hard-coding cellID: would be better to test for the point, but I'm being lazy
  _multiC->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  
  cellsToRefine.clear();
  cellsToRefine.push_back(1); // hard-coding cellID: would be better to test for the point, but I'm being lazy
  _multiC->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  
  cellsToRefine.clear();

  _C4multi = _multiC->getElement(4);
  _C5multi = _multiC->getElement(5);
  
  // PatchBasis meshes:
  _patchA = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _patchA->setUsePatchBasis(true);
  cellsToRefine.clear();
  cellsToRefine.push_back(0); // hard-coding cellID
  _patchA->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  
  // get elements
  _A1patch = _patchA->getElement(1);
  _A3patch = _patchA->getElement(3);
  _A4patch = _patchA->getElement(4);
  _A5patch = _patchA->getElement(5);
  
  _patchB = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _patchB->setUsePatchBasis(true);
  
  _B1patch = _patchB->getElement(1);
  
  cellsToRefine.clear();
  
  _patchC = Mesh::buildQuadMesh(quadPoints, horizontalCells, verticalCells, _fluxBilinearForm, H1Order, H1Order+delta_p);
  _patchC->setUsePatchBasis(true);
  cellsToRefine.push_back(0); // hard-coding cellID
  _patchC->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  
  cellsToRefine.clear();
  cellsToRefine.push_back(1); // hard-coding cellID
  _patchC->hRefine(cellsToRefine,RefinementPattern::regularRefinementPatternQuad());
  cellsToRefine.clear();
  
  _C4patch = _patchC->getElement(4);
  _C5patch = _patchC->getElement(5);  
}

void MeshRefinementTests::teardown() {
  
}

void MeshRefinementTests::runTests(int &numTestsRun, int &numTestsPassed) {
  setup();
  if (testMultiBasisSideParities()) {
    numTestsPassed++;
  }
  numTestsRun++;
  teardown();
  
  setup();
  if (testPatchBasisSideParities()) {
    numTestsPassed++;
  }
  numTestsRun++;
  teardown();
  
  setup();
  if (testUniformMeshStiffnessMatrices()) {
    numTestsPassed++;
  }
  numTestsRun++;
  teardown();
  
  setup();
  if (testMultiBasisStiffnessMatrices()) {
    numTestsPassed++;
  }
  numTestsRun++;
  teardown();
  
  setup();
  if (testPatchBasisStiffnessMatrices()) {
    numTestsPassed++;
  }
  numTestsRun++;
  teardown();
}

bool MeshRefinementTests::testUniformMeshStiffnessMatrices() {
  bool success = true;
  
  double tol = 1e-14;
  double maxDiff;
  
  FieldContainer<double> expectedValues;
  FieldContainer<double> physicalCellNodes;
  FieldContainer<double> actualValues;
  FieldContainer<double> sideParities;
  
  // test that _B1{multi|patch} and _C4{multi|patch} have the expected values
  
  // B1: (large element)
  // multi:
  // determine expected values:
  ElementTypePtr elemType = _B1multi->elementType();
  sideParities = _multiB->cellSideParitiesForCell(_B1multi->cellID());
  preStiffnessExpectedUniform(expectedValues,_h,elemType,sideParities);
  // get actual values:
  physicalCellNodes = _multiB->physicalCellNodesForCell(_B1multi->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _multiB, _B1multi->cellID());

  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in uniform mesh B (with usePatchBasis=false) stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  // patch:
  // determine expected values:
  elemType = _B1patch->elementType();
  sideParities = _patchB->cellSideParitiesForCell(_B1patch->cellID());
  preStiffnessExpectedUniform(expectedValues,_h,elemType,sideParities);
  // get actual values:
  physicalCellNodes = _patchB->physicalCellNodesForCell(_B1patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchB, _B1patch->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in uniform mesh B (with usePatchBasis=true) stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  // C4: (small element)
  // multi:
  // determine expected values:
  elemType = _C4multi->elementType();
  sideParities = _multiC->cellSideParitiesForCell(_C4multi->cellID());
  preStiffnessExpectedUniform(expectedValues,_h_small,elemType,sideParities);
  // get actual values:
  physicalCellNodes = _multiC->physicalCellNodesForCell(_C4multi->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _multiC, _C4multi->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in uniform mesh C (with usePatchBasis=false) stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  // patch:
  // determine expected values:
  elemType = _C4patch->elementType();
  sideParities = _patchC->cellSideParitiesForCell(_C4patch->cellID());
  preStiffnessExpectedUniform(expectedValues,_h_small,elemType,sideParities);
  // get actual values:
  physicalCellNodes = _patchC->physicalCellNodesForCell(_C4patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchC, _C4patch->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in uniform mesh C (with usePatchBasis=true) stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  return success;
}

bool MeshRefinementTests::testMultiBasisStiffnessMatrices() {
  bool success = false;
  
  cout << "testMultiBasisStiffnessMatrices unimplemented.\n";
  
  return success;
}

bool MeshRefinementTests::testPatchBasisStiffnessMatrices() {
  bool success = false;
  
  double tol = 1e-14;
  double maxDiff;
  
  FieldContainer<double> expectedValues;
  FieldContainer<double> physicalCellNodes;
  FieldContainer<double> actualValues;
  FieldContainer<double> sideParities;
  
  map<int,int> parentSideMap;
  
  // test that _A{3,4}patch and have the expected values
  
  // A3: (small element)
  // determine expected values:
  patchParentSideIndices(parentSideMap,_patchA,_A3patch);
  ElementTypePtr elemType = _A3patch->elementType();
  sideParities = _patchA->cellSideParitiesForCell(_A3patch->cellID());
  preStiffnessExpectedPatch(expectedValues,_h_small,parentSideMap,elemType,sideParities);

  // get actual values:
  physicalCellNodes = _patchA->physicalCellNodesForCell(_A3patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchA, _A3patch->cellID());

  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in element _A3patch stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
    
  // A4: (small element)
  // determine expected values:
  patchParentSideIndices(parentSideMap,_patchA,_A4patch);
  elemType = _A4patch->elementType();
  sideParities = _patchA->cellSideParitiesForCell(_A4patch->cellID());
  preStiffnessExpectedPatch(expectedValues,_h_small,parentSideMap,elemType,sideParities);
  
  // get actual values:
  physicalCellNodes = _patchA->physicalCellNodesForCell(_A4patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchA, _A4patch->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in element _A4patch stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  // *******  following are elements that should have just "normal" values (no PatchBasis interaction) *******
  
  // A1: (large element)
  // determine expected values:
  patchParentSideIndices(parentSideMap,_patchA,_A1patch);
  elemType = _A1patch->elementType();
  sideParities = _patchA->cellSideParitiesForCell(_A1patch->cellID());
  preStiffnessExpectedPatch(expectedValues,_h,parentSideMap,elemType,sideParities);
  
  // get actual values:
  physicalCellNodes = _patchA->physicalCellNodesForCell(_A1patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchA, _A1patch->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in element _A1patch stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  // A5: (small element)
  // determine expected values:
  patchParentSideIndices(parentSideMap,_patchA,_A5patch);
  elemType = _A5patch->elementType();
  sideParities = _patchA->cellSideParitiesForCell(_A5patch->cellID());
  preStiffnessExpectedPatch(expectedValues,_h_small,parentSideMap,elemType,sideParities);
  
  // get actual values:
  physicalCellNodes = _patchA->physicalCellNodesForCell(_A5patch->cellID());
  BilinearFormUtility::computeStiffnessMatrixForCell(actualValues, _patchA, _A5patch->cellID());
  
  if ( !fcsAgree(expectedValues,actualValues,tol,maxDiff) ) {
    cout << "Failure in element _A5patch stiffness computation; maxDiff = " << maxDiff << endl;
    cout << "expectedValues:\n" << expectedValues;
    cout << "actualValues:\n" << actualValues;
    success = false;
  }
  
  return success;
}

bool MeshRefinementTests::testMultiBasisSideParities() {
  bool success = true;
  
  FieldContainer<double> A0_side_parities = _multiA->cellSideParitiesForCell(_A3multi->getParent()->cellID());
  FieldContainer<double> A1_side_parities = _multiA->cellSideParitiesForCell(_A1multi->cellID());
  FieldContainer<double> A3_side_parities = _multiA->cellSideParitiesForCell(_A3multi->cellID());
  FieldContainer<double> A4_side_parities = _multiA->cellSideParitiesForCell(_A4multi->cellID());
  
  // check the entry for sideIndex 1 (the patchBasis side)
  if (A0_side_parities(0,1) != A3_side_parities(0,1)) {
    success = false;
    cout << "Failure: MultiBasisSideParities: child doesn't match parent along broken side.\n";
  }
  if (A3_side_parities(0,1) != A4_side_parities(0,1)) {
    success = false;
    cout << "Failure: MultiBasisSideParities: children don't match each other along parent side.\n";
  }
  if (A3_side_parities(0,1) != - A1_side_parities(0,3)) {
    success = false;
    cout << "Failure: MultiBasisSideParities: children aren't opposite large neighbor cell parity.\n";
  }
  return success;
}

bool MeshRefinementTests::testPatchBasisSideParities() {
  bool success = true;
  
  FieldContainer<double> A0_side_parities = _patchA->cellSideParitiesForCell(_A3patch->getParent()->cellID());
  FieldContainer<double> A1_side_parities = _patchA->cellSideParitiesForCell(_A1patch->cellID());
  FieldContainer<double> A3_side_parities = _patchA->cellSideParitiesForCell(_A3patch->cellID());
  FieldContainer<double> A4_side_parities = _patchA->cellSideParitiesForCell(_A4patch->cellID());
  
  // check the entry for sideIndex 1 (the patchBasis side)
  if (A0_side_parities(0,1) != A3_side_parities(0,1)) {
    success = false;
    cout << "Failure: PatchBasisSideParities: child doesn't match parent along broken side.\n";
  }
  if (A3_side_parities(0,1) != A4_side_parities(0,1)) {
    success = false;
    cout << "Failure: PatchBasisSideParities: children don't match each other along parent side.\n";
  }
  if (A3_side_parities(0,1) != - A1_side_parities(0,3)) {
    success = false;
    cout << "Failure: PatchBasisSideParities: children aren't opposite large neighbor cell parity.\n";
  }

  return success;
}