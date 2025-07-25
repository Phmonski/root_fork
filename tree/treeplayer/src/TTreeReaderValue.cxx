// @(#)root/treeplayer:$Id$
// Author: Axel Naumann, 2011-09-28

/*************************************************************************
 * Copyright (C) 1995-2013, Rene Brun and Fons Rademakers and al.        *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TTreeReaderValue.h"

#include "TTreeReader.h"
#include "TBranchClones.h"
#include "TBranchElement.h"
#include "TBranchRef.h"
#include "TBranchSTL.h"
#include "TBranchObject.h"
#include "TBranchProxyDirector.h"
#include "TClassEdit.h"
#include "TEnum.h"
#include "TFriendElement.h"
#include "TFriendProxy.h"
#include "TLeaf.h"
#include "TTreeProxyGenerator.h"
#include "TRegexp.h"
#include "TStreamerInfo.h"
#include "TStreamerElement.h"
#include "TNtuple.h"
#include "TROOT.h"

#include <optional>
#include <vector>

// clang-format off
/**
 * \class TTreeReaderValue
 * \ingroup treeplayer
 * \brief An interface for reading values stored in ROOT columnar datasets
 *
 * The TTreeReaderValue is a type-safe tool to be used in association with a TTreeReader
 * to access the values stored in TTree, TNtuple and TChain datasets.
 * TTreeReaderValue can be also used to access collections such as `std::vector`s or TClonesArray
 * stored in columnar datasets but it is recommended to use TTreeReaderArray instead as it offers
 * several advantages.
 *
 * See the documentation of TTreeReader for more details and examples.
*/
// clang-format on

ClassImp(ROOT::Internal::TTreeReaderValueBase);

////////////////////////////////////////////////////////////////////////////////
/// Construct a tree value reader and register it with the reader object.

ROOT::Internal::TTreeReaderValueBase::TTreeReaderValueBase(TTreeReader *reader /*= 0*/, const char *branchname /*= 0*/,
                                                           TDictionary *dict /*= 0*/, bool opaqueRead)
   : fHaveLeaf(false),
     fHaveStaticClassOffsets(false),
     fReadStatus(kReadNothingYet),
     fBranchName(branchname),
     fTreeReader(reader),
     fDict(dict),
     fOpaqueRead(opaqueRead)
{
   RegisterWithTreeReader();
}

////////////////////////////////////////////////////////////////////////////////
/// Copy-construct.

ROOT::Internal::TTreeReaderValueBase::TTreeReaderValueBase(const TTreeReaderValueBase &rhs)
   : fHaveLeaf(rhs.fHaveLeaf),
     fHaveStaticClassOffsets(rhs.fHaveStaticClassOffsets),
     fReadStatus(rhs.fReadStatus),
     fSetupStatus(rhs.fSetupStatus),
     fBranchName(rhs.fBranchName),
     fLeafName(rhs.fLeafName),
     fTreeReader(rhs.fTreeReader),
     fDict(rhs.fDict),
     fProxy(rhs.fProxy),
     fLeaf(rhs.fLeaf),
     fStaticClassOffsets(rhs.fStaticClassOffsets)
{
   RegisterWithTreeReader();
}

////////////////////////////////////////////////////////////////////////////////
/// Copy-assign.

ROOT::Internal::TTreeReaderValueBase &ROOT::Internal::TTreeReaderValueBase::operator=(const TTreeReaderValueBase &rhs)
{
   if (&rhs != this) {
      fHaveLeaf = rhs.fHaveLeaf;
      fHaveStaticClassOffsets = rhs.fHaveStaticClassOffsets;
      fBranchName = rhs.fBranchName;
      fLeafName = rhs.fLeafName;
      if (fTreeReader != rhs.fTreeReader) {
         if (fTreeReader)
            fTreeReader->DeregisterValueReader(this);
         fTreeReader = rhs.fTreeReader;
         RegisterWithTreeReader();
      }
      fDict = rhs.fDict;
      fProxy = rhs.fProxy;
      fLeaf = rhs.fLeaf;
      fSetupStatus = rhs.fSetupStatus;
      fReadStatus = rhs.fReadStatus;
      fStaticClassOffsets = rhs.fStaticClassOffsets;
   }
   return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// Unregister from tree reader, cleanup.

ROOT::Internal::TTreeReaderValueBase::~TTreeReaderValueBase()
{
   if (fTreeReader)
      fTreeReader->DeregisterValueReader(this);
   R__ASSERT((fLeafName.Length() == 0) == !fHaveLeaf && "leafness disagreement");
   R__ASSERT(fStaticClassOffsets.empty() == !fHaveStaticClassOffsets && "static class offset disagreement");
}

////////////////////////////////////////////////////////////////////////////////
/// Register with tree reader.

void ROOT::Internal::TTreeReaderValueBase::RegisterWithTreeReader()
{
   if (fTreeReader) {
      if (!fTreeReader->RegisterValueReader(this)) {
         fTreeReader = nullptr;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Try to read the value from the TBranchProxy, returns
/// the status of the read.

template <ROOT::Internal::TTreeReaderValueBase::BranchProxyRead_t Func>
ROOT::Internal::TTreeReaderValueBase::EReadStatus ROOT::Internal::TTreeReaderValueBase::ProxyReadTemplate()
{
   if ((fProxy->*Func)()) {
      fReadStatus = kReadSuccess;
   } else {
      fReadStatus = kReadError;
   }
   return fReadStatus;
}

ROOT::Internal::TTreeReaderValueBase::EReadStatus ROOT::Internal::TTreeReaderValueBase::ProxyReadDefaultImpl()
{
   if (!fProxy)
      return kReadNothingYet;
   if (fProxy->IsInitialized() || fProxy->Setup()) {

      using EReadType = ROOT::Detail::TBranchProxy::EReadType;
      using TBranchPoxy = ROOT::Detail::TBranchProxy;

      EReadType readtype = EReadType::kNoDirector;
      if (fProxy)
         readtype = fProxy->GetReadType();

      switch (readtype) {
      case EReadType::kNoDirector:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoDirector>;
         break;
      case EReadType::kReadParentNoCollection:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadParentNoCollection>;
         break;
      case EReadType::kReadParentCollectionNoPointer:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadParentCollectionNoPointer>;
         break;
      case EReadType::kReadParentCollectionPointer:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadParentCollectionPointer>;
         break;
      case EReadType::kReadNoParentNoBranchCountCollectionPointer:
         fProxyReadFunc =
            &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentNoBranchCountCollectionPointer>;
         break;
      case EReadType::kReadNoParentNoBranchCountCollectionNoPointer:
         fProxyReadFunc =
            &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentNoBranchCountCollectionNoPointer>;
         break;
      case EReadType::kReadNoParentNoBranchCountNoCollection:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentNoBranchCountNoCollection>;
         break;
      case EReadType::kReadNoParentBranchCountCollectionPointer:
         fProxyReadFunc =
            &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentBranchCountCollectionPointer>;
         break;
      case EReadType::kReadNoParentBranchCountCollectionNoPointer:
         fProxyReadFunc =
            &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentBranchCountCollectionNoPointer>;
         break;
      case EReadType::kReadNoParentBranchCountNoCollection:
         fProxyReadFunc = &TTreeReaderValueBase::ProxyReadTemplate<&TBranchPoxy::ReadNoParentBranchCountNoCollection>;
         break;
      case EReadType::kDefault:
         // intentional fall through.
      default: fProxyReadFunc = &TTreeReaderValueBase::ProxyReadDefaultImpl;
      }
      return (this->*fProxyReadFunc)();
   }

   // If somehow the Setup fails call the original Read to
   // have the proper error handling (message only if the Setup fails
   // and the current proxy entry is different than the TTree's current entry)
   if (fProxy->Read()) {
      fReadStatus = kReadSuccess;
   } else {
      fReadStatus = kReadError;
   }
   return fReadStatus;
}

////////////////////////////////////////////////////////////////////////////////
/// Stringify the template argument.
std::string ROOT::Internal::TTreeReaderValueBase::GetElementTypeName(const std::type_info &ti)
{
   int err;
   char *buf = TClassEdit::DemangleTypeIdName(ti, err);
   std::string ret = buf;
   free(buf);
   return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// The TTreeReader has switched to a new TTree. Update the leaf.

void ROOT::Internal::TTreeReaderValueBase::NotifyNewTree(TTree *newTree)
{
   // Since the TTree structure might have change, let's make sure we
   // use the right reading function.
   fProxyReadFunc = &TTreeReaderValueBase::ProxyReadDefaultImpl;

   if (!fHaveLeaf || !newTree) {
      fLeaf = nullptr;
      return;
   }

   TBranch *myBranch = newTree->GetBranch(fBranchName);

   if (!myBranch) {
      fReadStatus = kReadError;
      Error("TTreeReaderValueBase::GetLeaf()", "Unable to get the branch from the tree");
      return;
   }

   fLeaf = myBranch->GetLeaf(fLeafName);
   if (!fLeaf) {
      Error("TTreeReaderValueBase::GetLeaf()", "Failed to get the leaf from the branch");
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Returns the memory address of the object being read.

void *ROOT::Internal::TTreeReaderValueBase::GetAddress()
{

   // If an indexed friend did not match the current entry and if this reader
   // is associated with that friend (i.e. its active read entry is -1), avoid
   // reading altogether
   if (fTreeReader->GetEntryStatus() == TTreeReader::kIndexedFriendNoMatch && fProxy &&
       fProxy->fDirector->GetReadEntry() == -1)
      return nullptr;

   if (ProxyRead() != kReadSuccess)
      return nullptr;

   if (fHaveLeaf) {
      if (GetLeaf()) {
         return fLeaf->GetValuePointer();
      } else {
         fReadStatus = kReadError;
         Error("TTreeReaderValueBase::GetAddress()", "Unable to get the leaf");
         return nullptr;
      }
   }
   if (fHaveStaticClassOffsets) { // Follow all the pointers
      Byte_t *address = (Byte_t *)fProxy->GetWhere();

      for (unsigned int i = 0; i < fStaticClassOffsets.size() - 1; ++i) {
         address = *(Byte_t **)(address + fStaticClassOffsets[i]);
      }

      return address + fStaticClassOffsets.back();
   }
   return (Byte_t *)fProxy->GetWhere();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Search a branch the name of which contains a "."
/// \param[out] myLeaf The leaf identified by the name if found (can be untouched).
/// \param[out] branchActualType Dictionary associated to the type of the leaf (can be untouched).
/// \param[out] errMsg The error message (can be untouched).
/// \return The address of the branch if found, nullptr otherwise
/// This method allows to efficiently search for branches which have names which
/// contain "dots", for example "w.v.a" or "v.a".
/// Therefore, it allows to support names such as v.a where the branch was
/// created with this syntax:
/// ```{.cpp}
/// myTree->Branch("v", &v, "a/I:b:/I")
/// ```
/// The method has some side effects, namely it can modify fSetupStatus, fProxy
/// and fStaticClassOffsets/fHaveStaticClassOffsets.
TBranch *ROOT::Internal::TTreeReaderValueBase::SearchBranchWithCompositeName(TLeaf *&myLeaf,
                                                                             TDictionary *&branchActualType,
                                                                             std::string &errMsg)
{
   TRegexp leafNameExpression("\\.[a-zA-Z0-9_]+$");
   TString leafName(fBranchName(leafNameExpression));
   TString branchName = fBranchName(0, fBranchName.Length() - leafName.Length());
   auto branch = fTreeReader->GetTree()->GetBranch(branchName);
   if (!branch) {
      std::vector<TString> nameStack;
      nameStack.push_back(TString()); // Trust me
      nameStack.push_back(leafName.Strip(TString::kBoth, '.'));
      leafName = branchName(leafNameExpression);
      branchName = branchName(0, branchName.Length() - leafName.Length());

      branch = fTreeReader->GetTree()->GetBranch(branchName);
      if (!branch)
         branch = fTreeReader->GetTree()->GetBranch(branchName + ".");
      if (leafName.Length())
         nameStack.push_back(leafName.Strip(TString::kBoth, '.'));

      while (!branch && branchName.Contains(".")) {
         leafName = branchName(leafNameExpression);
         branchName = branchName(0, branchName.Length() - leafName.Length());
         branch = fTreeReader->GetTree()->GetBranch(branchName);
         if (!branch)
            branch = fTreeReader->GetTree()->GetBranch(branchName + ".");
         nameStack.push_back(leafName.Strip(TString::kBoth, '.'));
      }

      if (branch && branch->IsA() == TBranchElement::Class()) {
         TBranchElement *myBranchElement = (TBranchElement *)branch;

         TString traversingBranch = nameStack.back();
         nameStack.pop_back();

         bool found = true;

         TDataType *finalDataType = nullptr;

         std::vector<Long64_t> offsets;
         Long64_t offset = 0;
         TClass *elementClass = nullptr;

         TObjArray *myObjArray = myBranchElement->GetInfo()->GetElements();
         TVirtualStreamerInfo *myInfo = myBranchElement->GetInfo();

         while (!nameStack.empty() && found) {
            found = false;

            for (int i = 0; i < myObjArray->GetEntries(); ++i) {

               TStreamerElement *tempStreamerElement = (TStreamerElement *)myObjArray->At(i);

               if (!strcmp(tempStreamerElement->GetName(), traversingBranch.Data())) {
                  offset += myInfo->GetElementOffset(i);

                  traversingBranch = nameStack.back();
                  nameStack.pop_back();

                  elementClass = tempStreamerElement->GetClass();
                  if (elementClass) {
                     myInfo = elementClass->GetStreamerInfo(0);
                     myObjArray = myInfo->GetElements();
                     // FIXME: this is odd, why is 'i' not also reset????
                  } else {
                     finalDataType = TDataType::GetDataType((EDataType)tempStreamerElement->GetType());
                     if (!finalDataType) {
                        TDictionary *seType = TDictionary::GetDictionary(tempStreamerElement->GetTypeName());
                        if (seType && seType->IsA() == TDataType::Class()) {
                           finalDataType = TDataType::GetDataType((EDataType)((TDataType *)seType)->GetType());
                        }
                     }
                  }

                  if (tempStreamerElement->IsaPointer()) {
                     offsets.push_back(offset);
                     offset = 0;
                  }

                  found = true;
                  break;
               }
            }
         }

         offsets.push_back(offset);

         if (found) {
            fStaticClassOffsets = offsets;
            fHaveStaticClassOffsets = true;

            if (fDict != finalDataType && fDict != elementClass) {
               errMsg = "Wrong data type ";
               errMsg += finalDataType ? finalDataType->GetName() : elementClass ? elementClass->GetName() : "UNKNOWN";
               fSetupStatus = kSetupMismatch;
               fProxy = nullptr;
               return nullptr;
            }
         }
      }

      if (!fHaveStaticClassOffsets) {
         errMsg = "The tree does not have a branch called ";
         errMsg += fBranchName;
         errMsg += ". You could check with TTree::Print() for available branches.";
         fSetupStatus = kSetupMissingBranch;
         fProxy = nullptr;
         return nullptr;
      }
   } else {
      myLeaf = branch->GetLeaf(TString(leafName(1, leafName.Length())));
      if (!myLeaf) {
         errMsg = "The tree does not have a branch, nor a sub-branch called ";
         errMsg += fBranchName;
         errMsg += ". You could check with TTree::Print() for available branches.";
         fSetupStatus = kSetupMissingBranch;
         fProxy = nullptr;
         return nullptr;
      } else {
         TDataType *tempDict = gROOT->GetType(myLeaf->GetTypeName());
         if (tempDict && fDict->IsA() == TDataType::Class() && tempDict->GetType() == ((TDataType *)fDict)->GetType()) {
            // fLeafOffset = myLeaf->GetOffset() / 4;
            branchActualType = fDict;
            fLeaf = myLeaf;
            fBranchName = branchName;
            fLeafName = leafName(1, leafName.Length());
            fHaveLeaf = fLeafName.Length() > 0;
            fSetupStatus = kSetupMatchLeaf;
         } else {
            errMsg = "Leaf of type ";
            errMsg += myLeaf->GetTypeName();
            errMsg += " cannot be read by TTreeReaderValue<";
            errMsg += fDict->GetName();
            errMsg += ">.";
            fSetupStatus = kSetupMismatch;
            return nullptr;
         }
      }
   }

   return branch;
}

////////////////////////////////////////////////////////////////////////////////
/// Create the proxy object for our branch.

void ROOT::Internal::TTreeReaderValueBase::CreateProxy()
{

   constexpr const char *errPrefix = "TTreeReaderValueBase::CreateProxy()";

   if (fProxy) {
      return;
   }

   fSetupStatus = kSetupInternalError; // Fallback; set to something concrete below.
   if (!fTreeReader) {
      Error(errPrefix, "TTreeReader object not set / available for branch %s!", fBranchName.Data());
      fSetupStatus = kSetupTreeDestructed;
      return;
   }

   auto branchFromFullName = fTreeReader->GetTree()->GetBranch(fBranchName);
   if (branchFromFullName == nullptr) // use the slower but more thorough FindBranch as fallback
      branchFromFullName = fTreeReader->GetTree()->FindBranch(fBranchName);

   if (!fDict) {
      if (!fOpaqueRead) {
         const char *brDataType = "{UNDETERMINED}";
         if (branchFromFullName) {
            TDictionary *brDictUnused = nullptr;
            brDataType = GetBranchDataType(branchFromFullName, brDictUnused, fDict);
         }
         Error(errPrefix,
               "The template argument type T of %s accessing branch %s (which contains data of type %s) is not known "
               "to ROOT. You will need to create a dictionary for it.",
               GetDerivedTypeName(), fBranchName.Data(), brDataType);
         fSetupStatus = kSetupMissingDictionary;
         return;
      } else {
         // We do not care about the branch data type in this case, so we can
         // generate the dictionary from the branch directly
         if (branchFromFullName) {
            GetBranchDataType(branchFromFullName, fDict, nullptr);
         }
      }
   }

   // Search for the branchname, determine what it contains, and wire the
   // TBranchProxy representing it to us so we can access its data.

   TNamedBranchProxy *namedProxy = fTreeReader->FindProxy(fBranchName);
   if (namedProxy && namedProxy->GetDict() == fDict) {
      fProxy = namedProxy->GetProxy();
      // But go on: we need to set fLeaf etc!
   }

   const std::string originalBranchName = fBranchName.Data();

   TLeaf *myLeaf = nullptr;
   TDictionary *branchActualType = nullptr;
   std::string errMsg;

   TBranch *branch = nullptr;
   // If the branch name contains at least a dot, we analyse it in detail and
   // we give priority to the branch identified over the one which is found
   // with the TTree::GetBranch method. This allows to correctly pick the desired
   // branch in cases where a TTree has two branches, one called for example
   // "w.v.a" and another one called "v.a".
   // This behaviour is described in ROOT-9312.
   if (fBranchName.Contains(".")) {
      branch = SearchBranchWithCompositeName(myLeaf, branchActualType, errMsg);
      // In rare cases where leaves contain the name of the branch as part of their
      // name and a dot in the branch name (such as: branch "b." and leaf "b.a")
      // the previous method may return the branch name ("b.") rather than the
      // leaf name ("b.a") as we expect.
      // For these cases, we do not have to give priority to the previously
      // identified branch since we are in a different situation.
      // This behaviour is described in ROOT-9757.
      if (branch && branch->IsA() == TBranchElement::Class() && branchFromFullName) {
         branch = branchFromFullName;
         fStaticClassOffsets = {};
         fHaveStaticClassOffsets = false;
      }
   }

   // Check whether the user wants to suppress errors for this specific branch
   // if it is missing. We have to use this information differently in two
   // different situations:
   // - If the branch was present in the first tree of the chain, but missing
   //   when switching to a new tree
   // - If the branch is missing from the first tree already. In this case we
   //   also need to postpone the creation of the branch proxy until at least
   //   one tree in the chain has that branch
   const auto &suppressErrorsForMissingBranches = fTreeReader->fSuppressErrorsForMissingBranches;
   const bool suppressErrorsForThisBranch =
      (suppressErrorsForMissingBranches.find(originalBranchName) != suppressErrorsForMissingBranches.cend());

   if (!branch) {
      // We had an error, the branch name had no "." or we simply did not find anything.
      // We check if we had a branch found with the full name with a dot in it.
      branch = branchFromFullName;
      if (!branch) {
         // Also that one was empty. We need to error out, but only if the user
         // did not explicitly request to avoid errors about missing branches
         fSetupStatus = kSetupMissingBranch;
         fProxy = nullptr;
         if (suppressErrorsForThisBranch)
            return;

         // Now we error out, first checking if we did not get a more specific
         // error message from SearchBranchWithCompositeName. If not, we go with
         // a generic message.
         if (errMsg.empty()) {
            errMsg = "The tree does not have a branch called ";
            errMsg += fBranchName.Data();
            errMsg += ". You could check with TTree::Print() for available branches.";
         }
#if !defined(_MSC_VER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
         Error(errPrefix, errMsg.c_str());
#if !defined(_MSC_VER)
#pragma GCC diagnostic pop
#endif
         return;
      }
      // The branch found with the simplest search approach was successful.
      // We reset the state, we continue
      fSetupStatus = kSetupInternalError;
      fStaticClassOffsets = {};
      fHaveStaticClassOffsets = false;
   }

   if (!myLeaf && !fHaveStaticClassOffsets) {
      // The following two lines cannot be swapped. The GetBranchDataType can
      // change the value of branchActualType
      const char *branchActualTypeName = GetBranchDataType(branch, branchActualType, fDict);
      if (!branchActualType) {
         Error(errPrefix, "The branch %s contains data of type %s, which does not have a dictionary.",
               fBranchName.Data(), branchActualTypeName ? branchActualTypeName : "{UNDETERMINED TYPE}");
         fProxy = nullptr;
         return;
      }

      // Check if the dictionaries are TClass instances and if there is inheritance
      // because in this case, we can read the values.
      auto dictAsClass = dynamic_cast<TClass *>(fDict);
      auto branchActualTypeAsClass = dynamic_cast<TClass *>(branchActualType);
      auto inheritance = dictAsClass && branchActualTypeAsClass && branchActualTypeAsClass->InheritsFrom(dictAsClass);
      bool typeinfoMatch = dictAsClass && dictAsClass->GetTypeInfo() &&
                           dictAsClass->GetTypeInfo() == branchActualTypeAsClass->GetTypeInfo();

      if (!inheritance && !typeinfoMatch && fDict != branchActualType) {
         TDataType *dictdt = dynamic_cast<TDataType *>(fDict);
         TDataType *actualdt = dynamic_cast<TDataType *>(branchActualType);
         TEnum *dictenum = dynamic_cast<TEnum *>(fDict);
         TEnum *actualenum = dynamic_cast<TEnum *>(branchActualType);
         bool complainAboutMismatch = true;
         if (dictdt && actualdt) {
            if (dictdt->GetType() > 0 && dictdt->GetType() == actualdt->GetType()) {
               // Same numerical type but different TDataType, likely Long64_t
               complainAboutMismatch = false;
            } else if ((actualdt->GetType() == kDouble32_t && dictdt->GetType() == kDouble_t) ||
                       (actualdt->GetType() == kFloat16_t && dictdt->GetType() == kFloat_t)) {
               // Double32_t and Float16_t never "decay" to their underlying type;
               // we need to identify them manually here (ROOT-8731).
               complainAboutMismatch = false;
            }
         } else if ((dictdt || dictenum) && (actualdt || actualenum)) {
            if ((dictdt && dictdt->GetType() == kInt_t && actualenum) ||
                (actualdt && actualdt->GetType() == kInt_t && dictenum))
               complainAboutMismatch = false;
            if ((dictdt && actualenum && dictdt->GetType() == actualenum->GetUnderlyingType()) ||
                (actualdt && dictenum && actualdt->GetType() == dictenum->GetUnderlyingType()))
               complainAboutMismatch = false;
         }
         if (complainAboutMismatch) {
            Error(errPrefix, "The branch %s contains data of type %s. It cannot be accessed by a TTreeReaderValue<%s>",
                  fBranchName.Data(), branchActualType->GetName(), fDict->GetName());
            return;
         }
      }
   }

   if (!namedProxy) {
      // Search for the branchname, determine what it contains, and wire the
      // TBranchProxy representing it to us so we can access its data.
      // A proxy for branch must not have been created before (i.e. check
      // fProxies before calling this function!)

      TString membername;

      bool isTopLevel = branch->GetMother() == branch;
      if (!isTopLevel) {
         membername = strrchr(branch->GetName(), '.');
         if (membername.IsNull()) {
            membername = branch->GetName();
         }
      }
      auto director = fTreeReader->fDirector.get();
      // Determine if the branch is actually in a Friend TTree and if so which.
      if (branch->GetTree() != fTreeReader->GetTree()->GetTree()) {
         // It is in a friend, let's find the 'index' in the list of friend ...
         std::optional<std::size_t> index;
         std::size_t current{};
         auto &&friends = fTreeReader->GetTree()->GetTree()->GetListOfFriends();
         for (auto fe : TRangeDynCast<TFriendElement>(friends)) {
            if (branch->GetTree() == fe->GetTree()) {
               index = current;
               break;
            }
            ++current;
         }
         if (!index.has_value()) {
            Error(errPrefix,
                  "The branch %s is contained in a Friend TTree that is not directly attached to the main.\n"
                  "This is not yet supported by TTreeReader.",
                  fBranchName.Data());
            return;
         }
         auto &&friendProxy = fTreeReader->AddFriendProxy(index.value());
         fTreeReader->AddProxy(std::make_unique<TNamedBranchProxy>(friendProxy.GetDirector(), branch,
                                                                   originalBranchName.c_str(), branch->GetName(),
                                                                   membername, suppressErrorsForThisBranch));
      } else {
         fTreeReader->AddProxy(std::make_unique<TNamedBranchProxy>(director, branch, originalBranchName.c_str(),
                                                                   membername, suppressErrorsForThisBranch));
      }
      namedProxy = fTreeReader->FindProxy(originalBranchName.c_str());
   }

   // Update named proxy's dictionary
   if (!namedProxy->GetDict())
      namedProxy->SetDict(fDict);

   fProxy = namedProxy->GetProxy();
   if (fProxy) {
      // If we have already reached the end of the tree, expose this information
      // also through the value, in case the user is not checking via the
      // TTreeReader but via the value instead.
      if (fTreeReader->GetEntryStatus() == TTreeReader::EEntryStatus::kEntryBeyondEnd)
         fSetupStatus = kSetupMatchButEntryBeyondEnd;
      else
         fSetupStatus = kSetupMatch;
   } else {
      fSetupStatus = kSetupMismatch;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Retrieve the type of data stored by branch; put its dictionary into
/// dict, return its type name. If no dictionary is available, at least
/// its type name should be returned.

const char *
ROOT::Internal::TTreeReaderValueBase::GetBranchDataType(TBranch *branch, TDictionary *&dict, TDictionary const *curDict)
{
   dict = nullptr;
   if (branch->IsA() == TBranchElement::Class()) {
      TBranchElement *brElement = (TBranchElement *)branch;

      auto ResolveTypedef = [&]() -> void {
         if (dict->IsA() != TDataType::Class())
            return;
         // Resolve the typedef.
         dict = TDictionary::GetDictionary(((TDataType *)dict)->GetTypeName());
         if (dict->IsA() != TDataType::Class()) {
            // Might be a class.
            if (dict != curDict) {
               dict = TClass::GetClass(brElement->GetTypeName());
            }
            if (dict != curDict) {
               dict = brElement->GetCurrentClass();
            }
         }
      };

      if (brElement->GetType() == TBranchElement::kSTLNode || brElement->GetType() == TBranchElement::kLeafNode ||
          brElement->GetType() == TBranchElement::kObjectNode) {

         TStreamerInfo *streamerInfo = brElement->GetInfo();
         Int_t id = brElement->GetID();

         if (id >= 0) {
            TStreamerElement *element = (TStreamerElement *)streamerInfo->GetElements()->At(id);
            if (element->IsA() == TStreamerSTL::Class()) {
               TStreamerSTL *myStl = (TStreamerSTL *)element;
               dict = myStl->GetClass();
               return nullptr;
            }
         }

         if (brElement->GetType() == 3 || brElement->GetType() == 4) {
            dict = brElement->GetCurrentClass();
            return brElement->GetTypeName();
         }

         if (brElement->GetTypeName())
            dict = TDictionary::GetDictionary(brElement->GetTypeName());

         if (dict)
            ResolveTypedef();
         else
            dict = brElement->GetCurrentClass();

         return brElement->GetTypeName();
      } else if (brElement->GetType() == TBranchElement::kClonesNode) {
         dict = TClonesArray::Class();
         return "TClonesArray";
      } else if (brElement->GetType() == 31 || brElement->GetType() == 41) {
         // it's a member, extract from GetClass()'s streamer info
         Error("TTreeReaderValueBase::GetBranchDataType()",
               "Must use TTreeReaderArray to access a member of an object that is stored in a collection.");
      } else if (brElement->GetType() == -1 && brElement->GetTypeName()) {
         dict = TDictionary::GetDictionary(brElement->GetTypeName());
         ResolveTypedef();
         return brElement->GetTypeName();
      } else {
         Error("TTreeReaderValueBase::GetBranchDataType()", "Unknown type and class combination: %i, %s",
               brElement->GetType(), brElement->GetClassName());
      }
      return nullptr;
   } else if (branch->IsA() == TBranch::Class() || branch->IsA() == TBranchObject::Class() ||
              branch->IsA() == TBranchSTL::Class()) {
      if (branch->GetTree()->IsA() == TNtuple::Class()) {
         dict = TDataType::GetDataType(kFloat_t);
         return dict->GetName();
      }
      const char *dataTypeName = branch->GetClassName();
      if ((!dataTypeName || !dataTypeName[0]) && branch->IsA() == TBranch::Class()) {
         TLeaf *myLeaf = branch->GetLeaf(branch->GetName());
         if (!myLeaf) {
            myLeaf = branch->FindLeaf(branch->GetName());
         }
         if (!myLeaf && branch->GetListOfLeaves()->GetEntries() == 1) {
            myLeaf = static_cast<TLeaf *>(branch->GetListOfLeaves()->UncheckedAt(0));
         }
         if (myLeaf) {
            TDictionary *myDataType = TDictionary::GetDictionary(myLeaf->GetTypeName());
            if (myDataType && myDataType->IsA() == TDataType::Class()) {
               if (myLeaf->GetLeafCount() != nullptr || myLeaf->GetLenStatic() > 1) {
                  Error("TTreeReaderValueBase::GetBranchDataType()",
                        "Must use TTreeReaderArray to read branch %s: it contains an array or a collection.",
                        branch->GetName());
                  return nullptr;
               }
               dict = TDataType::GetDataType((EDataType)((TDataType *)myDataType)->GetType());
               return myLeaf->GetTypeName();
            }
         }

         // leaflist. Can't represent.
         Error("TTreeReaderValueBase::GetBranchDataType()",
               "The branch %s was created using a leaf list and cannot be represented as a C++ type. Please access one "
               "of its siblings using a TTreeReaderArray:",
               branch->GetName());
         TIter iLeaves(branch->GetListOfLeaves());
         TLeaf *leaf = nullptr;
         while ((leaf = (TLeaf *)iLeaves())) {
            Error("TTreeReaderValueBase::GetBranchDataType()", "   %s.%s", branch->GetName(), leaf->GetName());
         }
         return nullptr;
      }
      if (dataTypeName)
         dict = TDictionary::GetDictionary(dataTypeName);
      return dataTypeName;
   } else if (branch->IsA() == TBranchClones::Class()) {
      dict = TClonesArray::Class();
      return "TClonesArray";
   } else if (branch->IsA() == TBranchRef::Class()) {
      // Can't represent.
      Error("TTreeReaderValueBase::GetBranchDataType()",
            "The branch %s is a TBranchRef and cannot be represented as a C++ type.", branch->GetName());
      return nullptr;
   } else {
      Error("TTreeReaderValueBase::GetBranchDataType()",
            "The branch %s is of type %s - something that is not handled yet.", branch->GetName(),
            branch->IsA()->GetName());
      return nullptr;
   }

   return nullptr;
}

void ROOT::Internal::TTreeReaderValueBase::ErrorAboutMissingProxyIfNeeded()
{
   // Print the error only if the branch name does not appear in the list of
   // missing proxies that the user explicitly requested not to error about
   if (!fTreeReader || fTreeReader->fMissingProxies.count(fBranchName.Data()) == 0)
      Error("TTreeReaderValue::Get()",
            "Value reader for branch %s not properly initialized, did you call "
            "TTreeReader::Set(Next)Entry() or TTreeReader::Next()?",
            fBranchName.Data());
}

namespace cling {
// The value printers of TTreeReaderValue and TTreeReaderArray rely on the
// one of TTreeReaderValueBase, from which they both inherit.
// This is why we use RTTI inside the function, avoiding to duplicate code.
// The performance penalty is irrelevant because we are already printing
// the objects in an interactive environment.
std::string printValue(ROOT::Internal::TTreeReaderValueBase *val)
{
   auto cl = TClass::GetClass(typeid(*val));
   std::string str = cl->GetName();
   str += " instance associated to column ";
   str += val->GetBranchName();
   return str;
}
} // namespace cling
