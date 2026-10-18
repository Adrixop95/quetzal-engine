import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs().length > 0 ? getScriptArgs()[0] : "/tmp";
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        PrintWriter all = new PrintWriter(new FileWriter(out + "/all.c"));
        PrintWriter idx = new PrintWriter(new FileWriter(out + "/functions.txt"));
        int n = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            idx.printf("%s %s %d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses());
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            if (r.decompileCompleted()) {
                all.println("// ==== " + f.getEntryPoint() + " " + f.getName());
                all.println(r.getDecompiledFunction().getC());
            }
            n++;
        }
        all.close(); idx.close();
        println("functions: " + n);
    }
}
