// Static recompiler: lifts every function of the open x86-32 program to C via Ghidra P-code.
// Usage (headless): -postScript LiftAll.java <outdir> [funcsPerFile]
// Output: <outdir>/gen_NNN.c, <outdir>/gen_funcs.h, <outdir>/gen_table.c, <outdir>/userops.txt
// Generated code targets runtime/aztec_rt.h (guest memory image + CPU register file).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.lang.*;

import java.io.*;
import java.util.*;

public class LiftAll extends GhidraScript {
    static final int FLAG_LO = 0x200, FLAG_HI = 0x220;

    Set<Long> funcEntries = new HashSet<>();
    Map<String, Integer> userops = new TreeMap<>();
    Map<String, Integer> opCounts = new TreeMap<>();

    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        int perFile = getScriptArgs().length > 1 ? Integer.parseInt(getScriptArgs()[1]) : 40;
        new File(out).mkdirs();
        File[] old = new File(out).listFiles((d, n) -> n.startsWith("gen_") && n.endsWith(".c"));
        if (old != null) for (File f : old) f.delete();

        discoverFunctions(getScriptArgs().length > 2 ? getScriptArgs()[2] : null);
        List<Function> funcs = new ArrayList<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isExternal() || f.isThunk() && f.getBody().getNumAddresses() == 0) continue;
            funcs.add(f);
            funcEntries.add(f.getEntryPoint().getOffset());
        }
        println("functions to lift: " + funcs.size());

        PrintWriter hdr = new PrintWriter(new FileWriter(out + "/gen_funcs.h"));
        hdr.println("#pragma once\n#include \"aztec_rt.h\"");
        for (Function f : funcs) hdr.printf("void f_%08x(CPU* c);%n", f.getEntryPoint().getOffset());
        hdr.println("extern const FuncEntry g_func_table[];\nextern const unsigned g_func_count;");
        hdr.close();

        PrintWriter tab = new PrintWriter(new FileWriter(out + "/gen_table.c"));
        tab.println("#include \"gen_funcs.h\"\nconst FuncEntry g_func_table[] = {");
        for (Function f : funcs) tab.printf("{0x%08x, f_%08x},%n", f.getEntryPoint().getOffset(), f.getEntryPoint().getOffset());
        tab.println("};\nconst unsigned g_func_count = " + funcs.size() + ";");
        tab.close();

        int fileNo = 0, inFile = 0;
        PrintWriter w = null;
        int ok = 0, failed = 0;
        for (Function f : funcs) {
            if (w == null || inFile >= perFile) {
                if (w != null) w.close();
                w = new PrintWriter(new FileWriter(String.format("%s/gen_%03d.c", out, fileNo++)));
                w.println("#include \"gen_funcs.h\"\n");
                inFile = 0;
            }
            try {
                emitFunction(w, f);
                ok++;
            } catch (Exception e) {
                failed++;
                println("FAILED " + f.getName() + ": " + e);
                w.printf("void f_%08x(CPU* c) { rt_unlifted(c, 0x%08x); }%n", f.getEntryPoint().getOffset(), f.getEntryPoint().getOffset());
            }
            inFile++;
        }
        if (w != null) w.close();

        PrintWriter u = new PrintWriter(new FileWriter(out + "/userops.txt"));
        for (Map.Entry<String, Integer> e : userops.entrySet()) u.println(e.getKey() + " " + e.getValue());
        u.close();
        println("lifted ok=" + ok + " failed=" + failed + " files=" + fileNo);
        println("userops: " + userops);
    }

    // Create functions for call targets Ghidra did not recognise, until a fixed point is reached.
    // extraFile: optional text file with one hex entry address per line (runtime-reported misses).
    void discoverFunctions(String extraFile) throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        AddressSetView exec = currentProgram.getMemory().getExecuteSet();
        List<Long> work = new ArrayList<>();
        if (extraFile != null && new File(extraFile).exists()) {
            for (String line : java.nio.file.Files.readAllLines(new File(extraFile).toPath())) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                work.add(Long.parseLong(line.replace("0x", ""), 16));
            }
        }
        // pointers to code stored in data (vtables, initialiser tables, callbacks)
        for (ghidra.program.model.mem.MemoryBlock blk : currentProgram.getMemory().getBlocks()) {
            if (!blk.isInitialized() || blk.isExecute()) continue;
            byte[] buf = new byte[(int) blk.getSize()];
            try { blk.getBytes(blk.getStart(), buf); } catch (Exception e) { continue; }
            for (int i = 0; i + 4 <= buf.length; i += 4) {
                long v = (buf[i] & 0xffL) | (buf[i + 1] & 0xffL) << 8 | (buf[i + 2] & 0xffL) << 16 | (buf[i + 3] & 0xffL) << 24;
                if (v < 0x401000L || v > 0x7fffffffL) continue;
                Address a = toAddr(v);
                if (a != null && exec.contains(a)) work.add(v);
            }
        }
        println("candidate code pointers from data: " + work.size());
        int created = 0;
        for (int pass = 0; pass < 20; pass++) {
            for (Instruction in : currentProgram.getListing().getInstructions(true)) {
                for (PcodeOp op : in.getPcode()) {
                    int oc = op.getOpcode();
                    if (oc == PcodeOp.CALL) {
                        Varnode d = op.getInput(0);
                        if (!d.getAddress().getAddressSpace().isConstantSpace()) work.add(d.getOffset());
                        continue;
                    }
                    // immediates that look like code addresses (callbacks passed as arguments)
                    for (int j = 0; j < op.getNumInputs(); j++) {
                        Varnode v = op.getInput(j);
                        if (v.isConstant() && v.getSize() == 4 && v.getOffset() >= 0x401000L && v.getOffset() < 0x500000L) {
                            Address ca = toAddr(v.getOffset());
                            // only already-disassembled orphan code qualifies (avoids data that looks like addresses)
                            if (ca != null && exec.contains(ca) && currentProgram.getListing().getInstructionAt(ca) != null
                                    && fm.getFunctionContaining(ca) == null) work.add(v.getOffset());
                        }
                    }
                }
            }
            int made = 0;
            for (long t : work) {
                Address a = toAddr(t);
                if (a == null || !exec.contains(a) || fm.getFunctionAt(a) != null) continue;
                if (currentProgram.getListing().getInstructionAt(a) == null) disassemble(a);
                if (currentProgram.getListing().getInstructionAt(a) == null) continue;
                try {
                    Function f = createFunction(a, null);
                    if (f != null) { made++; created++; }
                } catch (Exception e) { /* overlapping body etc. */ }
            }
            work.clear();
            println("discover pass " + pass + ": created " + made);
            if (made == 0) break;
        }
        println("functions created by discovery: " + created);
    }

    // ---------------------------------------------------------------- types
    static String U(int size) {
        switch (size) {
            case 1: return "uint8_t";
            case 2: return "uint16_t";
            case 4: return "uint32_t";
            case 8: return "uint64_t";
            case 10: return "f80";
            case 16: return "u128";
        }
        throw new RuntimeException("bad size " + size);
    }
    static String S(int size) {
        switch (size) {
            case 1: return "int8_t";
            case 2: return "int16_t";
            case 4: return "int32_t";
            case 8: return "int64_t";
        }
        throw new RuntimeException("bad signed size " + size);
    }
    static boolean isWide(int size) { return size > 8; }
    static boolean isFlag(Varnode v) {
        return v != null && v.getAddress().isRegisterAddress() && v.getSize() == 1
                && v.getOffset() >= FLAG_LO && v.getOffset() < FLAG_HI;
    }

    // ------------------------------------------------------------ operands
    Set<String> temps;

    String tempName(Varnode v) {
        String n = String.format("t%x_%d", v.getOffset(), v.getSize());
        temps.add(n + ":" + v.getSize());
        return n;
    }

    // Expression usable as lvalue or rvalue.
    String lv(Varnode v) {
        AddressSpace sp = v.getAddress().getAddressSpace();
        int size = v.getSize();
        if (sp.isRegisterSpace()) return String.format("(*(%s*)(c->r+0x%x))", U(size), v.getOffset());
        if (sp.isUniqueSpace()) return tempName(v);
        if (sp.isMemorySpace()) return String.format("(*(%s*)(g_mem+0x%x))", U(size), v.getOffset() & 0xffffffffL);
        throw new RuntimeException("lvalue in space " + sp.getName());
    }

    String rv(Varnode v) {
        if (v.isConstant()) {
            int size = v.getSize();
            if (isWide(size)) return "(" + U(size) + "){0}";
            long val = v.getOffset();
            if (size < 8) val &= (1L << (size * 8)) - 1;
            return String.format("((%s)0x%xULL)", U(size), val);
        }
        return lv(v);
    }

    // Pointer to a varnode's storage (for generic helpers); consts become compound literals.
    String ptr(Varnode v) {
        if (v.isConstant()) return "&(" + U(Math.max(v.getSize(), 1)) + "){" + rv(v) + "}";
        return "&" + lv(v);
    }

    // ----------------------------------------------------------- one function
    void emitFunction(PrintWriter w, Function f) throws Exception {
        long entry = f.getEntryPoint().getOffset();
        List<Instruction> insns = collectInstructions(f);
        if (insns.isEmpty()) throw new RuntimeException("no instructions");

        Map<Long, Integer> indexOf = new HashMap<>();
        for (int i = 0; i < insns.size(); i++) indexOf.put(insns.get(i).getAddress().getOffset(), i);

        // pcode per instruction
        List<PcodeOp[]> pcodes = new ArrayList<>();
        for (Instruction in : insns) pcodes.add(in.getPcode());

        // ---- flag liveness (instruction-level dataflow, then op-level skip masks)
        int n = insns.size();
        BitSet[] liveIn = new BitSet[n];
        BitSet[] liveOut = new BitSet[n];
        List<List<Integer>> succ = new ArrayList<>();
        boolean[] cutFlags = new boolean[n];
        boolean[] conservative = new boolean[n];
        for (int i = 0; i < n; i++) {
            liveIn[i] = new BitSet(); liveOut[i] = new BitSet();
            List<Integer> s = new ArrayList<>();
            boolean terminal = false;
            boolean fallsThrough = true;
            PcodeOp[] ops = pcodes.get(i);
            for (PcodeOp op : ops) {
                int oc = op.getOpcode();
                if (oc == PcodeOp.BRANCH || oc == PcodeOp.CBRANCH) {
                    Varnode d = op.getInput(0);
                    if (d.getAddress().getAddressSpace().isConstantSpace()) {
                        conservative[i] = true; // internal control flow
                    } else {
                        Integer t = indexOf.get(d.getOffset());
                        if (t != null) s.add(t);
                    }
                    if (oc == PcodeOp.BRANCH) fallsThrough = false;
                } else if (oc == PcodeOp.BRANCHIND) {
                    fallsThrough = false; cutFlags[i] = true;
                    for (Address a : insns.get(i).getFlows()) { Integer t = indexOf.get(a.getOffset()); if (t != null) s.add(t); }
                } else if (oc == PcodeOp.RETURN) {
                    fallsThrough = false; cutFlags[i] = true;
                } else if (oc == PcodeOp.CALL || oc == PcodeOp.CALLIND) {
                    cutFlags[i] = true;
                }
            }
            if (fallsThrough && i + 1 < n) s.add(i + 1);
            succ.add(s);
        }
        boolean changed = true;
        while (changed) {
            changed = false;
            for (int i = n - 1; i >= 0; i--) {
                BitSet out = new BitSet();
                if (!cutFlags[i]) for (int s : succ.get(i)) out.or(liveIn[s]);
                BitSet in = backward(pcodes.get(i), out, conservative[i], null);
                if (!out.equals(liveOut[i]) || !in.equals(liveIn[i])) { liveOut[i] = out; liveIn[i] = in; changed = true; }
            }
        }

        // ---- emit
        temps = new LinkedHashSet<>();
        StringWriter bodySw = new StringWriter();
        PrintWriter b = new PrintWriter(bodySw);
        for (int i = 0; i < n; i++) {
            Instruction in = insns.get(i);
            PcodeOp[] ops = pcodes.get(i);
            long ia = in.getAddress().getOffset();
            boolean[] skip = new boolean[ops.length];
            backward(ops, liveOut[i], conservative[i], skip);
            b.printf("L_%08x:; /* %s */%n", ia, in.toString().replace("*/", "* /"));
            for (int k = 0; k < ops.length; k++) {
                if (conservative[i]) b.printf("P_%08x_%d:;%n", ia, k);
                if (skip[k]) continue;
                emitOp(b, f, in, ops, k, indexOf);
            }
            if (conservative[i]) b.printf("P_%08x_%d:;%n", ia, ops.length);
            // implicit fallthrough
            if (!hasTerminal(ops) && in.getFallThrough() != null) {
                long ft = in.getFallThrough().getOffset();
                boolean nextIsFt = i + 1 < n && insns.get(i + 1).getAddress().getOffset() == ft;
                if (!nextIsFt) {
                    if (indexOf.containsKey(ft)) b.printf("  goto L_%08x;%n", ft);
                    else b.printf("  f_dispatch(c, 0x%08x); return;%n", ft);
                }
            }
        }
        b.flush();

        w.printf("void f_%08x(CPU* c) { /* %s */%n  RT_ENTER(0x%08x);%n", entry, f.getName(), entry);
        for (String t : temps) {
            String[] p = t.split(":");
            int sz = Integer.parseInt(p[1]);
            if (isWide(sz)) w.printf("  %s %s; memset(&%s, 0, sizeof %s);%n", U(sz), p[0], p[0], p[0]);
            else w.printf("  %s %s = 0;%n", U(sz), p[0]);
        }
        if (insns.get(0).getAddress().getOffset() != entry) w.printf("  goto L_%08x;  /* shared code lies below the entry */%n", entry);
        w.print(bodySw);
        w.println("}\n");
    }

    // Instructions reachable from the entry by intra-procedural flow (calls excluded). Code shared with
    // other functions (SEH handlers, shared tails) is duplicated; jumps to other function entries stay tail calls.
    List<Instruction> collectInstructions(Function f) {
        Listing listing = currentProgram.getListing();
        long entry = f.getEntryPoint().getOffset();
        Map<Long, Instruction> seen = new TreeMap<>();
        Deque<Address> work = new ArrayDeque<>();
        work.add(f.getEntryPoint());
        while (!work.isEmpty()) {
            Address a = work.poll();
            if (seen.containsKey(a.getOffset())) continue;
            if (a.getOffset() != entry && funcEntries.contains(a.getOffset())) continue;  // tail call target
            Instruction in = listing.getInstructionAt(a);
            if (in == null) { disassemble(a); in = listing.getInstructionAt(a); }
            if (in == null) continue;
            seen.put(a.getOffset(), in);
            boolean fall = true;
            for (PcodeOp op : in.getPcode()) {
                int oc = op.getOpcode();
                if (oc == PcodeOp.BRANCH || oc == PcodeOp.CBRANCH) {
                    Varnode d = op.getInput(0);
                    if (!d.getAddress().getAddressSpace().isConstantSpace()) work.add(toAddr(d.getOffset()));
                    if (oc == PcodeOp.BRANCH) fall = false;
                } else if (oc == PcodeOp.BRANCHIND) {
                    fall = false;
                    for (Address t : in.getFlows()) work.add(t);
                } else if (oc == PcodeOp.RETURN) {
                    fall = false;
                }
            }
            if (fall && in.getFallThrough() != null) work.add(in.getFallThrough());
        }
        return new ArrayList<>(seen.values());
    }

    boolean hasTerminal(PcodeOp[] ops) {
        for (PcodeOp op : ops) {
            int oc = op.getOpcode();
            if (oc == PcodeOp.BRANCH || oc == PcodeOp.BRANCHIND || oc == PcodeOp.RETURN) return true;
        }
        return false;
    }

    // Backward flag liveness over one instruction. If skip != null, marks dead flag stores.
    BitSet backward(PcodeOp[] ops, BitSet liveOut, boolean conservative, boolean[] skip) {
        BitSet live = (BitSet) liveOut.clone();
        if (conservative) {
            BitSet all = new BitSet();
            all.set(FLAG_LO, FLAG_HI);
            return all;
        }
        for (int k = ops.length - 1; k >= 0; k--) {
            PcodeOp op = ops[k];
            Varnode o = op.getOutput();
            if (o != null && isFlag(o)) {
                int off = (int) o.getOffset();
                // DF (0x20a) steers string instructions and is part of the ABI: never drop its writes
                if (!live.get(off) && skip != null && off != 0x20a) { skip[k] = true; continue; }
                live.clear(off);
            }
            for (int j = 0; j < op.getNumInputs(); j++) {
                Varnode v = op.getInput(j);
                if (v.getAddress().isRegisterAddress()) {
                    for (int q = 0; q < v.getSize(); q++) {
                        int off = (int) v.getOffset() + q;
                        if (off >= FLAG_LO && off < FLAG_HI) live.set(off);
                    }
                }
            }
        }
        return live;
    }

    // ---------------------------------------------------------------- p-code
    void emitOp(PrintWriter b, Function f, Instruction in, PcodeOp[] ops, int k, Map<Long, Integer> indexOf) {
        PcodeOp op = ops[k];
        int oc = op.getOpcode();
        Varnode o = op.getOutput();
        Varnode a = op.getNumInputs() > 0 ? op.getInput(0) : null;
        Varnode bb = op.getNumInputs() > 1 ? op.getInput(1) : null;
        long ia = in.getAddress().getOffset();
        String name = op.getMnemonic();
        opCounts.merge(name, 1, Integer::sum);
        String L = "  ";
        switch (oc) {
            case PcodeOp.COPY:
                if (isWide(o.getSize())) b.printf("%s%s = %s;%n", L, lv(o), a.isConstant() ? "(" + U(o.getSize()) + "){0}" : lv(a));
                else b.printf("%s%s = %s;%n", L, lv(o), rv(a));
                return;
            case PcodeOp.LOAD: {
                String addr = "(uint32_t)" + rv(op.getInput(1));
                if (isWide(o.getSize())) b.printf("%smemcpy(&%s, g_mem + %s, %d);%n", L, lv(o), addr, o.getSize());
                else b.printf("%s%s = *(%s*)(g_mem + %s);%n", L, lv(o), U(o.getSize()), addr);
                return;
            }
            case PcodeOp.STORE: {
                Varnode v = op.getInput(2);
                String addr = "(uint32_t)" + rv(op.getInput(1));
                if (isWide(v.getSize())) b.printf("%smemcpy(g_mem + %s, %s, %d);%n", L, addr, ptr(v), v.getSize());
                else b.printf("%s*(%s*)(g_mem + %s) = %s;%n", L, U(v.getSize()), addr, rv(v));
                return;
            }
            case PcodeOp.BRANCH: {
                if (a.getAddress().getAddressSpace().isConstantSpace()) {
                    b.printf("%sgoto P_%08x_%d;%n", L, ia, k + (int) a.getOffset());
                } else {
                    emitJump(b, a.getOffset(), indexOf);
                }
                return;
            }
            case PcodeOp.CBRANCH: {
                String cond = rv(bb) + " != 0";
                if (a.getAddress().getAddressSpace().isConstantSpace()) {
                    b.printf("%sif (%s) goto P_%08x_%d;%n", L, cond, ia, k + (int) a.getOffset());
                } else if (indexOf.containsKey(a.getOffset())) {
                    b.printf("%sif (%s) goto L_%08x;%n", L, cond, a.getOffset());
                } else {
                    b.printf("%sif (%s) { f_dispatch(c, 0x%08x); return; }%n", L, cond, a.getOffset());
                }
                return;
            }
            case PcodeOp.BRANCHIND: {
                b.printf("%sswitch ((uint32_t)%s) {%n", L, rv(a));
                Set<Long> seen = new HashSet<>();
                for (Address t : in.getFlows()) {
                    if (!indexOf.containsKey(t.getOffset()) || !seen.add(t.getOffset())) continue;
                    b.printf("%s case 0x%x: goto L_%08x;%n", L, t.getOffset(), t.getOffset());
                }
                b.printf("%s default: f_dispatch(c, (uint32_t)%s); return;%n%s}%n", L, rv(a), L);
                return;
            }
            case PcodeOp.CALL: {
                long t = a.getOffset();
                if (funcEntries.contains(t)) b.printf("%sf_%08x(c);%n", L, t);
                else b.printf("%sf_dispatch(c, 0x%08x);%n", L, t);
                return;
            }
            case PcodeOp.CALLIND:
                b.printf("%sf_dispatch(c, (uint32_t)%s);%n", L, rv(a));
                return;
            case PcodeOp.RETURN:
                b.printf("%sreturn;%n", L);
                return;
            case PcodeOp.CALLOTHER: {
                int idx = (int) a.getOffset();
                String uname = currentProgram.getLanguage().getUserDefinedOpName(idx);
                userops.merge(uname, 1, Integer::sum);
                StringBuilder args = new StringBuilder("c, 0");
                args.setLength(0);
                args.append("c");
                String outp = o != null ? ptr(o) : "NULL";
                int osz = o != null ? o.getSize() : 0;
                b.printf("%s{ /* userop %s */%n", L, uname);
                int ni = op.getNumInputs() - 1;
                for (int j = 0; j < ni; j++) {
                    Varnode v = op.getInput(j + 1);
                    b.printf("%s  %s ua%d = %s;%n", L, U(v.getSize()), j, isWide(v.getSize()) && v.isConstant() ? "(" + U(v.getSize()) + "){0}" : rv(v));
                }
                b.printf("%s  const void* uin[%d] = {", L, Math.max(ni, 1));
                for (int j = 0; j < ni; j++) b.printf("&ua%d,", j);
                if (ni == 0) b.print("0");
                b.printf("};%n");
                b.printf("%s  int usz[%d] = {", L, Math.max(ni, 1));
                for (int j = 0; j < ni; j++) b.printf("%d,", op.getInput(j + 1).getSize());
                if (ni == 0) b.print("0");
                b.printf("};%n");
                b.printf("%s  rt_userop(c, \"%s\", %s, %d, uin, usz, %d);%n%s}%n", L, uname, outp, osz, ni, L);
                return;
            }
            default:
        }
        // ---- arithmetic / logic
        String out = o != null ? lv(o) : null;
        int sz = o != null ? o.getSize() : 0;
        int asz = a != null ? a.getSize() : 0;
        String ra = a != null ? rv(a) : null;
        String rb = bb != null ? rv(bb) : null;
        String T = o != null ? U(sz) : null;
        switch (oc) {
            case PcodeOp.INT_ADD: b.printf("%s%s = (%s)(%s + %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_SUB: b.printf("%s%s = (%s)(%s - %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_MULT: b.printf("%s%s = (%s)(%s * %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_AND: b.printf("%s%s = (%s)(%s & %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_OR: b.printf("%s%s = (%s)(%s | %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_XOR: b.printf("%s%s = (%s)(%s ^ %s);%n", L, out, T, ra, rb); return;
            case PcodeOp.INT_NEGATE: b.printf("%s%s = (%s)(~%s);%n", L, out, T, ra); return;
            case PcodeOp.INT_2COMP: b.printf("%s%s = (%s)(0 - %s);%n", L, out, T, ra); return;
            case PcodeOp.INT_EQUAL: b.printf("%s%s = (%s == %s);%n", L, out, ra, rb); return;
            case PcodeOp.INT_NOTEQUAL: b.printf("%s%s = (%s != %s);%n", L, out, ra, rb); return;
            case PcodeOp.INT_LESS: b.printf("%s%s = (%s < %s);%n", L, out, ra, rb); return;
            case PcodeOp.INT_LESSEQUAL: b.printf("%s%s = (%s <= %s);%n", L, out, ra, rb); return;
            case PcodeOp.INT_SLESS: b.printf("%s%s = ((%s)%s < (%s)%s);%n", L, out, S(asz), ra, S(asz), rb); return;
            case PcodeOp.INT_SLESSEQUAL: b.printf("%s%s = ((%s)%s <= (%s)%s);%n", L, out, S(asz), ra, S(asz), rb); return;
            case PcodeOp.INT_ZEXT: b.printf("%s%s = (%s)%s;%n", L, out, T, ra); return;
            case PcodeOp.INT_SEXT: b.printf("%s%s = (%s)(%s)(%s)%s;%n", L, out, T, S(sz), S(asz), ra); return;
            case PcodeOp.INT_CARRY: b.printf("%s%s = ((%s)(%s + %s) < %s);%n", L, out, U(asz), ra, rb, ra); return;
            case PcodeOp.INT_SCARRY: b.printf("%s%s = (((%s)%s >= 0) == ((%s)%s >= 0)) && (((%s)(%s + %s) >= 0) != ((%s)%s >= 0));%n",
                    L, out, S(asz), ra, S(asz), rb, S(asz), ra, rb, S(asz), ra); return;
            case PcodeOp.INT_SBORROW: b.printf("%s%s = (((%s)%s >= 0) != ((%s)%s >= 0)) && (((%s)(%s - %s) >= 0) != ((%s)%s >= 0));%n",
                    L, out, S(asz), ra, S(asz), rb, S(asz), ra, rb, S(asz), ra); return;
            case PcodeOp.INT_LEFT: b.printf("%s%s = (%s)shl64((uint64_t)%s, (uint64_t)%s, %d);%n", L, out, T, ra, rb, sz * 8); return;
            case PcodeOp.INT_RIGHT: b.printf("%s%s = (%s)shr64((uint64_t)%s, (uint64_t)%s, %d);%n", L, out, T, ra, rb, asz * 8); return;
            case PcodeOp.INT_SRIGHT: b.printf("%s%s = (%s)sar64((int64_t)(%s)%s, (uint64_t)%s, %d);%n", L, out, T, S(asz), ra, rb, asz * 8); return;
            case PcodeOp.INT_DIV: b.printf("%s%s = (%s)(%s ? (%s / %s) : 0);%n", L, out, T, rb, ra, rb); return;
            case PcodeOp.INT_REM: b.printf("%s%s = (%s)(%s ? (%s %% %s) : 0);%n", L, out, T, rb, ra, rb); return;
            case PcodeOp.INT_SDIV: b.printf("%s%s = (%s)(%s ? ((%s)%s / (%s)%s) : 0);%n", L, out, T, rb, S(asz), ra, S(asz), rb); return;
            case PcodeOp.INT_SREM: b.printf("%s%s = (%s)(%s ? ((%s)%s %% (%s)%s) : 0);%n", L, out, T, rb, S(asz), ra, S(asz), rb); return;
            case PcodeOp.BOOL_NEGATE: b.printf("%s%s = (%s != 0) ? 0 : 1;%n", L, out, ra); return;
            case PcodeOp.BOOL_AND: b.printf("%s%s = (%s) & (%s);%n", L, out, ra, rb); return;
            case PcodeOp.BOOL_OR: b.printf("%s%s = (%s) | (%s);%n", L, out, ra, rb); return;
            case PcodeOp.BOOL_XOR: b.printf("%s%s = (%s) ^ (%s);%n", L, out, ra, rb); return;
            case PcodeOp.POPCOUNT: b.printf("%s%s = (%s)rt_popcount((uint64_t)%s);%n", L, out, T, ra); return;
            case PcodeOp.LZCOUNT: b.printf("%s%s = (%s)lzcnt64((uint64_t)%s, %d);%n", L, out, T, ra, asz * 8); return;
            case PcodeOp.SUBPIECE: {
                long sh = bb.getOffset() * 8;
                if (isWide(asz) || isWide(sz)) b.printf("%smemcpy(&%s, (uint8_t*)%s + %d, %d);%n", L, out, ptr(a), (int) bb.getOffset(), sz);
                else b.printf("%s%s = (%s)(%s >> %d);%n", L, out, T, ra, sh);
                return;
            }
            case PcodeOp.PIECE: {
                int bsz = bb.getSize();
                if (sz > 8) b.printf("%s{ memset(&%s, 0, %d); memcpy((uint8_t*)&%s, %s, %d); memcpy((uint8_t*)&%s + %d, %s, %d); }%n", L, out, sz, out, ptr(bb), bsz, out, bsz, ptr(a), asz);
                else b.printf("%s%s = (%s)(((%s)%s << %d) | (%s)%s);%n", L, out, T, T, ra, bsz * 8, T, rb);
                return;
            }
            case PcodeOp.CAST: b.printf("%s%s = %s;%n", L, out, ra); return;
            case PcodeOp.PTRADD: b.printf("%s%s = (%s)(%s + %s * %s);%n", L, out, T, ra, rb, rv(op.getInput(2))); return;
            case PcodeOp.PTRSUB: b.printf("%s%s = (%s)(%s + %s);%n", L, out, T, ra, rb); return;
            default:
        }
        // ---- floating point via generic helpers (operate on storage, any size 4/8/10)
        if (oc >= PcodeOp.FLOAT_EQUAL && oc <= PcodeOp.FLOAT_ROUND) {
            String m = name.toLowerCase();
            if (bb != null)
                b.printf("%s%s(%d, %s, %d, %s, %d, %s);%n", L, "rt_" + m, sz, ptr(o), asz, ptr(a), bb.getSize(), ptr(bb));
            else
                b.printf("%s%s(%d, %s, %d, %s);%n", L, "rt_" + m, sz, ptr(o), asz, ptr(a));
            return;
        }
        throw new RuntimeException("unhandled pcode op " + name + " at " + in.getAddress());
    }

    void emitJump(PrintWriter b, long target, Map<Long, Integer> indexOf) {
        if (indexOf.containsKey(target)) b.printf("  goto L_%08x;%n", target);
        else b.printf("  f_dispatch(c, 0x%08x); return;%n", target);
    }

    // P-code relative labels: emitted lazily — add after each op that is a branch target.
    // (handled by emitting labels for every op index; see labelEveryOp)
}
