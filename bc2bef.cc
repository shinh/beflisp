#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/system_error.h>

using namespace llvm;
using namespace std;

static const int kLocalPos = 9 * 9 * 9 * 9 * 8;
static const int kStackPos = 9 * 9 * 9 * 9 * 8;
static const int kGlobalPos = 9 * 9 * 9 * 9 * 9 * 6;
static const int kHeapPos = 9 * 9 * 9 * 9 * 9 * 8;

bool is_debug;

int getConstInt(const Value* v) {
  auto cv = dynamic_cast<const ConstantInt*>(v);
  assert(cv);
 return cv->getLimitedValue();
}

int getSizeOfType(const Type* t) {
  if (t->isPointerTy())
    return 1;
  if (t->isIntegerTy()) {
    assert(t->getPrimitiveSizeInBits() > 0);
    assert(t->getPrimitiveSizeInBits() <= 32);
    return 1;
  }
  if (t->isArrayTy()) {
    auto at = static_cast<const ArrayType*>(t);
    return (at->getNumElements() * getSizeOfType(at->getElementType()));
  }
  if (t->isStructTy()) {
    auto st = static_cast<const StructType*>(t);
    int s = 0;
    for (size_t i = 0; i < st->getNumElements(); i++) {
      s += getSizeOfType(st->getElementType(i));
    }
    return s;
  }
  t->dump();
  assert(false);
}

class B2B {
  enum MemType {
    LOCAL,
    MEM,
    PHI,
  };

public:
  explicit B2B(Module* module) : module_(module) {
  }

  void filterInstructions() {
    for (Function& func : module_->getFunctionList()) {
      for (BasicBlock& block : func.getBasicBlockList()) {
        vector<Instruction*> removes;
        for (Instruction& inst : block.getInstList()) {
          switch (inst.getOpcode()) {
            case Instruction::Call: {
              const Function* func =
                  static_cast<const CallInst&>(inst).getCalledFunction();
              assert(func);
              if (func->getName() != "llvm.lifetime.start" &&
                  func->getName() != "llvm.lifetime.end") {
                break;
              }
            } // fall through
#if 0
            case Instruction::BitCast:
            case Instruction::SExt:
#endif
              removes.push_back(&inst);
          }
        }

        for (Instruction* inst : removes) {
          inst->dump();
          inst->removeFromParent();
        }
      }
    }
  }

  void run() {
    filterInstructions();
    int entry_point = assignIds();
    emitSetup(entry_point);
    translate();
  }

  const string& code() const { return code_; }
  const vector<string>& befunge() const { return bef_; }

private:
  int assignIds() {
    int block_id = 0;
    int entry_point = -1;
    for (const Function& func : module_->getFunctionList()) {
      if (func.isDeclaration())
        continue;

      int id = 0;
      func_map_[&func] = block_id;

      if (func.getName() == "main")
        entry_point = block_id;

      for (const Argument& arg : func.getArgumentList()) {
        id_map_.insert(make_pair(&arg, id));
        id++;
      }

      for (const BasicBlock& block : func.getBasicBlockList()) {
        block_map_.insert(make_pair(&block, block_id));
        for (const Instruction& inst : block.getInstList()) {
          block_map_.insert(make_pair(&inst, block_id));
          id_map_.insert(make_pair(&inst, id));
          id++;

          if (inst.getOpcode() == Instruction::Call) {
            const Function* func =
                static_cast<const CallInst&>(inst).getCalledFunction();
            assert(func);
            if (func->getName() != "putchar" &&
                func->getName() != "getchar" &&
                func->getName() != "calloc" &&
                func->getName() != "free" &&
                func->getName() != "puts" &&
                func->getName() != "exit") {
              assert(inst.getNextNode());
              block_id++;
            }
          }
        }
        block_id++;
      }

      local_size_map_[&func] = id;
    }
    return entry_point;
  }

  void translate() {
    const string indent(20, ' ');
    for (const Function& func : module_->getFunctionList()) {
      if (func.isDeclaration())
        continue;

      stack_size_ = 0;

      char buf[999];
      sprintf(buf, "*** %s *** %d", func.getName().data(), func_map_[&func]);
      bef_.push_back(indent + buf);

      for (const Argument& arg : func.getArgumentList())
        set(LOCAL, id_map_[&arg]);

      const Instruction* last_inst = NULL;
      for (const BasicBlock& block : func.getBasicBlockList()) {
        if (is_debug) {
          sprintf(buf, "block %d", block_map_[&block]);
          bef_.push_back(indent + buf);
          for (const Instruction& inst : block.getInstList()) {
            ostringstream oss;
            raw_os_ostream ros(oss);
            inst.print(ros);
            bef_.push_back(indent + oss.str().substr(0, oss.str().find('\n')));
          }
        }

        for (const Instruction& inst : block.getInstList()) {
          fprintf(stderr, "%u %s %d\n",
                  inst.getOpcode(), inst.getOpcodeName(), inst.hasMetadata());
          last_inst = &inst;
          handleInstrcution(inst);
          if (inst.getOpcode() != Instruction::Br &&
              inst.getOpcode() != Instruction::Switch &&
              inst.getOpcode() != Instruction::Store &&
              inst.getOpcode() != Instruction::Ret) {
            set(LOCAL, getInstId(inst));
          }
          code_ += ' ';
        }

        assert(last_inst);
        emitBlock(*last_inst);
      }
    }
  }

  int getInstId(const Value& inst) {
    auto found = id_map_.find(&inst);
    assert(found != id_map_.end());
    return found->second;
  }

  void emitSetup(int entry_point) {
    genInt(kStackPos);
    setStackPointer();
    genInt(kHeapPos);
    setHeapPointer();
    genInt(kLocalPos);
    setLocalPointer();
    genInt(entry_point);

    setupGlobalVars();

    emitCode(0, false);
    code_.clear();

    emitChar(6, bef_.size() - 1, '<');
    emitChar(0, bef_.size() - 1, 'v');
  }

  void setupGlobalVars() {
    int global_id = kGlobalPos;
    for (const GlobalVariable& gv : module_->getGlobalList()) {
      if (gv.getInitializer()->getAggregateElement(0U)) {
        // TODO: check if this is a const char?
        continue;
      }

      if (!dynamic_cast<const ConstantPointerNull*>(gv.getInitializer())) {
        int v = getConstInt(gv.getInitializer());
        if (v) {
          genInt(v);
          genInt(global_id);
          make2D(MEM);
          code_ += 'p';
        }
      }

      global_map_.insert(make_pair(&gv, global_id));
      global_id += getSizeOfType(gv.getType());
    }
  }

  void emitBlock(const Value& block) {
    int block_id = block_map_.at(&block);
    bef_.push_back(">:#v_ >$");
    bef_.push_back("v-1<>  1+^");
    bef_.push_back("v   ^_^#:<");

    genInt(block_id);
    code_ += "-:0`!";
    int y = bef_.size() - 3;
    emitCode(y, true);
  }

  void emitCode(int oy, bool is_block) {
    int x = 10;
    int y = oy;
    int dx = 1;
    for (char c : code_) {
      if (c == 'S' || c == 'G') {
        static const int kMargin = 19;
        if (x > 78 - kMargin && dx == 1) {
          emitChar(x, y, 'v');
          emitChar(x, y + 1, '<');
          y++;
          x--;
          dx = -dx;
        }
        if (x < 10 + kMargin && dx == -1) {
          emitChar(x, y, 'v');
          emitChar(x, y + 1, '>');
          y++;
          x++;
          dx = -dx;
        }

        string code;
        if (c == 'S') {
          if (dx > 0) {
            code = "> #0 #\\_$";
          } else {
            code = "<!#0 #\\_$";
          }
        } else if (c == 'G') {
          code = "#@~";
        } else {
          assert(false);
        }

        for (char c : code) {
          emitChar(x, y, c);
          x += dx;
        }
        continue;
      }

      if (x == 78 && dx == 1) {
        emitChar(x, y, 'v');
        emitChar(x, y + 1, '<');
        y++;
        x--;
        dx = -dx;
      } else if (x == 10 && dx == -1) {
        emitChar(x, y, 'v');
        emitChar(x, y + 1, '>');
        y++;
        x++;
        dx = -dx;
      }

      emitChar(x, y, c);
      x += dx;
    }

    if (is_block) {
      if (x > 75) {
        if (dx > 0) {
          emitChar(x, y, 'v');
          emitChar(x, y + 1, '<');
          y++;
        }
        x = 75;
      }
      emitChar(x, y, 'v');
      y = max(y + 1, oy + 2);
      emitChar(x, y, '_');
      emitChar(x + 1, y, '1');
      emitChar(x + 2, y, '-');

      if (oy + 3 != (int)bef_.size()) {
        emitChar(0, bef_.size() - 1, 'v');
        emitChar(9, bef_.size() - 1, '^');
      }
    } else {
      if (dx > 0) {
        emitChar(x, y, 'v');
        emitChar(x, y + 1, '<');
        y++;
        x--;
        dx = -dx;
      }
    }

    code_.clear();
  }

  void emitChar(int x, int y, char c) {
    bef_.resize(max<int>(bef_.size(), y + 1));
    bef_[y].resize(max<int>(bef_[y].size(), x + 1), ' ');
    bef_[y][x] = c;
  }

  void handleInstrcution(const Instruction& inst) {
    switch (inst.getOpcode()) {
      case Instruction::Call:
        handleCall(static_cast<const CallInst&>(inst));
        break;

      case Instruction::Add:
      case Instruction::Sub:
      case Instruction::Mul:
      case Instruction::SDiv:
      case Instruction::SRem:
      case Instruction::And:
      case Instruction::Or:
      case Instruction::Xor:
        handleArith(inst);
        break;

      case Instruction::BitCast:
      case Instruction::PtrToInt:
      case Instruction::SExt:
      case Instruction::ZExt:
        getLocal(inst.getOperand(0));
        break;

      case Instruction::GetElementPtr:
        handleGetElementPtr(static_cast<const GetElementPtrInst&>(inst));
        break;

      case Instruction::ICmp:
        handleCmp(static_cast<const ICmpInst&>(inst));
        break;

      case Instruction::Br:
        handleBr(inst);
        break;

      case Instruction::Switch:
        handleSwitch(static_cast<const SwitchInst&>(inst));
        break;

      case Instruction::Select:
        getLocal(inst.getOperand(2));
        getLocal(inst.getOperand(1));
        getLocal(inst.getOperand(0));
        code_ += 'S';
        break;

      case Instruction::PHI:
        get(PHI, getInstId(inst));
        break;

      case Instruction::Ret:
        handleRet(inst);
        break;

      case Instruction::Alloca:
        handleAlloca(static_cast<const AllocaInst&>(inst));
        break;

      case Instruction::Load:
        getLocal(inst.getOperand(0));
        make2D(MEM);
        code_ += 'g';
        break;

      case Instruction::Store:
        getLocal(inst.getOperand(0));
        //code_ += ":.";
        getLocal(inst.getOperand(1));
        //code_ += ":.";
        make2D(MEM);
        code_ += 'p';
        break;

      default:
        fprintf(stderr, "Unknown op: %s\n", inst.getOpcodeName());
        assert(false);
    }
  }

  void handleArith(const Instruction& inst) {
    getLocal(inst.getOperand(0));
    getLocal(inst.getOperand(1));
    switch (inst.getOpcode()) {
      case Instruction::Xor:
        assert(inst.getType()->getPrimitiveSizeInBits() == 1);
      case Instruction::Or:
      case Instruction::Add:
        code_ += '+'; break;
      case Instruction::Sub:
        code_ += '-'; break;
      case Instruction::And:
        assert(inst.getType()->getPrimitiveSizeInBits() == 1);
      case Instruction::Mul:
        code_ += '*'; break;
      case Instruction::SDiv:
        code_ += '/'; break;
      case Instruction::SRem:
        code_ += '%'; break;
      default:
        assert(false);
    }

    if (inst.getOpcode() == Instruction::Xor) {
      code_ += "2%";
    }
  }

  void handleRet(const Instruction& inst) {
    if (stack_size_) {
      getStackPointer();
      genInt(stack_size_);
      code_ += '-';
      setStackPointer();
    }

    auto func = dynamic_cast<const Function*>(inst.getParent()->getParent());
    assert(func);
    if (func->getName() == "main") {
      code_ += '@';
    } else {
      // TODO: handle SP
      assert(inst.getNumOperands() < 2);
      if (inst.getNumOperands()) {
        getLocal(inst.getOperand(0));
        code_ += '\\';
      } else {
        code_ += "0\\";
      }
    }
  }

  void handleCall(const CallInst& ci) {
    const Function* func = ci.getCalledFunction();
    assert(func);

    fprintf(stderr, "func: %s\n", func->getName().data());
    if (func->getName() == "putchar") {
      assert(ci.getNumArgOperands() == 1);
      getLocal(ci.getArgOperand(0));
      code_ += ",0";
    } else if (func->getName() == "getchar") {
      assert(ci.getNumArgOperands() == 0);
      code_ += "G";
    } else if (func->getName() == "calloc") {
      assert(ci.getNumArgOperands() == 2);
      assert(getConstInt(ci.getArgOperand(1)) == 4);
      getHeapPointer();
      code_ += ':';
      getLocal(ci.getArgOperand(0));
      code_ += "+";
      setHeapPointer();
    } else if (func->getName() == "free") {
      code_ += "0";
    } else if (func->getName() == "puts") {
      assert(ci.getNumArgOperands() == 1);
      ci.getArgOperand(0)->dump();
      auto ce = dynamic_cast<ConstantExpr*>(ci.getArgOperand(0));
      assert(ce);
      auto ge = dynamic_cast<GetElementPtrInst*>(ce->getAsInstruction());
      assert(ge);
      auto gv = dynamic_cast<GlobalVariable*>(ge->getOperand(0));
      assert(gv);
      for (size_t i = 0; gv->getInitializer()->getAggregateElement(i); i++) {
        int v = getConstInt(gv->getInitializer()->getAggregateElement(i));
        if (!v)
          break;
        genInt(v);
        code_ += ',';
      }
      code_ += "52*,0";
    } else if (func->getName() == "exit") {
      code_ += "@";
    } else {
      auto found = block_map_.find(&ci);
      assert(found != block_map_.end());
      int ret = found->second + 1;
      genInt(ret);
      for (int i = ci.getNumArgOperands() - 1; i >= 0; i--)
        getLocal(ci.getArgOperand(i));

      const auto& blocks = func->getBasicBlockList();
      assert(!blocks.empty());
      prepareBranch(NULL, &*blocks.begin());

      modifyLocalPointer(ci, '+');
      emitBlock(ci);
      modifyLocalPointer(ci, '-');
    }
  }

  void modifyLocalPointer(const CallInst& ci, char op) {
    getLocalPointer();
    auto func = dynamic_cast<const Function*>(ci.getParent()->getParent());
    assert(func);
    genInt(local_size_map_[func]);
    code_ += op;
    setLocalPointer();
  }

  void prepareBranch(const Instruction* src, const BasicBlock* block) {
    for (const Instruction& inst : block->getInstList()) {
      if (inst.getOpcode() != Instruction::PHI)
        break;

      auto& phi = static_cast<const PHINode&>(inst);
      assert(src);
      const BasicBlock* src_block = src->getParent();

      bool handled = false;
      for (size_t i = 0; i < phi.getNumIncomingValues(); i++) {
        if (src_block != phi.getIncomingBlock(i))
          continue;
        if (dynamic_cast<UndefValue*>(phi.getIncomingValue(i)))
          genInt(0);
        else
          getLocal(phi.getIncomingValue(i));
#if 0
        genInt(getInstId(phi));
        code_ += " .:. ";
#endif
        set(PHI, getInstId(phi));
        handled = true;
        break;
      }
      assert(handled);
    }

    auto found = block_map_.find(block);
    assert(found != block_map_.end());
    genInt(found->second);
  }

  void handleBr(const Instruction& inst) {
    if (inst.getNumOperands() == 3) {
      prepareBranch(&inst, static_cast<const BasicBlock*>(inst.getOperand(1)));
      prepareBranch(&inst, static_cast<const BasicBlock*>(inst.getOperand(2)));
      getLocal(inst.getOperand(0));

      code_ += 'S';
    } else if (inst.getNumOperands() == 1) {
      prepareBranch(&inst, static_cast<const BasicBlock*>(inst.getOperand(0)));
    }
  }

  void handleSwitch(const SwitchInst& si) {
    assert(si.getNumOperands() > 2);
    prepareBranch(&si, static_cast<const BasicBlock*>(si.getOperand(1)));

    for (SwitchInst::ConstCaseIt it = si.case_begin();
         it != si.case_end();
         ++it) {
      prepareBranch(&si, it.getCaseSuccessor());
      const IntegersSubset& ints = it.getCaseValueEx();
      assert(ints.isSingleNumber());
      assert(ints.isSingleNumbersOnly());
      genInt(getConstInt(ints.getSingleNumber(0).toConstantInt()));
      getLocal(si.getOperand(0));
      code_ += "-!S";
    }
  }

  void handleCmp(const ICmpInst& cmp) {
    switch (cmp.getPredicate()) {
      case CmpInst::ICMP_EQ:
        getLocal(cmp.getOperand(0));
        //code_ += "88*,:.";
        getLocal(cmp.getOperand(1));
        code_ += "-!";
        //code_ += "88*,:.";
        break;

      case CmpInst::ICMP_NE:
        getLocal(cmp.getOperand(0));
        getLocal(cmp.getOperand(1));
        code_ += "-!!";
        break;

      case CmpInst::ICMP_UGT:
      case CmpInst::ICMP_SGT:
        getLocal(cmp.getOperand(0));
        if (cmp.getPredicate() == CmpInst::ICMP_UGT)
          convertUnsigned();
        getLocal(cmp.getOperand(1));
        if (cmp.getPredicate() == CmpInst::ICMP_UGT)
          convertUnsigned();
        code_ += "`";
        break;

      case CmpInst::ICMP_ULT:
      case CmpInst::ICMP_SLT:
        getLocal(cmp.getOperand(1));
        if (cmp.getPredicate() == CmpInst::ICMP_ULT)
          convertUnsigned();
        getLocal(cmp.getOperand(0));
        if (cmp.getPredicate() == CmpInst::ICMP_ULT)
          convertUnsigned();
        code_ += "`";
        break;

      case CmpInst::ICMP_UGE:
      case CmpInst::ICMP_SGE:
        getLocal(cmp.getOperand(1));
        if (cmp.getPredicate() == CmpInst::ICMP_UGE)
          convertUnsigned();
        getLocal(cmp.getOperand(0));
        if (cmp.getPredicate() == CmpInst::ICMP_UGE)
          convertUnsigned();
        code_ += "`!";
        break;

      case CmpInst::ICMP_ULE:
      case CmpInst::ICMP_SLE:
        getLocal(cmp.getOperand(0));
        if (cmp.getPredicate() == CmpInst::ICMP_ULE)
          convertUnsigned();
        getLocal(cmp.getOperand(1));
        if (cmp.getPredicate() == CmpInst::ICMP_ULE)
          convertUnsigned();
        code_ += "`!";
        break;

      default:
        assert(false);
    }
  }

  void handleAlloca(const AllocaInst& alloca) {
    getStackPointer();
    alloca.getAllocatedType()->dump();
    code_ += ':';
    int size = getSizeOfType(alloca.getAllocatedType());
    stack_size_ += size;
    genInt(size);
    code_ += '+';
    setStackPointer();
  }

  void handleGetElementPtr(const GetElementPtrInst& gep) {
    assert(gep.isInBounds());
    assert(gep.getNumOperands() > 0);
    getLocal(gep.getOperand(0));
    //gep.getOperand(0)->dump();
    for (size_t i = 1; i < gep.getNumOperands(); i++) {
      getLocal(gep.getOperand(i));
      code_ += '+';
    }
    // TODO: check type
  }

  void genInt(int v) {
    char op = '+';
    if (v < 0) {
      v = -v;
      op = '-';
    }
    vector<int> c;
    do {
      c.push_back(v % 9);
      v /= 9;
    } while (v);

    if (op == '-')
      code_ += '0';
    for (size_t i = 0; i < c.size(); i++) {
      if (i != 0)
        code_ += "9*";
      char v = c[c.size() - i - 1];
      if (v || c.size() == 1) {
        code_ += (v + '0');
        if (i != 0 || op == '-')
          code_ += op;
      }
    }
  }

  void getLocal(const Value* v) {
    auto found = global_map_.find(v);
    if (found != global_map_.end()) {
      genInt(found->second);
    } else if (auto cv = dynamic_cast<const ConstantInt*>(v)) {
      int c = cv->getLimitedValue();
      genInt(c);
    } else if (dynamic_cast<const ConstantPointerNull*>(v)) {
      genInt(0);
    } else {
      int id = getInstId(*v);
      get(LOCAL, id);
    }
  }

  void make2D(MemType mt) {
    code_ += ":9%";
    if (mt == MEM)
      code_ += "9+";
    else if (mt == PHI)
      code_ += "9+9+";
    else
      assert(mt == LOCAL);
    code_ += "\\9/";
  }

  void getStackPointer() {
    code_ += "00g";
  }

  void setStackPointer() {
    code_ += "00p";
  }

  void getHeapPointer() {
    code_ += "10g";
  }

  void setHeapPointer() {
    code_ += "10p";
  }

  void getLocalPointer() {
    code_ += "20g";
  }

  void setLocalPointer() {
    code_ += "20p";
  }

  void addr(MemType mt, int id) {
    if (mt == LOCAL) {
      getLocalPointer();
      genInt(id);
      code_ += '+';
    } else {
      int addr = kLocalPos + id;
      genInt(addr);
    }
  }

  void get(MemType mt, int id) {
    addr(mt, id);
    make2D(mt);
    code_ += 'g';
  }

  void set(MemType mt, int id) {
    addr(mt, id);
    make2D(mt);
    code_ += "p";
  }

  void convertUnsigned() {
    code_ += "::88*::*:**+\\01-`!S";
  }

  Module* module_;
  string code_;
  map<const Value*, int> block_map_;
  map<const Value*, int> id_map_;
  map<const Value*, int> global_map_;
  map<const Function*, int> func_map_;
  map<const Function*, int> local_size_map_;
  vector<string> bef_;
  int stack_size_;
};

int main(int argc, char* argv[]) {
  const char* arg0 = argv[0];

  if (!strcmp(argv[1], "-g")) {
    is_debug = true;
    argc--;
    argv++;
  }

  if (argc < 1) {
    fprintf(stderr, "Usage: %s bitcode\n", arg0);
    return 1;
  }

  LLVMContext context;
  OwningPtr<MemoryBuffer> buf;

  if (error_code ec = MemoryBuffer::getFile(argv[1], buf)) {
    fprintf(stderr, "Failed to read %s: %s\n", argv[1], ec.message().c_str());
  }

  Module* module = ParseBitcodeFile(buf.get(), context);
  fprintf(stderr, "%p\n", module);

  B2B b2b(module);
  b2b.run();
  for (size_t i = 0; i < b2b.befunge().size(); i++) {
    printf("%s\n", b2b.befunge()[i].c_str());
  }
}
