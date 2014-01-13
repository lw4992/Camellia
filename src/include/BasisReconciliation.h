//
//  BasisReconciliation.h
//  Camellia-debug
//
//  Created by Nate Roberts on 11/19/13.
//
//

#ifndef Camellia_debug_BasisReconciliation_h
#define Camellia_debug_BasisReconciliation_h

#include "Intrepid_FieldContainer.hpp"

#include "RefinementPattern.h"

#include "Basis.h"

using namespace Intrepid;
using namespace std;

struct SubBasisReconciliationWeights {
  FieldContainer<double> weights; // indices are (fine, coarse)
  set<int> fineOrdinals;
  set<int> coarseOrdinals;
};

class BasisReconciliation {
  bool _cacheResults;
  
  typedef pair< Camellia::Basis<>*, int > SideBasisRestriction;
  // cached values:
  map< pair< Camellia::Basis<>*, Camellia::Basis<>*>, FieldContainer<double> > _simpleReconciliationWeights; // simple: no sides involved
  map< pair< pair< SideBasisRestriction, SideBasisRestriction >, unsigned >, SubBasisReconciliationWeights > _sideReconciliationWeights;
public:
  BasisReconciliation(bool cacheResults = true) { _cacheResults = cacheResults; }

  const FieldContainer<double> &constrainedWeights(BasisPtr finerBasis, BasisPtr coarserBasis); // requires these to be defined on the same topology
  const SubBasisReconciliationWeights &constrainedWeights(BasisPtr finerBasis, int finerBasisSideIndex, BasisPtr coarserBasis, int coarserBasisSideIndex, unsigned vertexNodePermutation); // requires the sides to have the same topology
  
  // static workhorse methods:
  
  /* Unbroken elements: */
  static FieldContainer<double> computeConstrainedWeights(BasisPtr finerBasis, BasisPtr coarserBasis); // requires these to be defined on the same topology
  static SubBasisReconciliationWeights computeConstrainedWeights(BasisPtr finerBasis, BasisPtr coarserBasis, int finerBasisSideIndex, int coarserBasisSideIndex, unsigned vertexNodePermutation); // requires the sides to have the same topology
  // vertexNodePermutation: how to permute side as seen by finerBasis to produce side seen by coarserBasis.  Specifically, if iota_1 maps finerBasis vertices \hat{v}_i^1 to v_i in physical space, and iota_2 does the same for vertices of coarserBasis's topology, then the permutation is the one corresponding to iota_2^(-1) \ocirc iota_1.
  // vertexNodePermutation is an index into a structure defined by CellTopologyTraits.  See CellTopology::getNodePermutation() and CellTopology::getNodePermutationInverse().
  
  /* Broken elements: */
  // matching the whole bases:
  static FieldContainer<double> computeConstrainedWeights(BasisPtr finerBasis, BasisPtr coarserBasis, const FieldContainer<double> &finerBasisElementNodesInCoarserBasisReferenceCell);
  // matching along sides:
  static FieldContainer<double> computeConstrainedWeights(BasisPtr finerBasis, BasisPtr coarserBasis, int finerBasisSideIndex, int coarserBasisSideIndex,
                                                          const FieldContainer<double> &finerBasisSideNodesInCoarserBasisReferenceCell);
  // it's worth noting that these FieldContainer arguments are not especially susceptible to caching
  // for the caching interface, may want to use vector< pair< RefinementPattern*, int childIndex > > to define the coarse element's neighbor refinement hierarchy.  Probably still need a vertexPermutation
  // as well to fully define the relationship.  Something like:
private:
  typedef vector< pair< RefinementPattern*, int > > RefinementHierarchy;
  typedef pair< pair< Camellia::Basis<>*, Camellia::Basis<>* >, RefinementHierarchy > RefinedBasisPair;
  typedef pair< pair< SideBasisRestriction, SideBasisRestriction >, RefinementHierarchy > SideRefinedBasisPair;
  map< RefinedBasisPair, FieldContainer<double> > _simpleReconcilationWeights_h;
  map< pair< SideRefinedBasisPair, unsigned> , FieldContainer<double> > _sideReconcilationWeights_h;  // unsigned: the vertexPermutation
};

/* a few ideas come up here:
 
 1. Where should information about what continuities are enforced (e.g. vertices, edges, faces) reside?  Does it belong to the basis?  At a certain level that makes a good deal of sense, although it would break our current approach of using one lower order for L^2 functions, but otherwise using the H^1 basis.  Basis *does* offer the IntrepidExtendedTypes::EFunctionSpaceExtended functionSpace() method, which could be used for this purpose...  In fact, I believe we do get this right for our L^2 functions.
 2. Where should information about what pullbacks to use reside?  It seems to me this should also belong to the basis.  (And if we wanted to use functionSpace() to make this decision, this is what our L^2 basis implementation would break.)
 3. In particular, the answer to #1 affects our interface in BasisReconciliation.  The question is whether the side variants of computeConstrainedWeights should get subcell bases for vertices, edges, etc. or just the d-1 dimensional side.
 
 */


#endif