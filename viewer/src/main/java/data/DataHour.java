package data;

import data.computation.ComputedVariable;
import data.computation.VariableComputation;

import java.io.Serializable;
import java.util.*;

public class DataHour implements Serializable {

    private transient DataManager dataManager;

    private final long hourNumber;

    private transient boolean modified = false;

    private transient long lastUse = System.currentTimeMillis();

    private final List<DataVariable> variablesRaw;
    private final Map<UUID, ComputedVariable> variablesComputed;
    private transient boolean computationNeeded;

    public DataHour(long hourNumber, DataManager dataManager){
        this.dataManager = dataManager;
        this.hourNumber = hourNumber;
        variablesRaw = new LinkedList<>();
        variablesComputed = new HashMap<>();
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

    public boolean log(int variableId, int samplesPerHour, int sampleNumber, double value) {
        lastUse = System.currentTimeMillis();

        DataVariable dataVariable = getVariable(variableId, samplesPerHour, true);
        boolean result = dataVariable.log(sampleNumber, value);
        if(result && !modified){
            modified = true;
        }

        if(result && !computationNeeded){
            computationNeeded = true;
        }

        return result;
    }

    public DataVariable getVariable(int variableId, int samplesPerHour, boolean create) {
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
            variablesComputed.put(uuid, result);
        }

        return result;
    }

    private ComputedVariable findComputedVariable(UUID uuid) {
        return variablesComputed.get(uuid);
    }

    public List<DataVariable> getVariablesRaw() {
        return variablesRaw;
    }

    public void runComputations(DataManager dataManager) {
        if(this.dataManager == null) {
            this.dataManager = dataManager;
        }
        for(VariableComputation calculation : dataManager.getVariableCalculations()) {
            variablesComputed.putIfAbsent(calculation.getUuid(), new ComputedVariable(calculation.getSamplesPerHour(), calculation.getUuid()));
            if(variablesComputed.get(calculation.getUuid()).runCalculation(dataManager, calculation, this)){
                this.modified = true;
            }
        }
    }

    public boolean isComputationNeeded() {
        return computationNeeded;
    }
}
