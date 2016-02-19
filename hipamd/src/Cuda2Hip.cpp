//===---- tools/extra/ToolTemplate.cpp - Template for refactoring tool ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements an empty refactoring tool using the clang tooling.
//  The goal is to lower the "barrier to entry" for writing refactoring tools.
//
//  Usage:
//  tool-template <cmake-output-dir> <file1> <file2> ...
//
//  Where <cmake-output-dir> is a CMake build directory in which a file named
//  compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//  CMake to get this output).
//
//  <file1> ... specify the paths of files in the CMake source tree. This path
//  is looked up in the compile command database. If the path of a file is
//  absolute, it needs to point into CMake's source tree. If the path is
//  relative, the current working directory needs to be in the CMake source
//  tree and the file must be in a subdirectory of the current working
//  directory. "./" prefixes in the relative files will be automatically
//  removed, but the rest of a relative path must be a suffix of a path in
//  the compile command line database.
//
//  For example, to use tool-template on all files in a subtree of the
//  source tree, use:
//
//    /path/in/subtree $ find . -name '*.cpp'|
//        xargs tool-template /path/to/build
//
//===----------------------------------------------------------------------===//

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Debug.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"

#include <fstream>
#include <cstdio>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

#define DEBUG_TYPE "cuda2hip"

namespace {
  struct hipName {
  hipName() {
      // defines
      cuda2hipRename["__CUDACC__"] = "__HIPCC__";

      // includes
      cuda2hipRename["cuda_runtime.h"] = "hip_runtime.h";
      cuda2hipRename["cuda_runtime_api.h"] = "hip_runtime_api.h";

      // Error codes and return types:
      cuda2hipRename["cudaError_t"] = "hipError_t";
      cuda2hipRename["cudaError"] = "hipError";
      cuda2hipRename["cudaSuccess"] = "hipSuccess";

      cuda2hipRename["cudaErrorUnknown"] = "hipErrorUnknown";
      cuda2hipRename["cudaErrorMemoryAllocation"] = "hipErrorMemoryAllocation";
      cuda2hipRename["cudaErrorMemoryFree"] = "hipErrorMemoryFree";
      cuda2hipRename["cudaErrorUnknownSymbol"] = "hipErrorUnknownSymbol";
      cuda2hipRename["cudaErrorOutOfResources"] = "hipErrorOutOfResources";
      cuda2hipRename["cudaErrorInvalidValue"] = "hipErrorInvalidValue";
      cuda2hipRename["cudaErrorInvalidResourceHandle"] = "hipErrorInvalidResourceHandle";
      cuda2hipRename["cudaErrorInvalidDevice"] = "hipErrorInvalidDevice";
      cuda2hipRename["cudaErrorNoDevice"] = "hipErrorNoDevice";
      cuda2hipRename["cudaErrorNotReady"] = "hipErrorNotReady";
      cuda2hipRename["cudaErrorUnknown"] = "hipErrorUnknown";

      // error APIs:
      cuda2hipRename["cudaGetLastError"] = "hipGetLastError";
      cuda2hipRename["cudaPeekAtLastError"] = "hipPeekAtLastError";
      cuda2hipRename["cudaGetErrorName"] = "hipGetErrorName";
      cuda2hipRename["cudaGetErrorString"] = "hipGetErrorString";

      // Memcpy
      cuda2hipRename["cudaMemcpy"] = "hipMemcpy";
      cuda2hipRename["cudaMemcpyHostToHost"] = "hipMemcpyHostToHost";
      cuda2hipRename["cudaMemcpyHostToDevice"] = "hipMemcpyHostToDevice";
      cuda2hipRename["cudaMemcpyDeviceToHost"] = "hipMemcpyDeviceToHost";
      cuda2hipRename["cudaMemcpyDeviceToDevice"] = "hipMemcpyDeviceToDevice";
      cuda2hipRename["cudaMemcpyDefault"] = "hipMemcpyDefault";
      cuda2hipRename["cudaMemcpyToSymbol"] = "hipMemcpyToSymbol";
      cuda2hipRename["cudaMemset"] = "hipMemset";
      cuda2hipRename["cudaMemsetAsync"] = "hipMemsetAsync";
      cuda2hipRename["cudaMemcpyAsync"] = "hipMemcpyAsync";
      cuda2hipRename["cudaMemGetInfo"] = "hipMemGetInfo";
      cuda2hipRename["cudaMemcpyKind"] = "hipMemcpyKind";

      // Memory management :
      cuda2hipRename["cudaMalloc"] = "hipMalloc";
      cuda2hipRename["cudaMallocHost"] = "hipMallocHost";
      cuda2hipRename["cudaFree"] = "hipFree";
      cuda2hipRename["cudaFreeHost"] = "hipFreeHost";

      // Coordinate Indexing and Dimensions:
      cuda2hipRename["threadIdx.x"] = "hipThreadIdx_x";
      cuda2hipRename["threadIdx.y"] = "hipThreadIdx_y";
      cuda2hipRename["threadIdx.z"] = "hipThreadIdx_z";

      cuda2hipRename["blockIdx.x"] = "hipBlockIdx_x";
      cuda2hipRename["blockIdx.y"] = "hipBlockIdx_y";
      cuda2hipRename["blockIdx.z"] = "hipBlockIdx_z";

      cuda2hipRename["blockDim.x"] = "hipBlockDim_x";
      cuda2hipRename["blockDim.y"] = "hipBlockDim_y";
      cuda2hipRename["blockDim.z"] = "hipBlockDim_z";

      cuda2hipRename["gridDim.x"] = "hipGridDim_x";
      cuda2hipRename["gridDim.y"] = "hipGridDim_y";
      cuda2hipRename["gridDim.z"] = "hipGridDim_z";

      cuda2hipRename["blockIdx.x"] = "hipBlockIdx_x";
      cuda2hipRename["blockIdx.y"] = "hipBlockIdx_y";
      cuda2hipRename["blockIdx.z"] = "hipBlockIdx_z";

      cuda2hipRename["blockDim.x"] = "hipBlockDim_x";
      cuda2hipRename["blockDim.y"] = "hipBlockDim_y";
      cuda2hipRename["blockDim.z"] = "hipBlockDim_z";

      cuda2hipRename["gridDim.x"] = "hipGridDim_x";
      cuda2hipRename["gridDim.y"] = "hipGridDim_y";
      cuda2hipRename["gridDim.z"] = "hipGridDim_z";


      cuda2hipRename["warpSize"] = "hipWarpSize";

      // Events 
      cuda2hipRename["cudaEvent_t"] = "hipEvent_t";
      cuda2hipRename["cudaEventCreate"] = "hipEventCreate";
      cuda2hipRename["cudaEventCreateWithFlags"] = "hipEventCreateWithFlags";
      cuda2hipRename["cudaEventDestroy"] = "hipEventDestroy";
      cuda2hipRename["cudaEventRecord"] = "hipEventRecord";
      cuda2hipRename["cudaEventElapsedTime"] = "hipEventElapsedTime";
      cuda2hipRename["cudaEventSynchronize"] = "hipEventSynchronize";

      // Streams
      cuda2hipRename["cudaStream_t"] = "hipStream_t";
      cuda2hipRename["cudaStreamCreate"] = "hipStreamCreate";
      cuda2hipRename["cudaStreamCreateWithFlags"] = "hipStreamCreateWithFlags";
      cuda2hipRename["cudaStreamDestroy"] = "hipStreamDestroy";
      cuda2hipRename["cudaStreamWaitEvent"] = "hipStreamWaitEven";
      cuda2hipRename["cudaStreamSynchronize"] = "hipStreamSynchronize";
      cuda2hipRename["cudaStreamDefault"] = "hipStreamDefault";
      cuda2hipRename["cudaStreamNonBlocking"] = "hipStreamNonBlocking";

      // Other synchronization 
      cuda2hipRename["cudaDeviceSynchronize"] = "hipDeviceSynchronize";
      cuda2hipRename["cudaThreadSynchronize"] = "hipDeviceSynchronize";  // translate deprecated cudaThreadSynchronize
      cuda2hipRename["cudaDeviceReset"] = "hipDeviceReset";
      cuda2hipRename["cudaThreadExit"] = "hipDeviceReset";               // translate deprecated cudaThreadExit
      cuda2hipRename["cudaSetDevice"] = "hipSetDevice";
      cuda2hipRename["cudaGetDevice"] = "hipGetDevice";

      // Device 
      cuda2hipRename["cudaDeviceProp"] = "hipDeviceProp_t";
      cuda2hipRename["cudaGetDeviceProperties"] = "hipDeviceGetProperties";

      // Cache config
      cuda2hipRename["cudaDeviceSetCacheConfig"] = "hipDeviceSetCacheConfig";
      cuda2hipRename["cudaThreadSetCacheConfig"] = "hipDeviceSetCacheConfig"; // translate deprecated
      cuda2hipRename["cudaDeviceGetCacheConfig"] = "hipDeviceGetCacheConfig";
      cuda2hipRename["cudaThreadGetCacheConfig"] = "hipDeviceGetCacheConfig"; // translate deprecated
      cuda2hipRename["cudaFuncCache"] = "hipFuncCache";
      cuda2hipRename["cudaFuncCachePreferNone"] = "hipFuncCachePreferNone";
      cuda2hipRename["cudaFuncCachePreferShared"] = "hipFuncCachePreferShared";
      cuda2hipRename["cudaFuncCachePreferL1"] = "hipFuncCachePreferL1";
      cuda2hipRename["cudaFuncCachePreferEqual"] = "hipFuncCachePreferEqual";
      // function
      cuda2hipRename["cudaFuncSetCacheConfig"] = "hipFuncSetCacheConfig";

      cuda2hipRename["cudaDriverGetVersion"] = "hipDriverGetVersion";

      // Peer2Peer
      cuda2hipRename["cudaDeviceCanAccessPeer"] = "hipDeviceCanAccessPeer";
      cuda2hipRename["cudaDeviceDisablePeerAccess"] = "hipDeviceDisablePeerAccess";
      cuda2hipRename["cudaDeviceEnablePeerAccess"] = "hipDeviceEnablePeerAccess";
      cuda2hipRename["cudaMemcpyPeerAsync"] = "hipMemcpyPeerAsync";
      cuda2hipRename["cudaMemcpyPeer"] = "hipMemcpyPeer";

      // Shared mem:
      cuda2hipRename["cudaDeviceSetSharedMemConfig"] = "hipDeviceSetSharedMemConfig";
      cuda2hipRename["cudaThreadSetSharedMemConfig"] = "hipDeviceSetSharedMemConfig"; // translate deprecated
      cuda2hipRename["cudaDeviceGetSharedMemConfig"] = "hipDeviceGetSharedMemConfig";
      cuda2hipRename["cudaThreadGetSharedMemConfig"] = "hipDeviceGetSharedMemConfig";  // translate deprecated
      cuda2hipRename["cudaSharedMemConfig"] = "hipSharedMemConfig";
      cuda2hipRename["cudaSharedMemBankSizeDefault"] = "hipSharedMemBankSizeDefault";
      cuda2hipRename["cudaSharedMemBankSizeFourByte"] = "hipSharedMemBankSizeFourByte";
      cuda2hipRename["cudaSharedMemBankSizeEightByte"] = "hipSharedMemBankSizeEightByte";

      cuda2hipRename["cudaGetDeviceCount"] = "hipGetDeviceCount";

      // Profiler
      //cuda2hipRename["cudaProfilerInitialize"] = "hipProfilerInitialize";  // see if these are called anywhere.
      cuda2hipRename["cudaProfilerStart"] = "hipProfilerStart";
      cuda2hipRename["cudaProfilerStop"] = "hipProfilerStop";

      cuda2hipRename["cudaChannelFormatDesc"] = "hipChannelFormatDesc";
      cuda2hipRename["cudaFilterModePoint"] = "hipFilterModePoint";
      cuda2hipRename["cudaReadModeElementType"] = "hipReadModeElementType";

      cuda2hipRename["cudaCreateChannelDesc"] = "hipCreateChannelDesc";
      cuda2hipRename["cudaBindTexture"] = "hipBindTexture";
      cuda2hipRename["cudaUnbindTexture"] = "hipUnbindTexture";
    }
    DenseMap<StringRef, StringRef> cuda2hipRename;
  };

  struct HipifyPPCallbacks : public PPCallbacks, public SourceFileCallbacks {
    HipifyPPCallbacks(Replacements * R)
      : SeenEnd(false), _sm(nullptr), Replace(R)
    {
    }

    virtual bool handleBeginSource(CompilerInstance &CI, StringRef Filename) override
    {
      Preprocessor &PP = CI.getPreprocessor();
      SourceManager & SM = CI.getSourceManager();
      setSourceManager(&SM);
      PP.addPPCallbacks(std::unique_ptr<HipifyPPCallbacks>(this));
      PP.Retain();
      return true;
    }

    virtual void InclusionDirective(
      SourceLocation hash_loc,
      const Token &include_token,
      StringRef file_name,
      bool is_angled,
      CharSourceRange filename_range,
      const FileEntry *file,
      StringRef search_path,
      StringRef relative_path,
      const Module *imported) override
    {
      if (_sm->isWrittenInMainFile(hash_loc)) {
        if (is_angled) {
          if (N.cuda2hipRename.count(file_name)) {
            std::string repName = N.cuda2hipRename[file_name];
            llvm::errs() << "\nInclude file found: " << file_name << "\n";
            llvm::errs() << "\nSourceLocation:"; filename_range.getBegin().dump(*_sm);
            llvm::errs() << "\nWill be replaced with " << repName << "\n";
            SourceLocation sl = filename_range.getBegin();
            SourceLocation sle = filename_range.getEnd();
            const char* B = _sm->getCharacterData(sl);
            const char* E = _sm->getCharacterData(sle);
            repName = "<" + repName + ">";
            Replacement Rep(*_sm, sl, E - B, repName);
            Replace->insert(Rep);
          }
        }
      }
    }

    virtual void MacroDefined(const Token &MacroNameTok,
      const MacroDirective *MD) override
    {
      if (_sm->isWrittenInMainFile(MD->getLocation()) &&
          MD->getKind() == MacroDirective::MD_Define)
      {
        for (auto T : MD->getMacroInfo()->tokens())
        {
          if (T.isAnyIdentifier()) {
            StringRef name = T.getIdentifierInfo()->getName();
            if (N.cuda2hipRename.count(name)) {
              StringRef repName = N.cuda2hipRename[name];
              llvm::errs() << "\nIdentifier " << name
              << " found in definition of macro " << MacroNameTok.getIdentifierInfo()->getName() << "\n";
              llvm::errs() << "\nwill be replaced with: " << repName << "\n";
              SourceLocation sl = T.getLocation();
              llvm::errs() << "\nSourceLocation: ";sl.dump(*_sm);
              llvm::errs() << "\n";
              Replacement Rep(*_sm, sl, name.size(), repName);
              Replace->insert(Rep);
            }
          }
        }
      }
    }

    void EndOfMainFile() override 
    { 
   
    }
    
    bool SeenEnd;
    void setSourceManager(SourceManager * sm) { _sm = sm; }

    private:

    SourceManager * _sm;
    Replacements * Replace;
    struct hipName N;
  };

class Cuda2HipCallback : public MatchFinder::MatchCallback {
 public:
   Cuda2HipCallback(Replacements *Replace) : Replace(Replace) {}

  void run(const MatchFinder::MatchResult &Result) override {

    SourceManager * SM = Result.SourceManager;

	  if (const CallExpr * call = Result.Nodes.getNodeAs<clang::CallExpr>("cudaCall"))
	  {
		  const FunctionDecl * funcDcl = call->getDirectCallee();
		  std::string name = funcDcl->getDeclName().getAsString();
		  if (N.cuda2hipRename.count(name)) {
			  std::string repName = N.cuda2hipRename[name];
        SourceLocation sl = call->getLocStart();
			  Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
         SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
			  Replace->insert(Rep);
		  }
	  }

	  if (const CUDAKernelCallExpr * launchKernel = Result.Nodes.getNodeAs<clang::CUDAKernelCallExpr>("cudaLaunchKernel"))
	  {
      LangOptions DefaultLangOptions;

		  const FunctionDecl * kernelDecl = launchKernel->getDirectCallee();

      const ParmVarDecl * pvdFirst = kernelDecl->getParamDecl(0);
      const ParmVarDecl * pvdLast = kernelDecl->getParamDecl(kernelDecl->getNumParams()-1);
      SourceLocation kernelArgListStart(pvdFirst->getLocStart());
      SourceLocation kernelArgListEnd(pvdLast->getLocEnd());
      SourceLocation stop = clang::Lexer::getLocForEndOfToken(kernelArgListEnd, 0, *SM, DefaultLangOptions);
      size_t replacementLength = SM->getCharacterData(stop) - SM->getCharacterData(kernelArgListStart);
      std::string outs(SM->getCharacterData(kernelArgListStart), replacementLength);
      llvm::outs() << "initial paramlist: " << outs.c_str() << "\n";
      outs = "hipLaunchParm lp, " + outs;
      llvm::outs() << "new paramlist: " << outs.c_str() << "\n";
      Replacement Rep0(*(Result.SourceManager), kernelArgListStart, replacementLength, outs);
      Replace->insert(Rep0);


      std::string name = kernelDecl->getDeclName().getAsString();
      std::string repName = "hipLaunchKernel(HIP_KERNEL_NAME(" + name + "), ";
      

      const CallExpr * config = launchKernel->getConfig();
      llvm::outs() << "\nKernel config arguments:\n";
      for (unsigned argno = 0; argno < config->getNumArgs(); argno++)
      {
        const Expr * arg = config->getArg(argno);
        if (!isa<CXXDefaultArgExpr>(arg)) {
          std::string typeCtor = "";
          const ParmVarDecl * pvd = config->getDirectCallee()->getParamDecl(argno);
          
          SourceLocation sl(arg->getLocStart());
          SourceLocation el(arg->getLocEnd());
          SourceLocation stop = clang::Lexer::getLocForEndOfToken(el, 0, *SM, DefaultLangOptions);
          std::string outs(SM->getCharacterData(sl), SM->getCharacterData(stop) - SM->getCharacterData(sl));
          llvm::outs() << "args[ " << argno << "]" << outs.c_str() << " <" << pvd->getType().getAsString() << ">\n";
          if (pvd->getType().getAsString().compare("dim3") == 0)
            repName += " dim3(" + outs + "),";
          else
            repName += " " + outs + ",";
        } else
          repName += " 0,";
      }
 
      for (unsigned argno = 0; argno < launchKernel->getNumArgs(); argno++)
      {
        const Expr * arg = launchKernel->getArg(argno);
        SourceLocation sl(arg->getLocStart());
        SourceLocation el(arg->getLocEnd());
        SourceLocation stop = clang::Lexer::getLocForEndOfToken(el, 0, *SM, DefaultLangOptions);
        std::string outs(SM->getCharacterData(sl), SM->getCharacterData(stop) - SM->getCharacterData(sl));
        llvm::outs() << outs.c_str() << "\n";
        repName += " " + outs + ",";
      }
      repName.pop_back();
      repName += ")";
      size_t length = SM->getCharacterData(clang::Lexer::getLocForEndOfToken(launchKernel->getLocEnd(), 0, *SM, DefaultLangOptions)) -
        SM->getCharacterData(launchKernel->getLocStart());
      Replacement Rep(*SM, launchKernel->getLocStart(), length, repName);
      Replace->insert(Rep);
	  }

	  if (const MemberExpr * threadIdx = Result.Nodes.getNodeAs<clang::MemberExpr>("cudaBuiltin"))
	  {
		  if (const OpaqueValueExpr * refBase = dyn_cast<OpaqueValueExpr>(threadIdx->getBase())) {
        if (const DeclRefExpr * declRef = dyn_cast<DeclRefExpr>(refBase->getSourceExpr())) {
          std::string name = declRef->getDecl()->getNameAsString();
          std::string memberName = threadIdx->getMemberDecl()->getNameAsString();
          size_t pos = memberName.find_first_not_of("__fetch_builtin_");
          memberName = memberName.substr(pos, memberName.length() - pos);
          name += "." + memberName;
     		  std::string repName = N.cuda2hipRename[name];
          SourceLocation sl = threadIdx->getLocStart();
          Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
          SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
			    Replace->insert(Rep);
        }
		  }
	  }

    if (const DeclRefExpr * cudaEnumConstantRef = Result.Nodes.getNodeAs<clang::DeclRefExpr>("cudaEnumConstantRef"))
    {
      std::string name = cudaEnumConstantRef->getDecl()->getNameAsString();
      std::string repName = N.cuda2hipRename[name];
      SourceLocation sl = cudaEnumConstantRef->getLocStart();
      Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
        SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
      Replace->insert(Rep);
    }

    if (const VarDecl * cudaEnumConstantDecl = Result.Nodes.getNodeAs<clang::VarDecl>("cudaEnumConstantDecl"))
    {
      std::string name = cudaEnumConstantDecl->getType()->getAsTagDecl()->getNameAsString();
      std::string repName = N.cuda2hipRename[name];
      SourceLocation sl = cudaEnumConstantDecl->getLocStart();
      Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
        SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
      Replace->insert(Rep);
    }

    if (const VarDecl * cudaStructVar = Result.Nodes.getNodeAs<clang::VarDecl>("cudaStructVar"))
    {
      std::string name = 
        cudaStructVar->getType()->getAsStructureType()->getDecl()->getNameAsString();
      std::string repName = N.cuda2hipRename[name];
      SourceLocation sl = cudaStructVar->getLocStart();
      Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
        SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
      Replace->insert(Rep);
    }

    if (const ParmVarDecl * cudaParamDecl = Result.Nodes.getNodeAs<clang::ParmVarDecl>("cudaParamDecl"))
    {
      QualType QT = cudaParamDecl->getOriginalType();
      std::string name = QT.getAsString();
      std::string repName = N.cuda2hipRename[name];
      SourceLocation sl = cudaParamDecl->getLocStart();
      Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
        SM->getImmediateSpellingLoc(sl) : sl, name.length(), repName);
      Replace->insert(Rep);
    }
    if (const StringLiteral * stringLiteral = Result.Nodes.getNodeAs<clang::StringLiteral>("stringLiteral"))
    {
      std::string s = stringLiteral->getString();
      std::string search("cuda"), replace("hip");
      size_t pos = 0;
      while ((pos = s.find("cuda", pos)) != std::string::npos) {
        llvm::outs() << "String Literal: " << s << "\n";
        s.replace(pos, search.length(), replace);
        pos += replace.length();
        SourceLocation sl = stringLiteral->getLocStart();
        Replacement Rep(*SM, SM->isMacroArgExpansion(sl) ?
         SM->getImmediateSpellingLoc(sl) : sl, stringLiteral->getLength(), s);
        Replace->insert(Rep);
      }
    }
  }

 private:
  Replacements *Replace;
  struct hipName N;
};

} // end anonymous namespace

// Set up the command line options
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::OptionCategory ToolTemplateCategory("CUDA to HIP source translator options");

int main(int argc, const char **argv) {

  llvm::sys::PrintStackTraceOnErrorSignal();

  int Result;
  
  CommonOptionsParser OptionsParser(argc, argv, ToolTemplateCategory);
  std::vector<std::string> savedSources;
  for (auto I : OptionsParser.getSourcePathList())
  {
    size_t pos = I.find(".cu");
    if (pos != std::string::npos)
    {
      std::string dst = I.substr(0, pos) + ".hip.cu";
      std::ifstream source(I, std::ios::binary);
      std::ofstream dest(dst, std::ios::binary);
      dest << source.rdbuf();
      source.close();
      dest.close();
      savedSources.push_back(dst);
    }
  }

  RefactoringTool Tool(OptionsParser.getCompilations(), savedSources);
  ast_matchers::MatchFinder Finder;
  Cuda2HipCallback Callback(&Tool.getReplacements());
  HipifyPPCallbacks PPCallbacks(&Tool.getReplacements());
  Finder.addMatcher(callExpr(isExpansionInMainFile(), callee(functionDecl(matchesName("cuda.*")))).bind("cudaCall"), &Callback);
  Finder.addMatcher(cudaKernelCallExpr().bind("cudaLaunchKernel"), &Callback);
  Finder.addMatcher(memberExpr(isExpansionInMainFile(), hasObjectExpression(hasType(cxxRecordDecl(matchesName("__cuda_builtin_"))))).bind("cudaBuiltin"), &Callback);
  Finder.addMatcher(declRefExpr(isExpansionInMainFile(), to(enumConstantDecl(matchesName("cuda.*")))).bind("cudaEnumConstantRef"), &Callback);
  Finder.addMatcher(varDecl(isExpansionInMainFile(), hasType(enumDecl(matchesName("cuda.*")))).bind("cudaEnumConstantDecl"), &Callback);
  Finder.addMatcher(varDecl(isExpansionInMainFile(), hasType(cxxRecordDecl(matchesName("cuda.*")))).bind("cudaStructVar"), &Callback);
  Finder.addMatcher(parmVarDecl(isExpansionInMainFile(), hasType(namedDecl(matchesName("cuda.*")))).bind("cudaParamDecl"), &Callback);
  Finder.addMatcher(stringLiteral().bind("stringLiteral"), &Callback);

  auto action = newFrontendActionFactory(&Finder, &PPCallbacks);

  std::vector<const char *> compilationStages;
  compilationStages.push_back("--cuda-host-only");
  compilationStages.push_back("--cuda-device-only");

  for (auto Stage : compilationStages)
  {
    Tool.appendArgumentsAdjuster(combineAdjusters(
       getInsertArgumentAdjuster(Stage, ArgumentInsertPosition::BEGIN),
       getClangSyntaxOnlyAdjuster()));

    Result = Tool.run(action.get());

	  Tool.clearArgumentsAdjusters();
  }
  
  LangOptions DefaultLangOptions;
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);
  DiagnosticsEngine Diagnostics(
    IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
    &*DiagOpts, &DiagnosticPrinter, false);
  SourceManager Sources(Diagnostics, Tool.getFiles());

  llvm::outs() << "Replacements collected by the tool:\n";
  for (auto &r : Tool.getReplacements()) {
    llvm::outs() << r.toString() << "\n";
  }

  Rewriter Rewrite(Sources, DefaultLangOptions);

  if (!Tool.applyAllReplacements(Rewrite)) {
    llvm::errs() << "Skipped some replacements.\n";
  }

  Result = Rewrite.overwriteChangedFiles();


  for (auto I : savedSources)
  {
	  size_t pos = I.find(".cu");
	  if (pos != std::string::npos)
	  {
		  rename(I.c_str(), I.substr(0, pos).c_str());
	  }
  }
  return Result;
}
