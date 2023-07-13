package data;

import java.io.Serializable;
import java.util.LinkedList;
import java.util.List;
import java.util.UUID;

public class DataHour implements Serializable {

    private final long hourNumber;

    private transient boolean modified = false;

    private transient long lastUse = System.currentTimeMillis();

    private final List<DataVariable> variablesRaw;
    private final List<ComputedVariable> variablesComputed;

    public DataHour(long hourNumber){
        this.hourNumber = hourNumber;
        variablesRaw = new LinkedList<>();
        variablesComputed = new LinkedList<>();
    }

    public long getHourNumber() {
        return hourNumber;
    }

    public boolean isModified() {
        return modified;
    }

    public long getLastUse() {
        return lastUse;
    }

    public void log(int variableId, int samplesPerHour, int sampleNumber, double value) {
        lastUse = System.currentTimeMillis();

        DataVariable dataVariable = getVariable(variableId, samplesPerHour, true);
        if(dataVariable.log(sampleNumber, value) && !modified){
            modified = true;
        }
    }

    private DataVariable getVariable(int variableId, int samplesPerHour, boolean create) {
        DataVariable result = findVariable(variableId, samplesPerHour);
        if(result == null && create){
            result = new DataVariable(variableId, samplesPerHour);
            variablesRaw.add(result);
        }

        return result;
    }

    private DataVariable findVariable(int variableId, int samplesPerHour) {
        for(DataVariable dataVariable:variablesRaw){
            if(dataVariable.getId() == variableId && dataVariable.getSamplesPerHour() == samplesPerHour){
                return dataVariable;
            }
        }
        return null;
    }

    private ComputedVariable getComputedVariable(UUID uuid, int samplesPerHour, boolean create) {
        ComputedVariable result = findComputedVariable(uuid);
        if(result == null && create){
            result = new ComputedVariable(samplesPerHour, uuid);
            variablesComputed.add(result);
        }

        return result;
    }

    private ComputedVariable findComputedVariable(UUID uuid) {
        for(ComputedVariable computedVariable:variablesComputed){
            if(computedVariable.getUuid() == uuid){
                return computedVariable;
            }
        }
        return null;
    }

    public List<DataVariable> getVariablesRaw() {
        return variablesRaw;
    }
}
