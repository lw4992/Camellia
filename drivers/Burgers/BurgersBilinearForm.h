#ifndef DPG_BURGERS_BILINEAR_FORM
#define DPG_BURGERS_BILINEAR_FORM

#include "BilinearForm.h"

class Solution;

class BurgersBilinearForm : public BilinearForm {
private:
  double _epsilon;
  Teuchos::RCP<Solution> _backgroundFlow;
public:
  //  BurgersBilinearForm(double epsilon, Teuchos::RCP<Solution> backgroundFlow);
  BurgersBilinearForm(double epsilon);
  
  // implement the virtual methods declared in super:
  const string & testName(int testID);
  const string & trialName(int trialID);
  
  bool trialTestOperator(int trialID, int testID, 
                         EOperatorExtended &trialOperator, EOperatorExtended &testOperator);
  
//  void applyBilinearFormData(int trialID, int testID,
//                             FieldContainer<double> &trialValues, FieldContainer<double> &testValues, 
//                             int operatorIndex, 
//                             Teuchos::RCP<BasisCache> basisCache);
  
  void applyBilinearFormData(FieldContainer<double> &trialValues, FieldContainer<double> &testValues, 
                             int trialID, int testID, int operatorIndex, // operatorIndex unused
                             Teuchos::RCP<BasisCache> basisCache);
  
  virtual EFunctionSpaceExtended functionSpaceForTest(int testID);
  virtual EFunctionSpaceExtended functionSpaceForTrial(int trialID);

  double getEpsilon();
  void setEpsilon(double newEpsilon);
  FieldContainer<double> getBeta(Teuchos::RCP<BasisCache> basisCache);
  FieldContainer<double> getBeta(const FieldContainer<double> &physicalPoints); // BC depends on this version
  vector<double> getBeta(double x, double y);
  void setBackgroundFlow(Teuchos::RCP<Solution> flow);
  Teuchos::RCP<Solution> getBackgroundFlow();
  
  bool isFluxOrTrace(int trialID);
  
  enum ETestIDs {
    TAU = 0,
    V
  };
  
  enum ETrialIDs {
    U_HAT = 0,
    BETA_N_U_MINUS_SIGMA_HAT,
    U,
    SIGMA_1,
    SIGMA_2
  };
};

#endif