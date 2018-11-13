//===- LoopRotation.cpp - Loop Rotation Pass ------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements Loop Rotation Pass.
//
//===----------------------------------------------------------------------===//

#include "helix/LoopPrologueSplitter.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include <iostream>
#include <queue>

using namespace llvm;
const char *endl = "\n";

#define DEBUG_TYPE "loop-helix"

LoopPrologueSplitter::LoopPrologueSplitter() {}

PreservedAnalyses LoopPrologueSplitter::run(Loop &L, LoopAnalysisManager &AM,
                                            LoopStandardAnalysisResults &AR,
                                            LPMUpdater &) {
  PreservedAnalyses pa;
  return pa;
}

namespace {

class LoopPrologueSplitterPass : public LoopPass {
  unsigned MaxHeaderSize;

public:
  static char ID;
  LoopPrologueSplitterPass() : LoopPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    std::cout << "getting analysis usage" << std::endl;
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
  }

  struct LoopCarried {
    LoopCarried(int i, PHINode *o) : idx(i), main_phi(o) {}
    int idx;
    PHINode *main_phi;
    Value *main_init_value = nullptr;
    Value *main_member_ptr = nullptr;
    Value *main_exit_value = nullptr;

    PHINode *worker_phi = nullptr;
    Value *worker_value_ptr = nullptr;
    LoadInst *worker_replacement_load = nullptr;
    SmallVector<StoreInst *, 10> worker_replacement_stores;

    // is needed to make the prologue decision
    bool is_induction_variable = false;
  };

  static StructType *getThreadDataStruct(Module *mod, LLVMContext &ctx) {
    StructType *thread_data_t = mod->getTypeByName("struct.ThreadBuffer");
    assert(thread_data_t != nullptr);

    auto &els = thread_data_t->elements();
    assert(els[0] == IntegerType::get(ctx, 8));
    assert(els[1] == IntegerType::get(ctx, 8));
    assert(els[2] == IntegerType::get(ctx, 64));
    assert(els[3] == thread_data_t->getPointerTo());
    return thread_data_t;
  }

  struct TDPointers {
    Value *keep_going;
    Value *start_prologue;
    Value *iteration_to_run;
    Value *next_thread_data;
    Value *next_thread_start;

    Value *current_td;
    Value *next_td;

    static TDPointers load(Value *tda, StructType *tds, IRBuilder<> &b) {
      TDPointers ptrs;
      ptrs.keep_going = b.CreateGEP(tds, tda, {b.getInt32(0), b.getInt32(0)},
                                    "keep_going_ptr");
      ptrs.start_prologue = b.CreateGEP(
          tds, tda, {b.getInt32(0), b.getInt32(1)}, "start_prologue_ptr");
      ptrs.iteration_to_run = b.CreateGEP(
          tds, tda, {b.getInt32(0), b.getInt32(2)}, "iteration_to_run_ptr");
      ptrs.next_thread_data = b.CreateGEP(
          tds, tda, {b.getInt32(0), b.getInt32(3)}, "next_thread_data_ptr");

      auto ntu = b.CreateLoad(ptrs.next_thread_data, "next_thread_data_value");

      ptrs.next_thread_start = b.CreateGEP(
          tds, ntu, {b.getInt32(0), b.getInt32(1)}, "next_thread_start");

      ptrs.current_td = b.CreateBitCast(tda, tds->getPointerTo(), "current_td");
      ptrs.next_td = b.CreateBitCast(ntu, tds->getPointerTo(), "next_td");

      return ptrs;
    }
  };

  using BlockVector = SmallVector<BasicBlock *, 20>;
  using ValueSet = std::set<Value *>;
  using LoopCarriedStore = SmallVector<LoopCarried, 20>;

  struct OriginalLoopDescription {
    OriginalLoopDescription(LLVMContext &ctx, Loop *L, PostDominatorTree &PDT) {
      assert(L->getNumBackEdges() == 1);

      preloop = L->getLoopPredecessor();
      assert(preloop != nullptr && "has to have a loop predecessor");

      header = L->getHeader();
      assert(header != nullptr && "loop needs to have a header");

      exit = L->getUniqueExitBlock();
      assert(exit != nullptr);

      for (auto *bb : L->getBlocks()) {
        if (PDT.dominates(header, bb) == false or bb == header) {
          prologue.push_back(bb);
          std::cout << "in prologue bb=" << bb->getName().str() << std::endl;
        } else if (bb != exit and bb != preloop) {
          body.push_back(bb);
          std::cout << "in body bb=" << bb->getName().str() << std::endl;
        }
      }

      assert(!body.empty());
      assert(!prologue.empty());

      std::cout << "body blocks=" << body.size()
                << " prologue blocks=" << prologue.size() << std::endl;
    }

    BasicBlock *preloop;
    BasicBlock *header;
    BasicBlock *exit;

    BlockVector prologue;
    BlockVector body;
  };

  // get all the instructions in the prologue that depend on values outside
  // also account for loops, as there might be circular dependencies through
  // phis in the prologue itself (eg. when we write to a value in the
  // prologue)
  ValueSet getExternalValueDependencies(BlockVector blocks) {
    ValueSet externalValues;
    ValueSet consideredValues;
    ValueSet internalBlocks(blocks.begin(), blocks.end());
    std::function<void(Value *)> getControllers;
    getControllers = [&](Value *v) {
      if (v == nullptr) {
        std::cout << "Value was a nullptr, ignoring" << std::endl;
      }

      // return every instruction that uses something outside the internal
      // bbs
      v->dump();

      if (consideredValues.count(v)) {
        std::cout << "detecting loop - returnign empty set" << std::endl;
        return;
      }
      consideredValues.insert(v);

      Instruction *i = dyn_cast<Instruction>(v);
      if (i == nullptr) {
        // std::cout << "Unable to cast to instruction" << std::endl;
        return;
      }

      bool depends_on_externals = false;

      for (Value *operand_value : i->operands()) {

        if (Argument *av = dyn_cast<Argument>(operand_value)) {
          depends_on_externals = true;
          // externalValues.insert(operand_value);
        } else if (Instruction *iv = dyn_cast<Instruction>(operand_value)) {

          if (internalBlocks.count(iv->getParent()) == 0) {
            depends_on_externals = true;
            // externalValues.insert(operand_value);
          }

          getControllers(operand_value);
        }
      }

      if (isa<PHINode>(i) and depends_on_externals) {
        externalValues.insert(i);
      }
    };

    for (auto bb : blocks) {
      getControllers(bb->getTerminator());
    }
    return externalValues;
  }

  LoopCarriedStore getLoopCarriedValues(const OriginalLoopDescription &old) {
    LoopCarriedStore lcv;

    auto ext_values = getExternalValueDependencies({old.prologue.back()});
    outs() << "external values" << endl;
    for (auto *v : ext_values) {
      outs() << *v << endl;
    }
    assert(!ext_values.empty() && "loop needs to have an external value");

    int i = 0;
    for (PHINode &phi : old.header->phis()) {
      lcv.emplace_back(i, &phi);
      i++;
      LoopCarried &c = *(lcv.end() - 1);

      if (ext_values.count(&phi)) {
        c.is_induction_variable = true;
      }

      /*
      for (const User *u : I.users()) {
        for (auto bb : bodyBlocks) {
          if (u->isUsedInBasicBlock(bb)) {
            c.loop_carried_dependency = true;
            break;
          }
        }
      }*/
    }
    return lcv;
  }

  struct HelixFunctionBody {

    HelixFunctionBody(LLVMContext &ctx, Function *f,
                      OriginalLoopDescription &old, LoopCarriedStore &lcv)
        : entry{BasicBlock::Create(ctx, "entry", f)},
          check_going{BasicBlock::Create(ctx, "check_going", f)},
          check_prologue{BasicBlock::Create(ctx, "check_prologue", f)} {
      int i = 0;
      for (auto bb : old.prologue) {
        auto r = CloneBasicBlock(bb, oldToNewInstMap, "prol", f);
        r->setName("prologue_" + std::to_string(i++));

        oldToNewBBMap[bb] = r;
        prologue.push_back(r);
      }
      remapInstructionsInBlocks(prologue, oldToNewInstMap);

      start_next = BasicBlock::Create(ctx, "start_next", f);

      i = 0;
      for (auto bb : old.body) {
        auto r = CloneBasicBlock(bb, oldToNewInstMap, "body", f);
        r->setName("body_" + std::to_string(i++));
        oldToNewBBMap[bb] = r;
        body.push_back(r);
      }
      remapInstructionsInBlocks(body, oldToNewInstMap);

      end = BasicBlock::Create(ctx, "end", f);

      for (auto *bb : prologue) {
        auto t = bb->getTerminator();
        for (unsigned i = 0; i < t->getNumOperands(); i++) {
          if (t->getOperand(i) == old.body[0]) {
            t->setOperand(i, start_next);
          }

          if (t->getOperand(i) == old.exit) {
            t->setOperand(i, end);
          }
        }
      }

      for (auto *bb : body) {
        auto t = bb->getTerminator();
        for (unsigned i = 0; i < t->getNumOperands(); i++) {
          if (t->getOperand(i) == old.prologue[0]) {
            t->setOperand(i, check_going);
          }
        }
      }

      // update the jump targets for the copied in blocks
      outs() << "updating jump targets" << endl;
      for (BasicBlock &bb : *f) {
        auto term = bb.getTerminator();
        if (term == nullptr) {
          continue;
        }
        Instruction *i = dyn_cast<Instruction>(term);
        for (unsigned x = 1; x < i->getNumOperands(); x++) {
          auto it = oldToNewBBMap.find(i->getOperand(x));
          if (it != oldToNewBBMap.end()) {
            i->setOperand(x, const_cast<Value *>(it->first));
          }
        }
      }

      outs() << "updating phi sources" << endl;
      // this could be an issue for phis that depend on the function arguments
      for (auto &v : lcv) {
        auto it = oldToNewInstMap.find(v.main_phi);
        assert(it != oldToNewInstMap.end() && "unable to find phi");
        v.worker_phi = const_cast<PHINode *>(cast<PHINode>(it->second));
        assert(v.worker_phi != v.main_phi && "should be different");

        outs() << "considering incoming values to worker phi: " << *v.worker_phi
               << "(" << v.worker_phi->getNumIncomingValues() << ")" << endl;

        // update phi inbounds
        for (unsigned i = 0; i < v.worker_phi->getNumIncomingValues(); i++) {
          Value *incoming = v.worker_phi->getIncomingValue(i);

          outs() << "considering: " << *incoming << endl;

          if (Instruction *I = dyn_cast<Instruction>(incoming)) {
            outs() << "is instruction with parent=" << *I->getParent() << endl;

            auto in_body = std::find(old.body.begin(), old.body.end(),
                                     I->getParent()) != old.body.end();
            auto in_prologue =
                std::find(old.prologue.begin(), old.prologue.end(),
                          I->getParent()) != old.prologue.end();

            outs() << "in_body=" << in_body << " in_prologue=" << in_prologue
                   << endl;

            if (!in_body and !in_prologue) {
              // this must be coming from outside the loop
              assert(v.main_init_value == nullptr &&
                     "can only have a single init");
              v.main_init_value = incoming;
              continue;
            }

            Value *new_incoming = oldToNewInstMap[incoming];
            if (new_incoming == nullptr) {
              // this was not an old instruction, so must be new, eg. can ignore
              continue;
            }
            Instruction *new_incoming_i = dyn_cast<Instruction>(new_incoming);
            v.worker_phi->setIncomingValue(i, new_incoming_i);
            v.worker_phi->setIncomingBlock(i, new_incoming_i->getParent());
          } else {
            outs() << "this is an arg or constant" << endl;
            assert(v.main_init_value == nullptr &&
                   "can only have a single init");
            v.main_init_value = incoming;
          }
        }

        std::cout << "result: ";
        v.worker_phi->dump();

        assert(v.main_init_value != nullptr && "init value must have been set");
      }
    }

    // hopefully not exposed
    ValueToValueMapTy oldToNewInstMap;
    ValueToValueMapTy oldToNewBBMap;

    // actual function entry data
    BasicBlock *entry;
    BasicBlock *check_going;
    BasicBlock *check_prologue;
    BlockVector prologue;
    BasicBlock *start_next;
    BlockVector body;
    BasicBlock *end;
  };

  Function *createHelixOptim(LLVMContext &ctx, Module *mod, FunctionType *ftype,
                             LoopCarriedStore &lcv,
                             OriginalLoopDescription &old, StructType *carry_t,
                             StructType *thread_data_t, StringRef name) {
    Constant *c = mod->getOrInsertFunction(name, ftype);
    Function *worker = cast<Function>(c);
    worker->setCallingConv(CallingConv::C);

    // Set names
    Function::arg_iterator args = worker->arg_begin();
    Value *td_arg = args++;
    td_arg->setName("thread_data");
    Value *carries_arg = args++;
    carries_arg->setName("carries");

    // Create the framework blocks
    HelixFunctionBody hb{ctx, worker, old, lcv};

    Function *thread_wait = cast<Function>(mod->getOrInsertFunction(
        "thread_wait",
        FunctionType::get(Type::getVoidTy(ctx),
                          {thread_data_t->getPointerTo(), Type::getInt8Ty(ctx)},
                          false)));
    assert(thread_wait != nullptr);
    // outs() << "thread_wait=" << *thread_wait << endl;

    Function *thread_signal = cast<Function>(mod->getOrInsertFunction(
        "thread_signal",
        FunctionType::get(Type::getVoidTy(ctx),
                          {thread_data_t->getPointerTo(), Type::getInt8Ty(ctx)},
                          false)));
    assert(thread_signal != nullptr);
    // outs() << "thread_signal=" << *thread_signal << endl;

    Function *dbgF = cast<Function>(mod->getOrInsertFunction(
        "print_abc",
        FunctionType::get(Type::getVoidTy(ctx),
                          {Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
                           Type::getInt32Ty(ctx)},
                          false)));

    outs() << "building function entry" << *worker << endl;
    IRBuilder<> entryBuilder(hb.entry);
    auto td_ptrs = TDPointers::load(td_arg, thread_data_t, entryBuilder);

    for (auto &v : lcv) {
      v.worker_value_ptr = entryBuilder.CreateGEP(
          carry_t,
          entryBuilder.CreateBitCast(carries_arg, carry_t->getPointerTo(),
                                     "typed_carry_ptr"),
          {entryBuilder.getInt32(0), entryBuilder.getInt32(v.idx)},
          v.is_induction_variable ? "induction_ptr" : "lcv_ptr");

      if (v.is_induction_variable) {
        v.worker_replacement_load = new LoadInst(
            v.worker_value_ptr, v.worker_value_ptr->getName() + "_load");

        outs() << "considering induction values: " << *v.worker_phi << endl;

        for (Value *Incoming : v.worker_phi->incoming_values()) {
          if (auto *I = dyn_cast<Instruction>(Incoming)) {
            BasicBlock *src = I->getParent();
            assert(src != nullptr && "Unable to find bb");
            outs() << "src bb=" << src->getName() << endl;

            // if this refers to the main function, don't emit a store
            if (src->getParent() != worker) {
              continue;
            }

            IRBuilder<> builder(src);

            auto *t = src->getTerminator();
            if (t) {
              builder.SetInsertPoint(t);
              auto st = builder.CreateStore(I, v.worker_value_ptr);
              v.worker_replacement_stores.push_back(st);
            }
          }
        }

        assert(v.worker_phi != nullptr);
        assert(v.worker_replacement_load != nullptr);
        ReplaceInstWithInst(v.worker_phi, v.worker_replacement_load);
      } else {
        outs() << "handling normal lcv of phi=" << *v.worker_phi << endl;

        assert(v.worker_phi->getNumUses() == 1);

        for (Value *use : v.worker_phi->users()) {
          if (auto *I = dyn_cast<Instruction>(use)) {
            // create the load before
            IRBuilder<> b{I};
            b.CreateCall(thread_wait, {td_ptrs.current_td, b.getInt8(v.idx)});
            v.worker_replacement_load = b.CreateLoad(
                v.worker_value_ptr, v.worker_value_ptr->getName() + "_load");

            v.worker_phi->replaceAllUsesWith(v.worker_replacement_load);
          }
        }

        for (Value *Incoming : v.worker_phi->incoming_values()) {
          if (auto *I = dyn_cast<Instruction>(Incoming)) {
            if (std::find(hb.body.begin(), hb.body.end(), I->getParent()) ==
                hb.body.end()) {
              outs() << "is outside body, ignoring" << endl;
              continue;
            }

            outs() << "incoming=" << *I << endl;
            // create the store after
            IRBuilder<> b{I->getNextNode()};
            b.CreateStore(I, v.worker_value_ptr);
            b.CreateCall(thread_signal, {td_ptrs.next_td, b.getInt8(v.idx)});
          }
        }

        outs() << "erasing phi=" << *v.worker_phi << endl;
        v.worker_phi->eraseFromParent();
      }

      // assert(v.worker)
      v.worker_phi = nullptr;
    }

    {
      outs() << "populating check_going" << endl;
      IRBuilder<> b(hb.check_going);
      auto l = b.CreateLoad(td_ptrs.keep_going);
      l->setAtomic(AtomicOrdering::SequentiallyConsistent);
      l->setAlignment(1);
      auto c = b.CreateICmpEQ(l, b.getInt8(0));
      b.CreateCondBr(c, hb.end, hb.check_prologue);
    }

    {
      outs() << "populating check_prologue" << endl;

      IRBuilder<> b(hb.check_prologue);
      auto l = b.CreateLoad(td_ptrs.start_prologue);
      l->setAtomic(AtomicOrdering::SequentiallyConsistent);
      l->setAlignment(1);
      auto c = b.CreateICmpEQ(l, b.getInt8(0));
      b.CreateCondBr(c, hb.check_going, hb.prologue[0]);
    }

    {
      outs() << "populating the store to reset start_prologue" << endl;

      auto bb = hb.prologue[0];
      IRBuilder<> b(bb);
      b.SetInsertPoint(bb, bb->getFirstInsertionPt());

      auto st = b.CreateStore(b.getInt8(0), td_ptrs.start_prologue);
      st->setAtomic(AtomicOrdering::SequentiallyConsistent);
      st->setAlignment(1);
    }

    {
      outs() << "populating start_next" << endl;

      IRBuilder<> b(hb.start_next);

      // option 3 by writing to the global state
      for (auto &v : lcv) {
        if (v.is_induction_variable) {
          SetVector<Instruction *> userInstructions;

          for (StoreInst *SI : v.worker_replacement_stores) {

            std::function<void(Instruction *)> t;
            t = [&t, &v, &userInstructions, &hb](Instruction *i) -> void {
              if (i == v.worker_replacement_load) {
                outs() << "found the initial load of this chain" << endl;
                return;
              }

              if (std::find(hb.body.begin(), hb.body.end(), i->getParent()) ==
                  hb.body.end()) {
                outs() << "is not part of helix body blocks" << *i << endl;
                return;
              }

              userInstructions.insert(i);

              for (Use &u : i->operands()) {
                if (auto op_i = dyn_cast<Instruction>(u.get())) {
                  t(op_i);
                }
              }
            };
            t(SI);
          }

          // TODO check whether these values aren't stored anywhere etc
          auto s = b.CreateLoad(v.worker_value_ptr);

          for (auto it = userInstructions.rbegin();
               it != userInstructions.rend(); ++it) {
            (*it)->moveBefore(s);
          }

          s->eraseFromParent();
        }
      }

      auto st = b.CreateStore(b.getInt8(1), td_ptrs.next_thread_start);
      st->setAtomic(AtomicOrdering::SequentiallyConsistent);
      st->setAlignment(1);
      b.CreateBr(hb.body[0]);
    }

    {
      outs() << "populating end" << endl;

      IRBuilder<> b(hb.end);
      Function *helixStop = cast<Function>(mod->getOrInsertFunction(
          "stop_helix",
          FunctionType::get(Type::getVoidTy(ctx),
                            {thread_data_t->getPointerTo()}, false)));
      b.CreateCall(helixStop, {td_arg});
      b.CreateRetVoid();
    }

    entryBuilder.CreateBr(hb.check_going);
    return worker;
  }

  void insertHelixOptim(LLVMContext &ctx, Module *mod, Function *f,
                        Function *worker, LoopCarriedStore &lcv,
                        OriginalLoopDescription &old, StructType *carry_t,
                        StructType *thread_data_t, Type *void_type) {

    BasicBlock *decider = SplitEdge(old.preloop, old.header);
    BasicBlock *helix = BasicBlock::Create(ctx, "helix", f, old.header);

    {
      decider->getTerminator()->eraseFromParent();

      IRBuilder<> b(decider);
      auto v = b.CreateICmpEQ(b.getInt8(1), b.getInt8(1), "run_helix");
      b.CreateCondBr(v, helix, old.header);
    }

    {
      IRBuilder<> b(helix);

      auto carries_alloc = b.CreateAlloca(carry_t, nullptr, "stackslot");

      for (auto &v : lcv) {
        v.main_member_ptr =
            b.CreateGEP(carry_t, carries_alloc,
                        {b.getInt32(0), b.getInt32(v.idx)}, "memberptr");
        // assume the init is the first value
        assert(v.main_init_value != nullptr && "init value needs to be set");
        b.CreateStore(v.main_init_value, v.main_member_ptr);
      }

      Constant *schedulerC = mod->getOrInsertFunction(
          "schedule_helix",
          FunctionType::get(
              Type::getVoidTy(ctx),
              {worker->getType(), void_type, Type::getInt32Ty(ctx)}, false));
      Function *scheduler = cast<Function>(schedulerC);

      b.CreateCall(
          scheduler,
          {worker, b.CreateBitCast(carries_alloc, void_type), b.getInt32(2)});


      for (auto &v : lcv) {
        v.main_exit_value = b.CreateLoad(v.main_member_ptr);
      }
      b.CreateBr(old.exit);

      for (PHINode &phi : old.exit->phis()) {
        Value *phi_op = phi.getOperand(0);

        auto c = [phi_op](LoopCarried &v) { return v.main_phi == phi_op; };
        auto it = std::find_if(lcv.begin(), lcv.end(), c);

        assert(it != lcv.end());
        phi.addIncoming(it->main_exit_value, helix);
      }
    }

    f->dump();
  }

  StructType *getCarryStructType(LLVMContext &ctx, LoopCarriedStore &lcv) {
    SmallVector<Type *, 10> carried_values;
    for (auto &v : lcv) {
      carried_values.push_back(v.main_phi->getType());
    }

    // gather the types needed for both the function and the call site
    return StructType::create(ctx, carried_values, "stackstruct");
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    Module *mod = L->getHeader()->getModule();
    Function *f = L->getHeader()->getParent();
    if (f->getName().contains("helix_optim")) {
      return false;
    }

    LLVMContext &ctx = mod->getContext();
    // DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    PostDominatorTree &PDT =
        getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();

    OriginalLoopDescription old{ctx, L, PDT};

    auto lcv = getLoopCarriedValues(old);
    for (auto &v : lcv) {
      outs() << "lcv: " << v.idx << " is_induction=" << v.is_induction_variable
             << " main_phi=" << *v.main_phi << endl;
    }
    // f->dump();

    // Get the loop-carried dependencies
    StructType *carry_t = getCarryStructType(ctx, lcv);
    StructType *thread_data_t = getThreadDataStruct(mod, ctx);
    Type *opaque_ptr_type = Type::getInt8PtrTy(ctx);
    FunctionType *helixWorkerType = FunctionType::get(
        Type::getVoidTy(ctx), {thread_data_t->getPointerTo(), opaque_ptr_type},
        false);

    /*
    Function *dbgF = cast<Function>(mod->getOrInsertFunction(
        "print_abc",
        FunctionType::get(Type::getVoidTy(ctx),
                          {Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
                           Type::getInt32Ty(ctx)},
                          false)));
                          */
    Twine name = "helix_optim_" + f->getName();
    auto worker = createHelixOptim(ctx, mod, helixWorkerType, lcv, old, carry_t,
                                   thread_data_t, name.str());

    outs() << "created worker function=" << *worker << endl;

    insertHelixOptim(ctx, mod, f, worker, lcv, old, carry_t, thread_data_t,
                     opaque_ptr_type);

    // f->dump();
    // worker->dump();
    return true;
  }
}; // namespace
} // namespace

char LoopPrologueSplitterPass::ID = 0;

static RegisterPass<LoopPrologueSplitterPass> X("loop-helix", "Apply HELIX transform to loops",
                             true,
                             true);
//INITIALIZE_PASS_DEPENDENCY(LoopPass)

/*
INITIALIZE_PASS_BEGIN(LoopPrologueSplitterPass, "",
                      "", false, false)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)

INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(LoopPrologueSplitterPass, "loop-helix",
                    "Apply HELIX transform to loops", false, false)

Pass *llvm::createLoopPrologueSplitterPass() {
  return new LoopPrologueSplitterPass();
}
*/
