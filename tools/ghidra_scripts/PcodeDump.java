import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.address.*;

public class PcodeDump extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address a = toAddr(getScriptArgs()[0]);
        int max = getScriptArgs().length > 1 ? Integer.parseInt(getScriptArgs()[1]) : 40;
        Function f = getFunctionAt(a);
        InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
        int n = 0;
        while (it.hasNext() && n++ < max) {
            Instruction i = it.next();
            println(i.getAddress() + "  " + i);
            for (PcodeOp op : i.getPcode()) println("      " + op);
        }
    }
}
