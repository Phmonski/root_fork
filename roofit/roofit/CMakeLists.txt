# Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

############################################################################
# CMakeLists.txt file for building ROOT rootfit package
# @author Pere Mato, CERN
############################################################################

set (EXTRA_DICT_OPTS)
if (runtime_cxxmodules AND WIN32)
  set (EXTRA_DICT_OPTS NO_CXXMODULE)
endif()

ROOT_STANDARD_LIBRARY_PACKAGE(RooFit
  HEADERS
    Roo2DKeysPdf.h
    RooArgusBG.h
    RooBCPEffDecay.h
    RooBCPGenDecay.h
    RooBDecay.h
    RooBernstein.h
    RooBifurGauss.h
    RooBlindTools.h
    RooBMixDecay.h
    RooBreitWigner.h
    RooBukinPdf.h
    RooCBShape.h
    RooCrystalBall.h
    RooCFunction1Binding.h
    RooCFunction2Binding.h
    RooCFunction3Binding.h
    RooCFunction4Binding.h
    RooChebychev.h
    RooChi2MCSModule.h
    RooChiSquarePdf.h
    RooDecay.h
    RooDstD0BG.h
    RooExponential.h
    RooLegacyExpPoly.h
    RooPowerSum.h
    RooFunctor1DBinding.h
    RooFunctorBinding.h
    RooGamma.h
    RooGaussExpTails.h
    RooGaussian.h
    RooGaussModel.h
    RooGExpModel.h
    RooHistConstraint.h
    RooIntegralMorph.h
    RooJeffreysPrior.h
    RooKeysPdf.h
    RooLagrangianMorphFunc.h
    RooLandau.h
    RooLognormal.h
    RooMathCoreReg.h
    RooMomentMorphFunc.h
    RooMomentMorphFuncND.h
    RooMomentMorph.h
    RooMultiBinomial.h
    RooNDKeysPdf.h
    RooNonCPEigenDecay.h
    RooNovosibirsk.h
    RooParametricStepFunction.h
    RooParamHistFunc.h
    RooPoisson.h
    RooPolynomial.h
    RooPyBind.h
    RooSpline.h
    RooStepFunction.h
    RooTFnBinding.h
    RooTFnPdfBinding.h
    RooTMathReg.h
    RooUnblindCPAsymVar.h
    RooUnblindOffset.h
    RooUnblindPrecision.h
    RooUnblindUniform.h
    RooUniform.h
    RooVoigtian.h
    RooJohnson.h
  SOURCES
    src/Roo2DKeysPdf.cxx
    src/RooArgusBG.cxx
    src/RooBCPEffDecay.cxx
    src/RooBCPGenDecay.cxx
    src/RooBDecay.cxx
    src/RooBernstein.cxx
    src/RooBifurGauss.cxx
    src/RooBlindTools.cxx
    src/RooBMixDecay.cxx
    src/RooBreitWigner.cxx
    src/RooBukinPdf.cxx
    src/RooCBShape.cxx
    src/RooCrystalBall.cxx
    src/RooCFunction1Binding.cxx
    src/RooCFunction2Binding.cxx
    src/RooCFunction3Binding.cxx
    src/RooCFunction4Binding.cxx
    src/RooChebychev.cxx
    src/RooChi2MCSModule.cxx
    src/RooChiSquarePdf.cxx
    src/RooDecay.cxx
    src/RooDstD0BG.cxx
    src/RooExponential.cxx
    src/RooLegacyExpPoly.cxx
    src/RooPowerSum.cxx
    src/RooFunctor1DBinding.cxx
    src/RooFunctorBinding.cxx
    src/RooGamma.cxx
    src/RooGaussExpTails.cxx
    src/RooGaussian.cxx
    src/RooGaussModel.cxx
    src/RooGExpModel.cxx
    src/RooHistConstraint.cxx
    src/RooIntegralMorph.cxx
    src/RooJeffreysPrior.cxx
    src/RooKeysPdf.cxx
    src/RooLagrangianMorphFunc.cxx
    src/RooLandau.cxx
    src/RooLognormal.cxx
    src/RooMathCoreReg.cxx
    src/RooMomentMorph.cxx
    src/RooMomentMorphFunc.cxx
    src/RooMomentMorphFuncND.cxx
    src/RooMultiBinomial.cxx
    src/RooNDKeysPdf.cxx
    src/RooNonCPEigenDecay.cxx
    src/RooNovosibirsk.cxx
    src/RooParametricStepFunction.cxx
    src/RooParamHistFunc.cxx
    src/RooPoisson.cxx
    src/RooPolynomial.cxx
    src/RooSpline.cxx
    src/RooStepFunction.cxx
    src/RooTFnBinding.cxx
    src/RooTFnPdfBinding.cxx
    src/RooTMathReg.cxx
    src/RooUnblindCPAsymVar.cxx
    src/RooUnblindOffset.cxx
    src/RooUnblindPrecision.cxx
    src/RooUnblindUniform.cxx
    src/RooUniform.cxx
    src/RooVoigtian.cxx
    src/RooJohnson.cxx
  DICTIONARY_OPTIONS
    "-writeEmptyRootPCM"
  LINKDEF
    LinkDef1.h
  LIBRARIES
    RooBatchCompute
  DEPENDENCIES
    Core
    RooFitCore
    Tree
    RIO
    Matrix
    MathCore
  ${EXTRA_DICT_OPTS}
)

# For recent clang, this can facilitate auto-vectorisation.
# In RooFit, the errno side effect is not needed, anyway:
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  target_compile_options(RooFit PUBLIC -fno-math-errno)
endif()

ROOT_ADD_TEST_SUBDIRECTORY(test)
