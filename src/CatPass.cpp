#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <algorithm>
#include <sstream>

#include "Noelle.hpp"

using namespace llvm::noelle ;

namespace {

  struct CAT : public ModulePass {
    static char ID; 

    CAT() : ModulePass(ID) {}

    bool doInitialization (Module &M) override {
      return false;
    }

    bool runOnModule (Module &M) override {

      /*
       * Fetch NOELLE
       */
      auto& noelle = getAnalysis<Noelle>();

      /*
       * Fetch the entry point.
       */
      auto fm = noelle.getFunctionsManager();
      auto mainF = fm->getEntryFunction();

      /*
       * Fetch the loops with all their abstractions 
       * (e.g., Loop Dependence Graph, SCCDAG)
       */
      auto loops = noelle.getLoops();
      auto PDG = noelle.getProgramDependenceGraph();
      //auto loops = noelle.getLoops(mainF);
      
      std::unordered_set<std::string> dependence_set;

      /*
       * Print loop induction variables and invariant.
       */
      int count = 1;
      for (auto loop : *loops){
        /*
         * Print the first instruction the loop executes.
         */
        auto LS = loop->getLoopStructure();
        auto loopid = loop->getID();
        errs() << "This is Loop " << loopid << " in function " << LS->getFunction()->getName() << "\n";

        auto instlist = LS->getInstructions();
        for(auto inst : instlist)
          errs() << "    " << *inst << "\n";

        for(auto inst : instlist){
          for(auto loop_compare: *loops){
            if(loop == loop_compare)
              continue;
            auto LS_compare = loop_compare->getLoopStructure();
            auto instlist_compare = LS_compare->getInstructions();
            auto loopid_compare = loop_compare->getID();
            for(auto inst_compare: instlist_compare){
              // get dependence information
              auto dependence = PDG->getDependences(inst, inst_compare);
              if(!dependence.empty()) {
                std::ostringstream oss;
                oss << loopid << " to " << loopid_compare;
                std::string temp = oss.str();
                dependence_set.insert(temp);
              }
              //for(auto dep: dependence)
              //  errs() << "Dependence exists from  " << loopid//*inst
              //    << " to " << loopid_compare/**inst_compare*/ << ": " << dep << "\n"; 
            }
          }
        }

        count++;
      }
      errs() << "\n";

      for(auto iter: dependence_set)
        errs() << "There is a dependence from Loop " << iter << "\n";
      /*
       * Fetch the loops with only the loop structure abstraction.
       */
      auto loopStructures = noelle.getLoopStructures();
      //auto loopStructures = noelle.getLoopStructures(mainF);

      /*
       * Iterate over all loops, 
       * and compute the LoopDependenceInfo only for those that we care.
       */
      for (auto l : *loopStructures){
        if (l->getNestingLevel() > 1){
          continue ;
        }

        /*
         * Get the LoopDependenceInfo
         */
        auto ldi = noelle.getLoop(l);
      }

      /*
       * Fetch the loop forest.
       */
      auto loopForest = noelle.organizeLoopsInTheirNestingForest(*loopStructures);

      /*
       * Define the iterator that will print all nodes of a tree.
       */
      std::function<void (StayConnectedNestedLoopForestNode *)> printTree = 
        [&printTree](StayConnectedNestedLoopForestNode *n){

        /*
         * Print the current node.
         */
        auto l = n->getLoop();
        for (auto i = 1; i < l->getNestingLevel(); i++){
          errs() << "-" ;
        }
        errs() << "-> ";
        errs() << "[ " << l->getFunction()->getName() << " ] " << *l->getEntryInstruction() << "\n";
        
        /*
         * Print the children
         */
        for (auto c : n->getDescendants()){
          printTree(c);
        }

        return ;
      };

      /*
       * Iterate over the trees that compose the forest.
       */
      errs() << "Printing the loop forest\n";
      for (auto loopTree : loopForest->getTrees()){
        
        /*
         * Fetch the root of the current tree.
         */
        auto rootLoop = loopTree->getLoop();
        errs() << "======= Tree with root " << *rootLoop->getEntryInstruction() << "\n";
        printTree(loopTree);
        errs() << "\n";
      }

      return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<Noelle>();
    }
  };
}

// Next there is code to register your pass to "opt"
char CAT::ID = 0;
static RegisterPass<CAT> X("CAT", "Simple user of the Noelle framework");

// Next there is code to register your pass to "clang"
static CAT * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new CAT()); }}); // ** for -O0
