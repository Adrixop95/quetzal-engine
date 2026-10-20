import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
import java.nio.file.*;

/** args: addrlist.txt out.c ; decompiles (creating if missing) the functions at the listed hex addresses. */
public class DecompAddrs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        PrintWriter out = new PrintWriter(new FileWriter(a[1]));
        for (String s : Files.readAllLines(Paths.get(a[0]))) {
            s = s.trim(); if (s.isEmpty()) continue;
            Address ad = toAddr("0x" + s);
            Function f = getFunctionAt(ad);
            if (f == null) {
                disassemble(ad);
                f = createFunction(ad, null);
            }
            if (f == null) { out.println("// ==== " + s + " FAILED"); continue; }
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            if (r.decompileCompleted()) {
                out.println("// ==== " + s + " " + f.getName());
                out.println(r.getDecompiledFunction().getC());
            }
        }
        out.close();
    }
}
