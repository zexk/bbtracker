// Ghidra headless script: print references and containing functions for addresses.
// @category bbtracker

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.nio.charset.StandardCharsets;

public class GhidraStringRefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        for (String value : getScriptArgs()) {
            Address address;
            if (value.startsWith("text:")) {
                byte[] bytes = value.substring(5).getBytes(StandardCharsets.US_ASCII);
                address = currentProgram.getMemory().findBytes(
                    currentProgram.getMinAddress(), currentProgram.getMaxAddress(),
                    bytes, null, true, monitor);
            } else {
                address = toAddr(value);
            }
            println("TARGET " + address);
            if (address == null)
                continue;
            for (Reference reference : getReferencesTo(address)) {
                Address source = reference.getFromAddress();
                Function function = getFunctionContaining(source);
                println("REF " + source + " " + function);
            }
        }
    }
}
