package data;

import data.computation.ComputedVariable;
import data.computation.VariableComputation;
import exception.RuntimeApplicationException;
import main.ZejfMeteo;

import java.io.IOException;
import java.io.Serializable;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class DataHour implements Serializable {

    private static final long VARIABLE_CHECK_INTERVAL = 1000 * 60 * 10;
    private static final long DATA_CHECK_INTERVAL = 1000 * 60 * 10;
    private transient DataManager dataManager;

    private final long hourNumber;

    private transient boolean modified = false;

    private transient long lastUse = System.currentTimeMillis();

    private transient long lastVariablesCheck = 0;

    private final Queue<DataVariable> variablesRaw;
    private final Map<UUID, ComputedVariable> variablesComputed;
    private transient boolean computationNeeded;

    public DataHour(long hourNumber, DataManager dataManager){
        this.dataManager = dataManager;
        this.hourNumber = hourNumber;
        variablesRaw = new ConcurrentLinkedQueue<>();
        variablesComputed = new ConcurrentHashMap<>();
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

        lastUse = System.currentTimeMillis();

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

    public ComputedVariable getComputedVariable(UUID uuid, int samplesPerHour, boolean create) {
        ComputedVariable result = findComputedVariable(uuid);
        if(result == null && create){
            result = new ComputedVariable(samplesPerHour, uuid);
            variablesComputed.put(uuid, result);
        }

        lastUse = System.currentTimeMillis();

        return result;
    }

    private ComputedVariable findComputedVariable(UUID uuid) {
        return variablesComputed.get(uuid);
    }

    public Queue<DataVariable> getVariablesRaw() {
        return variablesRaw;
    }

    public void runComputations(DataManager dataManager) {
        if(this.dataManager == null) {
            this.dataManager = dataManager;
        }
        for(VariableComputation calculation : dataManager.getVariableCalculations()) {
            variablesComputed.putIfAbsent(calculation.getUuid(), new ComputedVariable(calculation.getSamplesPerHour(), calculation.getUuid()));
            // todo remove unused
            if(variablesComputed.get(calculation.getUuid()).runCalculation(dataManager, calculation, this)){
                this.modified = true;
            }
        }

        runDataChecks();
    }

    public void runDataChecks(){
        if(!ZejfMeteo.getSocketManager().isSocketRunning() || ZejfMeteo.getSocketManager().getZejfCommunicator() == null){
            return;
        }
        try {
            if (System.currentTimeMillis() - lastVariablesCheck >= VARIABLE_CHECK_INTERVAL) {
                ZejfMeteo.getSocketManager().getZejfCommunicator().sendVariablesRequest(this);
                this.lastVariablesCheck = System.currentTimeMillis();
            }

            for(DataVariable dataVariable : variablesRaw){
                if (System.currentTimeMillis() - dataVariable.getLastDataCheck() >= DATA_CHECK_INTERVAL) {
                    ZejfMeteo.getSocketManager().getZejfCommunicator().sendDataCheck(dataVariable, hourNumber);
                }
            }
        }catch(IOException e){
            throw new RuntimeApplicationException("", e);
        }
    }

    public boolean isComputationNeeded() {
        return computationNeeded;
    }

    public void saved() {
        this.modified = false;
    }
}
