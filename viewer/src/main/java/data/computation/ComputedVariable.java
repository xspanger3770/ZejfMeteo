package data.computation;

import data.DataHour;
import data.DataManager;

import java.io.Serializable;
import java.util.UUID;

public class ComputedVariable implements Serializable {

    private final int samplesPerHour;

    private final UUID uuid;

    private final ComputedLog[] computedLogs;

    public ComputedVariable(int samplesPerHour, UUID calculationUUID){
        this.samplesPerHour = samplesPerHour;
        this.uuid = calculationUUID;

        computedLogs = new ComputedLog[samplesPerHour];
        for(int i = 0; i < samplesPerHour; i++){
            computedLogs[i] = new ComputedLog();
        }
    }

    public UUID getUuid() {
        return uuid;
    }

    public ComputedLog[] getComputedLogs() {
        return computedLogs;
    }

    public int getSamplesPerHour() {
        return samplesPerHour;
    }

    public boolean runCalculation(DataManager dataManager, VariableComputation calculation, DataHour dataHour) {
        System.out.println("RUNCALC");
        return  calculation.calculate(dataManager, calculation, dataHour, this);
    }
}
